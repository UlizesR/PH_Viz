#include "Model.h"
#include "SpatialIndex.hpp"
#include "Graphics/Utils.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glad/glad.h>
#include <limits>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <cstddef>  // For offsetof
#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>  // For normalize
#include <cmath>
#include <cstring>  // For memcpy
#include <thread>
#include <mutex>
#include <atomic>

using Graphics::Half::floatToHalf;

namespace Graphics {

static Vertex makeVertex(const aiMesh* mesh, unsigned int i) {
	Vertex v{};
	if (mesh->HasPositions()) {
		v.position.x = mesh->mVertices[i].x;
		v.position.y = mesh->mVertices[i].y;
		v.position.z = mesh->mVertices[i].z;
	}
	if (mesh->HasNormals()) {
		v.normal.x = mesh->mNormals[i].x;
		v.normal.y = mesh->mNormals[i].y;
		v.normal.z = mesh->mNormals[i].z;
	}
	if (mesh->HasTextureCoords(0)) {
		v.texcoord.x = mesh->mTextureCoords[0][i].x;
		v.texcoord.y = mesh->mTextureCoords[0][i].y;
	}
	// Load vertex colors from PLY RGB (uses color set 0)
	if (mesh->HasVertexColors(0)) {
		v.color.r = mesh->mColors[0][i].r;
		v.color.g = mesh->mColors[0][i].g;
		v.color.b = mesh->mColors[0][i].b;
	}
	return v;
}

// Loads models from various formats supported by Assimp, including:
// - .obj, .ply, .off (polygon formats - automatically triangulated)
// - .stl, .fbx, .dae, .3ds, and many others
// - Point clouds: files with vertices but no faces (detected automatically)
bool Model::loadFromFile(const std::string& path, std::string& outError) {
	mMeshes.clear();
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(
		path,
		aiProcess_Triangulate |  // Required for .ply/.off polygon formats (ignored for point clouds)
		aiProcess_GenNormals |
		aiProcess_JoinIdenticalVertices |
		aiProcess_ImproveCacheLocality |
		aiProcess_SortByPType |
		aiProcess_OptimizeMeshes);
		// Note: Removed aiProcess_ValidateDataStructure to allow point clouds (meshes with no faces)

	if (!scene || !scene->HasMeshes()) {
		outError = importer.GetErrorString();
		return false;
	}

	mMin = glm::vec3( std::numeric_limits<float>::max());
	mMax = glm::vec3(-std::numeric_limits<float>::max());
	mScalarMin =  std::numeric_limits<float>::max();
	mScalarMax = -std::numeric_limits<float>::max();

	mMeshes.resize(scene->mNumMeshes);
	
	// Multi-threaded mesh processing: process multiple meshes in parallel
	// Only use threading if we have multiple meshes or large meshes (>= 10k vertices)
	unsigned int totalVertices = 0;
	for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
		totalVertices += scene->mMeshes[m]->mNumVertices;
	}
	
    bool useThreading = (totalVertices >= Graphics::Config::MinVerticesForThreading || scene->mNumMeshes >= Graphics::Config::MinMeshesForThreading);
	
	// Thread-safe accumulators for AABB and scalar range
	std::mutex mutex;
	
