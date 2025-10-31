#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <algorithm>
#include "Graphics/Utils.hpp"

namespace Graphics {

// Color rendering modes
enum class ColorMode {
	Uniform,    // Single color for entire model
	VertexRGB,  // Use vertex colors from PLY RGB
	Scalar      // Color by scalar value (filtration, etc.)
};

// Point cloud rendering modes
enum class PointCloudRenderMode {
	Points,          // GL_POINTS for pure speed
	SphereImpostors, // Sphere impostors for shaded depth perception
	InstancedSpheres // Instanced low-poly spheres for close-ups
};

// Bounding box and axes renderer
class BoundingBoxRenderer {
public:
	BoundingBoxRenderer() = default;
	~BoundingBoxRenderer() { destroy(); }
	
	BoundingBoxRenderer(const BoundingBoxRenderer&) = delete;
	BoundingBoxRenderer& operator=(const BoundingBoxRenderer&) = delete;
	
	BoundingBoxRenderer(BoundingBoxRenderer&& other) noexcept
		: mMin(other.mMin), mMax(other.mMax),
		  mBoxVAO(std::move(other.mBoxVAO)), mAxesVAO(std::move(other.mAxesVAO)),
		  mBoxVBO(std::move(other.mBoxVBO)), mAxesVBO(std::move(other.mAxesVBO)) {}
	
	BoundingBoxRenderer& operator=(BoundingBoxRenderer&& other) noexcept {
		if (this != &other) {
			destroy();
			mMin = other.mMin; mMax = other.mMax;
			mBoxVAO = std::move(other.mBoxVAO);
			mAxesVAO = std::move(other.mAxesVAO);
			mBoxVBO = std::move(other.mBoxVBO);
			mAxesVBO = std::move(other.mAxesVBO);
		}
		return *this;
	}

