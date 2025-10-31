#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>

#include "Graphics/Utils.hpp"
#include "Graphics/SpatialIndex.hpp"

namespace Graphics {

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texcoord;
	glm::vec3 color = glm::vec3(1.0f);  // Vertex color (RGB), default white
	float scalar = 0.0f;  // Scalar value for color mapping
};

static_assert(sizeof(Vertex) == sizeof(float) * 12, "Unexpected Vertex size; check packing.");

struct Mesh {
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	GlVertexArray vao;
	GlBuffer vbo;
	GlBuffer ebo;

	unsigned int indexCount = 0;
	unsigned int vertexCount = 0;
	bool isPointCloud = false;  // True if no faces, just points
	bool uses16BitIndices = false;  // True if using uint16_t indices (< 65k vertices)
	bool usesOptimizedVertices = false;  // True if using half-floats for positions/UVs (reduces memory bandwidth)
};

/// 3D model loader and renderer. Supports .obj, .ply, and .off file formats.
/// Handles CPU-side loading with Assimp, GPU upload with optimizations (half-floats, 16-bit indices),
/// and various rendering modes (regular meshes, point clouds, sphere impostors, instanced spheres).
class Model {
public:
	/// Load model from file. Supports .obj, .ply, and .off formats.
	/// Extracts vertices, normals, UVs, colors, and scalar values.
	/// Computes AABB, scalar range, and builds spatial index for large point clouds.
	/// @param path File path to 3D model
	/// @param outError Error message if loading fails
	/// @return true if successful, false on error
	bool loadFromFile(const std::string& path, std::string& outError);
	
	/// Get all meshes in the model.
	/// @return Const reference to mesh vector
	const std::vector<Mesh>& meshes() const { return mMeshes; }

	/// Upload mesh data to GPU. Creates VAOs, VBOs, and EBOs.
	/// Applies optimizations: half-floats for positions/UVs, 16-bit indices when possible.
	/// @param dropCpu If true, clears CPU-side vertex/index vectors after upload
	void uploadToGPU(bool dropCpu = true);
	
	/// Draw all meshes with full vertex attributes (position, normal, UV, color, scalar).
	/// Uses indexed rendering with proper shader state.
	void draw() const;
	
	/// Draw point cloud using GL_POINTS primitive.
	/// @param pointSize Size of points in pixels (set via glPointSize)
	void drawPoints(float pointSize = 1.0f) const;
	
	/// Draw a subset of points using provided indices. Used for spatial indexing culling.
	/// Creates a temporary EBO for the subset and renders with GL_POINTS.
	/// @param indices Point indices to render
	/// @param pointSize Size of points in pixels
	void drawPointsSubset(const std::vector<unsigned int>& indices, float pointSize = 1.0f) const;
	
	/// Draw point cloud as sphere impostors using a geometry shader.
	/// Expands each point into a billboard quad that's shaded as a sphere.
	/// @param pointSize Radius of sphere impostors in world space
	void drawSphereImpostors(float pointSize = 1.0f) const;
	
	/// Draw point cloud as instanced low-poly spheres.
	/// Uses hardware instancing to render many spheres efficiently.
	/// @param radius Radius of each sphere in world space
	void drawInstancedSpheres(float radius = 0.01f) const;
	
	/// Destroy all GPU resources (VAOs, VBOs, EBOs). Call before destroying Model.
	void destroyGPU();

	bool isPointCloud() const { return !mMeshes.empty() && mMeshes[0].isPointCloud; }

	// AABB accessors
	const glm::vec3& min() const { return mMin; }
	const glm::vec3& max() const { return mMax; }
	glm::vec3 center() const { return 0.5f * (mMin + mMax); }
	
	// Spatial index for point clouds (octree)
	const Octree& spatialIndex() const { return mSpatialIndex; }
	Octree& spatialIndex() { return mSpatialIndex; }
	bool hasSpatialIndex() const { return mSpatialIndex.valid(); }

	// Scalar range accessors (for color mapping)
	float scalarMin() const { return mScalarMin; }
	float scalarMax() const { return mScalarMax; }

	// Returns S * T so the longest axis fits 1 and model is centered at origin
	glm::mat4 scaleToUnitBox() const;

private:
	std::vector<Mesh> mMeshes;
	glm::vec3 mMin = glm::vec3(0.0f);
	glm::vec3 mMax = glm::vec3(0.0f);
	float mScalarMin = 0.0f;
	float mScalarMax = 1.0f;
	
	// Spatial index for point clouds (octree)
	Octree mSpatialIndex;
	
	// Sphere mesh for instanced rendering
	mutable struct {
		GlVertexArray vao;
		GlBuffer vbo;
		GlBuffer ebo;
		unsigned int indexCount = 0;
		bool uses16BitIndices = false;  // True if using uint16_t indices
		bool initialized = false;
	} mSphereMesh;
	
	void generateSphereMesh(unsigned int subdivisions = 2) const;
};

} // namespace Graphics