	// Function to process a single mesh
	auto processMesh = [&](unsigned int meshIndex) {
		const aiMesh* aimesh = scene->mMeshes[meshIndex];
		Mesh mesh;
		mesh.vertices.reserve(aimesh->mNumVertices);
		
		// Per-mesh AABB and scalar range (for thread safety)
		glm::vec3 meshMin(std::numeric_limits<float>::max());
		glm::vec3 meshMax(-std::numeric_limits<float>::max());
		float meshScalarMin = std::numeric_limits<float>::max();
		float meshScalarMax = -std::numeric_limits<float>::max();
		
		// Process vertices
		for (unsigned int i = 0; i < aimesh->mNumVertices; ++i) {
			Vertex v = makeVertex(aimesh, i);
			mesh.vertices.push_back(v);
			
			// Update per-mesh bounds
			meshMin.x = std::min(meshMin.x, v.position.x);
			meshMin.y = std::min(meshMin.y, v.position.y);
			meshMin.z = std::min(meshMin.z, v.position.z);
			meshMax.x = std::max(meshMax.x, v.position.x);
			meshMax.y = std::max(meshMax.y, v.position.y);
			meshMax.z = std::max(meshMax.z, v.position.z);
			meshScalarMin = std::min(meshScalarMin, v.scalar);
			meshScalarMax = std::max(meshScalarMax, v.scalar);
		}
		
		// Check if this is a point cloud (no faces)
		mesh.isPointCloud = (aimesh->mNumFaces == 0);
		
		if (!mesh.isPointCloud) {
			// Regular mesh with faces
			mesh.indices.reserve(aimesh->mNumFaces * 3);
			for (unsigned int f = 0; f < aimesh->mNumFaces; ++f) {
				const aiFace& face = aimesh->mFaces[f];
				for (unsigned int j = 0; j < face.mNumIndices; ++j) {
					mesh.indices.push_back(face.mIndices[j]);
				}
			}
			mesh.indexCount = static_cast<unsigned int>(mesh.indices.size());
		} else {
			// Point cloud - no indices needed
			mesh.indexCount = 0;
		}
		
		mesh.vertexCount = static_cast<unsigned int>(mesh.vertices.size());
		
		// Thread-safe merge of bounds
		{
			std::lock_guard<std::mutex> lock(mutex);
			mMeshes[meshIndex] = std::move(mesh);
			mMin.x = std::min(mMin.x, meshMin.x);
			mMin.y = std::min(mMin.y, meshMin.y);
			mMin.z = std::min(mMin.z, meshMin.z);
			mMax.x = std::max(mMax.x, meshMax.x);
			mMax.y = std::max(mMax.y, meshMax.y);
			mMax.z = std::max(mMax.z, meshMax.z);
			mScalarMin = std::min(mScalarMin, meshScalarMin);
			mScalarMax = std::max(mScalarMax, meshScalarMax);
		}
	};
	
	if (useThreading && scene->mNumMeshes > 1) {
		// Multi-threaded processing: use hardware concurrency
		unsigned int numThreads = std::min(static_cast<unsigned int>(std::thread::hardware_concurrency()), scene->mNumMeshes);
		numThreads = std::max(numThreads, 1u);  // At least 1 thread
		
		std::vector<std::thread> threads;
		threads.reserve(numThreads);
		
		// Distribute meshes across threads
		for (unsigned int t = 0; t < numThreads; ++t) {
			threads.emplace_back([&, t, numThreads]() {
				for (unsigned int m = t; m < scene->mNumMeshes; m += numThreads) {
					processMesh(m);
				}
			});
		}
		
		// Wait for all threads to complete
		for (auto& thread : threads) {
			thread.join();
		}
	} else {
		// Single-threaded processing (small models or single mesh)
		for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
			processMesh(m);
		}
	}
	
	// Normalize scalar range if needed (handle case where all scalars are same value)
	if (mScalarMax <= mScalarMin) {
		mScalarMin = 0.0f;
		mScalarMax = 1.0f;
	}
	
	// Build spatial index (octree) for point clouds with many points
	// Only build if point cloud has >= 100k points (spatial indexing is most beneficial for large clouds)
	if (isPointCloud() && !mMeshes.empty()) {
		unsigned int totalPoints = 0;
		for (const auto& mesh : mMeshes) {
			totalPoints += mesh.vertexCount;
		}
		
        if (totalPoints >= Graphics::Config::PointCloudMinPointsForOctree) {  // Build octree for point clouds over threshold
			std::vector<Octree::Point> points;
			points.reserve(totalPoints);
			
			// Collect all points from all meshes
			unsigned int globalIndex = 0;
			for (const auto& mesh : mMeshes) {
				for (unsigned int i = 0; i < mesh.vertexCount; ++i) {
					Octree::Point p;
					p.position = mesh.vertices[i].position;
					p.index = globalIndex++;  // Global index across all meshes
					points.push_back(p);
				}
			}
			
            // Build octree with configured parameters
            mSpatialIndex.build(points, mMin, mMax, Graphics::Config::OctreePointsPerNode, Graphics::Config::OctreeMaxDepth);
		}
	}
	
	return true;
}