	void create(const glm::vec3& min, const glm::vec3& max) {
		mMin = min;
		mMax = max;
		
		// Create box edges (12 edges of a cube)
		float vertices[] = {
			// Bottom face (z = min.z)
			min.x, min.y, min.z,  max.x, min.y, min.z,
			min.x, min.y, min.z,  min.x, max.y, min.z,
			max.x, min.y, min.z,  max.x, max.y, min.z,
			min.x, max.y, min.z,  max.x, max.y, min.z,
			
			// Top face (z = max.z)
			min.x, min.y, max.z,  max.x, min.y, max.z,
			min.x, min.y, max.z,  min.x, max.y, max.z,
			max.x, min.y, max.z,  max.x, max.y, max.z,
			min.x, max.y, max.z,  max.x, max.y, max.z,
			
			// Vertical edges (4 edges connecting bottom to top)
			min.x, min.y, min.z,  min.x, min.y, max.z,
			max.x, min.y, min.z,  max.x, min.y, max.z,
			min.x, max.y, min.z,  min.x, max.y, max.z,
			max.x, max.y, min.z,  max.x, max.y, max.z
		};
		
		// Create coordinate axes (centered at bounding box center, extending symmetrically)
		glm::vec3 center = 0.5f * (min + max);
		glm::vec3 size = max - min;
		
		float axes[] = {
			// X axis (red) - centered at box center, extending in +/- X
			center.x - size.x * 0.5f, center.y, center.z,
			center.x + size.x * 0.5f, center.y, center.z,
			// Y axis (green) - centered at box center, extending in +/- Y
			center.x, center.y - size.y * 0.5f, center.z,
			center.x, center.y + size.y * 0.5f, center.z,
			// Z axis (blue) - centered at box center, extending in +/- Z
			center.x, center.y, center.z - size.z * 0.5f,
			center.x, center.y, center.z + size.z * 0.5f
		};
		
		if (!mBoxVAO.valid()) {
			mBoxVAO.create();
			mBoxVBO.create();
			mBoxVAO.bind();
			mBoxVBO.bind(GL_ARRAY_BUFFER);
			mBoxVBO.setData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			mBoxVAO.bind(); // Keep VAO bound
		}
		
		if (!mAxesVAO.valid()) {
			mAxesVAO.create();
			mAxesVBO.create();
			mAxesVAO.bind();
			mAxesVBO.bind(GL_ARRAY_BUFFER);
			mAxesVBO.setData(GL_ARRAY_BUFFER, sizeof(axes), axes, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			mAxesVAO.bind(); // Keep VAO bound
		}
		glBindVertexArray(0);
	}

	void draw(unsigned int shaderProgram, const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj) const {
		if (!mBoxVAO.valid()) return;
		
		// Save current state
		GLint prevProgram = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
		GLfloat prevLineWidth = 1.0f;
		glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);
		
		glUseProgram(shaderProgram);
		glLineWidth(2.0f);  // Make bounding box lines slightly thicker
		
		// Set uniforms (assuming standard names)
		int modelLoc = glGetUniformLocation(shaderProgram, "uModel");
		int viewLoc = glGetUniformLocation(shaderProgram, "uView");
		int projLoc = glGetUniformLocation(shaderProgram, "uProj");
		int colorLoc = glGetUniformLocation(shaderProgram, "uColor");
		
		if (modelLoc >= 0) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
		if (viewLoc >= 0) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
		if (projLoc >= 0) glUniformMatrix4fv(projLoc, 1, GL_FALSE, &proj[0][0]);
		
		// Draw bounding box (white lines)
		if (colorLoc >= 0) glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
		mBoxVAO.bind();
		glDrawArrays(GL_LINES, 0, 24);  // 12 edges * 2 vertices each
		
		// Draw coordinate axes (colored: X=red, Y=green, Z=blue)
		if (mAxesVAO.valid()) {
			mAxesVAO.bind();
			if (colorLoc >= 0) {
				// X axis - red
				glUniform3f(colorLoc, 1.0f, 0.0f, 0.0f);
				glDrawArrays(GL_LINES, 0, 2);
				// Y axis - green
				glUniform3f(colorLoc, 0.0f, 1.0f, 0.0f);
				glDrawArrays(GL_LINES, 2, 2);
				// Z axis - blue
				glUniform3f(colorLoc, 0.0f, 0.0f, 1.0f);
				glDrawArrays(GL_LINES, 4, 2);
			} else {
				glDrawArrays(GL_LINES, 0, 6);  // 3 axes * 2 vertices each
			}
		}
		glBindVertexArray(0);
		
		// Restore previous state
		glLineWidth(prevLineWidth);
		if (prevProgram >= 0) {
			glUseProgram(static_cast<unsigned int>(prevProgram));
		}
	}

	void destroy() {
		mAxesVBO.destroy();
		mAxesVAO.destroy();
		mBoxVBO.destroy();
		mBoxVAO.destroy();
	}

	bool valid() const { return mBoxVAO.valid(); }

private:
	glm::vec3 mMin, mMax;
	GlVertexArray mBoxVAO, mAxesVAO;
	GlBuffer mBoxVBO, mAxesVBO;
};

// Camera preset storage
struct CameraPreset {
	glm::vec3 eye;
	glm::vec3 target;
	glm::vec3 up;
	float yaw;
	float pitch;
	
	void save(const glm::vec3& e, const glm::vec3& t, const glm::vec3& u, float y, float p) {
		eye = e; target = t; up = u; yaw = y; pitch = p;
	}
	
	void restore(glm::vec3& e, glm::vec3& t, glm::vec3& u, float& y, float& p) const {
		e = eye; t = target; u = up; y = yaw; p = pitch;
	}
};

// Frustum culling utility
class Frustum {
public:
	Frustum() = default;
	