void Model::uploadToGPU(bool dropCpu) {
	for (Mesh& mesh : mMeshes) {
		if (mesh.vao.valid()) continue;
		mesh.vao.create();
		mesh.vbo.create();
		if (!mesh.isPointCloud) {
			mesh.ebo.create();
		}

		mesh.vao.bind();
		mesh.vbo.bind(GL_ARRAY_BUFFER);
		
        // Vertex buffer optimization: use half-floats for positions and UVs to reduce memory bandwidth
		// This reduces vertex size from 48 bytes to ~38 bytes (21% reduction) for large models
		// Trade-off: slight precision loss (usually acceptable for positions/UVs)
		// Note: Disabled for point clouds as precision is more critical and they're typically smaller
        mesh.usesOptimizedVertices = (!mesh.isPointCloud && mesh.vertexCount >= Graphics::Config::VertexOptimizationMinVerts);
		
		if (mesh.usesOptimizedVertices) {
			// Pack vertices with half-floats for positions and UVs
			// Layout: pos (6 bytes, half-float vec3), normal (12 bytes, float vec3), 
			//         uv (4 bytes, half-float vec2), color (12 bytes, float vec3), scalar (4 bytes, float)
			// Total: 38 bytes per vertex (vs 48 bytes)
			
            // Use shared optimized vertex format
            using Graphics::OptimizedVertex;
			// Calculate actual size with compiler padding (typically 40-44 bytes due to alignment)
			
			std::vector<OptimizedVertex> optimizedVertices;
			optimizedVertices.reserve(mesh.vertexCount);
			
			for (const Vertex& v : mesh.vertices) {
				OptimizedVertex ov;
				ov.pos[0] = floatToHalf(v.position.x);
				ov.pos[1] = floatToHalf(v.position.y);
				ov.pos[2] = floatToHalf(v.position.z);
				ov.normal[0] = v.normal.x;
				ov.normal[1] = v.normal.y;
				ov.normal[2] = v.normal.z;
				ov.uv[0] = floatToHalf(v.texcoord.x);
				ov.uv[1] = floatToHalf(v.texcoord.y);
				ov.color[0] = v.color.x;
				ov.color[1] = v.color.y;
				ov.color[2] = v.color.z;
				ov.scalar = v.scalar;
				optimizedVertices.push_back(ov);
			}
			
			mesh.vbo.setData(GL_ARRAY_BUFFER, (GLsizeiptr)(mesh.vertexCount * sizeof(OptimizedVertex)), optimizedVertices.data(), GL_STATIC_DRAW);
		} else {
			// Use standard full-float vertex format
			mesh.vbo.setData(GL_ARRAY_BUFFER, (GLsizeiptr)(mesh.vertexCount * sizeof(Vertex)), mesh.vertices.data(), GL_STATIC_DRAW);
		}

		if (!mesh.isPointCloud && mesh.indexCount > 0) {
			mesh.ebo.bind(GL_ELEMENT_ARRAY_BUFFER);
			
			// Index buffer optimization: use 16-bit indices if vertex count < 65k
			// This reduces index buffer memory by 50% and improves cache performance
			mesh.uses16BitIndices = (mesh.vertexCount < 65536);
			
			if (mesh.uses16BitIndices) {
				// Convert indices to uint16_t
				std::vector<uint16_t> indices16;
				indices16.reserve(mesh.indexCount);
				
				// Validate all indices fit in 16-bit range
				bool allFit = true;
				for (unsigned int idx : mesh.indices) {
					if (idx >= 65536) {
						allFit = false;
						break;
					}
				}
				
				if (allFit) {
					// All indices fit, convert to 16-bit
					for (unsigned int idx : mesh.indices) {
						indices16.push_back(static_cast<uint16_t>(idx));
					}
					// Upload as 16-bit indices (50% memory savings)
					mesh.ebo.setData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(mesh.indexCount * sizeof(uint16_t)), indices16.data(), GL_STATIC_DRAW);
				} else {
					// Some indices exceed 16-bit range, fall back to 32-bit
					mesh.uses16BitIndices = false;
					mesh.ebo.setData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(mesh.indexCount * sizeof(unsigned int)), mesh.indices.data(), GL_STATIC_DRAW);
				}
			} else {
				// Use 32-bit indices (vertex count >= 65k or already decided)
				mesh.ebo.setData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(mesh.indexCount * sizeof(unsigned int)), mesh.indices.data(), GL_STATIC_DRAW);
			}
		}

		// layout: 0=pos, 1=normal, 2=uv, 3=color, 4=scalar
		if (mesh.usesOptimizedVertices) {
            // Optimized vertex format: half-floats for positions and UVs
            using Graphics::OptimizedVertex;
			
			const size_t vertexStride = sizeof(OptimizedVertex);
			
			// Position (half-float vec3)
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_HALF_FLOAT, GL_FALSE, static_cast<GLsizei>(vertexStride), (void*)offsetof(OptimizedVertex, pos));
			
			// Normal (float vec3)
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(vertexStride), (void*)offsetof(OptimizedVertex, normal));
			
			// UV (half-float vec2)
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_HALF_FLOAT, GL_FALSE, static_cast<GLsizei>(vertexStride), (void*)offsetof(OptimizedVertex, uv));
			
			// Color (float vec3)
			glEnableVertexAttribArray(3);
			glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(vertexStride), (void*)offsetof(OptimizedVertex, color));
			
			// Scalar (float)
			glEnableVertexAttribArray(4);
			glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(vertexStride), (void*)offsetof(OptimizedVertex, scalar));
		} else {
			// Standard vertex format: all floats
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));
			glEnableVertexAttribArray(3);
			glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
			glEnableVertexAttribArray(4);
			glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, scalar));
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	glBindVertexArray(0);

	if (dropCpu) {
		for (Mesh& mesh : mMeshes) {
			mesh.vertices.clear(); mesh.vertices.shrink_to_fit();
			mesh.indices.clear(); mesh.indices.shrink_to_fit();
		}
	}
}

void Model::draw() const {
	static bool warned = false;
	for (const Mesh& mesh : mMeshes) {
		if (!mesh.vao.valid()) {
			if (!warned) { std::cerr << "Model draw: encountered mesh with no VAO (skip)\n"; warned = true; }
			continue;
		}
		glBindVertexArray(mesh.vao.id());
		if (mesh.isPointCloud) {
			// Point cloud - use GL_POINTS
			glDrawArrays(GL_POINTS, 0, (GLsizei)mesh.vertexCount);
		} else {
			// Regular mesh - use indexed triangles
			if (mesh.indexCount == 0) continue;
			// Use 16-bit or 32-bit indices based on optimization
			GLenum indexType = mesh.uses16BitIndices ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
			glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indexCount, indexType, 0);
		}
	}
	glBindVertexArray(0);
}

void Model::drawPoints(float pointSize) const {
	static float sLastPointSize = 0.0f;
	if (pointSize != sLastPointSize) {
		glPointSize(pointSize);
		sLastPointSize = pointSize;
	}
	for (const Mesh& mesh : mMeshes) {
		if (!mesh.vao.valid() || mesh.vertexCount == 0) continue;
		glBindVertexArray(mesh.vao.id());
		glDrawArrays(GL_POINTS, 0, (GLsizei)mesh.vertexCount);
	}
	glBindVertexArray(0);
}