	// Extract frustum planes from view-projection matrix
	void extractFromMatrix(const glm::mat4& viewProj) {
		// Left plane
		mPlanes[0] = glm::vec4(
			viewProj[0][3] + viewProj[0][0],
			viewProj[1][3] + viewProj[1][0],
			viewProj[2][3] + viewProj[2][0],
			viewProj[3][3] + viewProj[3][0]
		);
		
		// Right plane
		mPlanes[1] = glm::vec4(
			viewProj[0][3] - viewProj[0][0],
			viewProj[1][3] - viewProj[1][0],
			viewProj[2][3] - viewProj[2][0],
			viewProj[3][3] - viewProj[3][0]
		);
		
		// Bottom plane
		mPlanes[2] = glm::vec4(
			viewProj[0][3] + viewProj[0][1],
			viewProj[1][3] + viewProj[1][1],
			viewProj[2][3] + viewProj[2][1],
			viewProj[3][3] + viewProj[3][1]
		);
		
		// Top plane
		mPlanes[3] = glm::vec4(
			viewProj[0][3] - viewProj[0][1],
			viewProj[1][3] - viewProj[1][1],
			viewProj[2][3] - viewProj[2][1],
			viewProj[3][3] - viewProj[3][1]
		);
		
		// Near plane
		mPlanes[4] = glm::vec4(
			viewProj[0][3] + viewProj[0][2],
			viewProj[1][3] + viewProj[1][2],
			viewProj[2][3] + viewProj[2][2],
			viewProj[3][3] + viewProj[3][2]
		);
		
		// Far plane
		mPlanes[5] = glm::vec4(
			viewProj[0][3] - viewProj[0][2],
			viewProj[1][3] - viewProj[1][2],
			viewProj[2][3] - viewProj[2][2],
			viewProj[3][3] - viewProj[3][2]
		);
		
		// Normalize all planes
		for (int i = 0; i < 6; ++i) {
			float length = glm::length(glm::vec3(mPlanes[i]));
			if (length > 0.0f) {
				mPlanes[i] /= length;
			}
		}
	}
	
	// Test if AABB intersects frustum (returns true if visible, false if culled)
	bool intersectsAABB(const glm::vec3& min, const glm::vec3& max) const {
		// Test AABB against all 6 planes
		// If AABB is completely outside any plane, it's culled
		for (int i = 0; i < 6; ++i) {
			const glm::vec4& plane = mPlanes[i];
			const glm::vec3 normal = glm::vec3(plane);
			
			// Find the positive vertex (furthest in the direction of the plane normal)
			glm::vec3 positiveVertex;
			positiveVertex.x = (normal.x > 0.0f) ? max.x : min.x;
			positiveVertex.y = (normal.y > 0.0f) ? max.y : min.y;
			positiveVertex.z = (normal.z > 0.0f) ? max.z : min.z;
			
			// Calculate distance from positive vertex to plane
			float distance = glm::dot(normal, positiveVertex) + plane.w;
			
			// If positive vertex is outside plane (negative distance), AABB is culled
			if (distance < 0.0f) {
				return false;
			}
		}
		
		// AABB intersects or is inside all planes
		return true;
	}
	
	// Test if AABB transformed by a model matrix intersects frustum
	bool intersectsTransformedAABB(const glm::vec3& min, const glm::vec3& max, const glm::mat4& modelMatrix) const {
		// Transform AABB corners to world space
		glm::vec3 corners[8];
		corners[0] = glm::vec3(modelMatrix * glm::vec4(min.x, min.y, min.z, 1.0f));
		corners[1] = glm::vec3(modelMatrix * glm::vec4(max.x, min.y, min.z, 1.0f));
		corners[2] = glm::vec3(modelMatrix * glm::vec4(min.x, max.y, min.z, 1.0f));
		corners[3] = glm::vec3(modelMatrix * glm::vec4(max.x, max.y, min.z, 1.0f));
		corners[4] = glm::vec3(modelMatrix * glm::vec4(min.x, min.y, max.z, 1.0f));
		corners[5] = glm::vec3(modelMatrix * glm::vec4(max.x, min.y, max.z, 1.0f));
		corners[6] = glm::vec3(modelMatrix * glm::vec4(min.x, max.y, max.z, 1.0f));
		corners[7] = glm::vec3(modelMatrix * glm::vec4(max.x, max.y, max.z, 1.0f));
		
		// Find transformed AABB bounds
		glm::vec3 tMin = corners[0];
		glm::vec3 tMax = corners[0];
		for (int i = 1; i < 8; ++i) {
			tMin = glm::min(tMin, corners[i]);
			tMax = glm::max(tMax, corners[i]);
		}
		
		// Test transformed AABB against frustum
		return intersectsAABB(tMin, tMax);
	}

private:
	glm::vec4 mPlanes[6];  // Left, Right, Bottom, Top, Near, Far
};

} // namespace Graphics