void Model::drawPointsSubset(const std::vector<unsigned int>& indices, float pointSize) const {
	if (indices.empty()) return;
	
	static float sLastPointSize = 0.0f;
	if (pointSize != sLastPointSize) {
		glPointSize(pointSize);
		sLastPointSize = pointSize;
	}
	
	// For spatial indexing, we need to render only a subset of points
	// We'll use glDrawElements with a temporary index buffer or use instanced rendering
	// For now, we'll create a temporary index buffer for the visible points
	// Note: This is a simple implementation - for better performance, we could cache index buffers
	
	// Group indices by mesh (for multi-mesh models)
	// For simplicity, assume all points are in the first mesh for point clouds
	if (!mMeshes.empty() && mMeshes[0].isPointCloud && mMeshes[0].vao.valid()) {
		const Mesh& mesh = mMeshes[0];
		
		// Create temporary index buffer for visible points
		GLuint tempEBO = 0;
		glGenBuffers(1, &tempEBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tempEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
		             static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
		             indices.data(), GL_DYNAMIC_DRAW);
		
		// Use 16-bit or 32-bit indices based on max index
		unsigned int maxIdx = 0;
		for (unsigned int idx : indices) {
			maxIdx = std::max(maxIdx, idx);
		}
		GLenum indexType = (maxIdx < 65536) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		
		// If indices fit in 16-bit, convert
		if (indexType == GL_UNSIGNED_SHORT) {
			std::vector<uint16_t> indices16;
			indices16.reserve(indices.size());
			for (unsigned int idx : indices) {
				indices16.push_back(static_cast<uint16_t>(idx));
			}
			glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			             static_cast<GLsizeiptr>(indices16.size() * sizeof(uint16_t)),
			             indices16.data(), GL_DYNAMIC_DRAW);
		}
		
		glBindVertexArray(mesh.vao.id());
		glDrawElements(GL_POINTS, static_cast<GLsizei>(indices.size()), indexType, 0);
		glBindVertexArray(0);
		
		// Clean up temporary buffer
		glDeleteBuffers(1, &tempEBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}

void Model::drawSphereImpostors(float pointSize) const {
	// Sphere impostors: expand points into billboard quads via geometry shader
	// For now, use GL_POINTS (geometry shader will be added in shader)
	// The shader needs to handle the sphere impostor rendering
	static float sLastPointSize = 0.0f;
	if (pointSize != sLastPointSize) {
		glPointSize(pointSize);
		sLastPointSize = pointSize;
	}
	for (const Mesh& mesh : mMeshes) {
		if (!mesh.vao.valid() || mesh.vertexCount == 0) continue;
		glBindVertexArray(mesh.vao.id());
		glDrawArrays(GL_POINTS, 0, (GLsizei)mesh.vertexCount);
	}
	glBindVertexArray(0);
}

void Model::generateSphereMesh(unsigned int subdivisions) const {
	if (mSphereMesh.initialized) return;
	
	// Generate icosphere
	// Start with icosahedron vertices
	const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;  // Golden ratio
	
	std::vector<glm::vec3> vertices;
	std::vector<unsigned int> indices;
	
	// Icosahedron vertices (normalized)
	vertices = {
		glm::normalize(glm::vec3(-1,  t,  0)),
		glm::normalize(glm::vec3( 1,  t,  0)),
		glm::normalize(glm::vec3(-1, -t,  0)),
		glm::normalize(glm::vec3( 1, -t,  0)),
		glm::normalize(glm::vec3( 0, -1,  t)),
		glm::normalize(glm::vec3( 0,  1,  t)),
		glm::normalize(glm::vec3( 0, -1, -t)),
		glm::normalize(glm::vec3( 0,  1, -t)),
		glm::normalize(glm::vec3( t,  0, -1)),
		glm::normalize(glm::vec3( t,  0,  1)),
		glm::normalize(glm::vec3(-t,  0, -1)),
		glm::normalize(glm::vec3(-t,  0,  1))
	};
	
	// Icosahedron faces (20 triangles)
	indices = {
		0, 11, 5,  0, 5, 1,  0, 1, 7,  0, 7, 10,  0, 10, 11,
		1, 5, 9,  5, 11, 4,  11, 10, 2,  10, 7, 6,  7, 1, 8,
		3, 9, 4,  3, 4, 2,  3, 2, 6,  3, 6, 8,  3, 8, 9,
		4, 9, 5,  2, 4, 11,  6, 2, 10,  8, 6, 7,  9, 8, 1
	};
	
	// Subdivide for smoother sphere (simple midpoint subdivision)
	for (unsigned int sub = 0; sub < subdivisions; ++sub) {
		std::vector<unsigned int> newIndices;
		std::unordered_map<unsigned long long, unsigned int> edgeToVertex;
		
		// Helper to get/create midpoint vertex for edge
		auto getMidpoint = [&](unsigned int a, unsigned int b) -> unsigned int {
			unsigned long long key = (static_cast<unsigned long long>(std::min(a, b)) << 32) | std::max(a, b);
			auto it = edgeToVertex.find(key);
			if (it != edgeToVertex.end()) return it->second;
			
			unsigned int newIdx = static_cast<unsigned int>(vertices.size());
			vertices.push_back(glm::normalize((vertices[a] + vertices[b]) * 0.5f));
			edgeToVertex[key] = newIdx;
			return newIdx;
		};
		
		// Subdivide each triangle into 4
		for (size_t i = 0; i < indices.size(); i += 3) {
			unsigned int v0 = indices[i];
			unsigned int v1 = indices[i + 1];
			unsigned int v2 = indices[i + 2];
			
			unsigned int m01 = getMidpoint(v0, v1);
			unsigned int m12 = getMidpoint(v1, v2);
			unsigned int m20 = getMidpoint(v2, v0);
			
			// Add 4 triangles
			newIndices.insert(newIndices.end(), {v0, m01, m20});
			newIndices.insert(newIndices.end(), {v1, m12, m01});
			newIndices.insert(newIndices.end(), {v2, m20, m12});
			newIndices.insert(newIndices.end(), {m01, m12, m20});
		}
		indices = newIndices;
	}
	
	// Build Vertex array for sphere mesh
	std::vector<Vertex> sphereVertices;
	sphereVertices.reserve(vertices.size());
	for (const glm::vec3& pos : vertices) {
		Vertex v;
		v.position = pos;
		v.normal = pos;  // Normal equals position for unit sphere
		v.texcoord = glm::vec2(0.0f);  // Not used for instanced spheres
		v.color = glm::vec3(1.0f);
		v.scalar = 0.0f;
		sphereVertices.push_back(v);
	}
	
	// Upload to GPU
	mSphereMesh.vao.create();
	mSphereMesh.vbo.create();
	mSphereMesh.ebo.create();
	
	mSphereMesh.vao.bind();
	mSphereMesh.vbo.bind(GL_ARRAY_BUFFER);
	mSphereMesh.vbo.setData(GL_ARRAY_BUFFER, (GLsizeiptr)(sphereVertices.size() * sizeof(Vertex)), sphereVertices.data(), GL_STATIC_DRAW);
	
	// Index buffer optimization: use 16-bit indices if vertex count < 65k
	unsigned int sphereVertexCount = static_cast<unsigned int>(sphereVertices.size());
	mSphereMesh.uses16BitIndices = (sphereVertexCount < 65536);
	
	if (mSphereMesh.uses16BitIndices) {
		// Convert indices to uint16_t
		std::vector<uint16_t> indices16;
		indices16.reserve(indices.size());
		
		bool allFit = true;
		for (unsigned int idx : indices) {
			if (idx >= 65536) {
				allFit = false;
				break;
			}
		}
		
		if (allFit) {
			for (unsigned int idx : indices) {
				indices16.push_back(static_cast<uint16_t>(idx));
			}
			mSphereMesh.ebo.bind(GL_ELEMENT_ARRAY_BUFFER);
			mSphereMesh.ebo.setData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(uint16_t)), indices16.data(), GL_STATIC_DRAW);
		} else {
			mSphereMesh.uses16BitIndices = false;
			mSphereMesh.ebo.bind(GL_ELEMENT_ARRAY_BUFFER);
			mSphereMesh.ebo.setData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);
		}
	} else {
		mSphereMesh.ebo.bind(GL_ELEMENT_ARRAY_BUFFER);
		mSphereMesh.ebo.setData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);
	}
	
	// Vertex attributes (same layout as regular mesh)
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, scalar));
	
	// Instance attribute: position offset (layout 5)
	// This will be set up when rendering
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	mSphereMesh.vao.bind();
	
	mSphereMesh.indexCount = static_cast<unsigned int>(indices.size());
	mSphereMesh.initialized = true;
}

void Model::drawInstancedSpheres(float /* radius */) const {
	// Generate sphere mesh if not already created
	if (!mSphereMesh.initialized) {
		generateSphereMesh(2);  // 2 subdivisions = ~320 triangles per sphere
	}
	
	if (!mSphereMesh.vao.valid()) return;
	
	// Set up instanced rendering
	// For each point cloud mesh, render instanced spheres
	for (const Mesh& mesh : mMeshes) {
		if (!mesh.isPointCloud || mesh.vertexCount == 0) continue;
		
		// Bind sphere mesh VAO
		mSphereMesh.vao.bind();
		
		// Set up instance data (positions from point cloud)
		// Use the point cloud's VBO as instance attribute
		glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo.id());
		
		// Instance attribute: position (layout 5)
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		glVertexAttribDivisor(5, 1);  // One per instance
		
		// Instance attribute: color (layout 6)
		glEnableVertexAttribArray(6);
		glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
		glVertexAttribDivisor(6, 1);  // One per instance
		
		// Instance attribute: scalar (layout 7)
		glEnableVertexAttribArray(7);
		glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, scalar));
		glVertexAttribDivisor(7, 1);  // One per instance
		
		// Draw instanced spheres
		// Each sphere is scaled by radius in the shader
		// Use 16-bit or 32-bit indices based on optimization
		GLenum indexType = mSphereMesh.uses16BitIndices ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)mSphereMesh.indexCount, indexType, 0, (GLsizei)mesh.vertexCount);
		
		// Disable instance attributes
		glVertexAttribDivisor(5, 0);
		glVertexAttribDivisor(6, 0);
		glVertexAttribDivisor(7, 0);
		glDisableVertexAttribArray(5);
		glDisableVertexAttribArray(6);
		glDisableVertexAttribArray(7);
	}
	
	glBindVertexArray(0);
}

void Model::destroyGPU() {
	for (Mesh& mesh : mMeshes) {
		if (!mesh.isPointCloud && mesh.ebo.valid()) {
			mesh.ebo.destroy();
		}
		mesh.vbo.destroy();
		mesh.vao.destroy();
	}
	
	// Destroy sphere mesh
	if (mSphereMesh.initialized) {
		if (mSphereMesh.ebo.valid()) {
			mSphereMesh.ebo.destroy();
		}
		mSphereMesh.vbo.destroy();
		mSphereMesh.vao.destroy();
		mSphereMesh.initialized = false;
	}
}

glm::mat4 Model::scaleToUnitBox() const {
	glm::vec3 size = mMax - mMin;
	float maxAxis = std::max(size.x, std::max(size.y, size.z));
	if (maxAxis <= 0.0f) return glm::mat4(1.0f);
	float s = 1.0f / maxAxis;
	glm::mat4 M(1.0f);
	M = glm::scale(M, glm::vec3(s));
	M = glm::translate(M, -center());
	return M;
}

} // namespace Graphics
