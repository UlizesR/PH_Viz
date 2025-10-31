#include "Graphics/Culling/OcclusionCuller.hpp"
#include "Graphics/Utils.hpp"
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

namespace Graphics {

OcclusionCuller::~OcclusionCuller() {
	if (mOcclusionQuery != 0) {
		glDeleteQueries(1, &mOcclusionQuery);
		mOcclusionQuery = 0;
	}
}

OcclusionCuller::OcclusionCuller(OcclusionCuller&& other) noexcept
	: mOcclusionQuery(other.mOcclusionQuery),
	  mOcclusionQuerySupported(other.mOcclusionQuerySupported),
	  mLastOcclusionResult(other.mLastOcclusionResult),
	  mOcclusionVAO(std::move(other.mOcclusionVAO)),
	  mOcclusionVBO(std::move(other.mOcclusionVBO)),
	  mOcclusionEBO(std::move(other.mOcclusionEBO)) {
	other.mOcclusionQuery = 0;
}

OcclusionCuller& OcclusionCuller::operator=(OcclusionCuller&& other) noexcept {
	if (this != &other) {
		if (mOcclusionQuery != 0) {
			glDeleteQueries(1, &mOcclusionQuery);
		}
		mOcclusionQuery = other.mOcclusionQuery;
		mOcclusionQuerySupported = other.mOcclusionQuerySupported;
		mLastOcclusionResult = other.mLastOcclusionResult;
		mOcclusionVAO = std::move(other.mOcclusionVAO);
		mOcclusionVBO = std::move(other.mOcclusionVBO);
		mOcclusionEBO = std::move(other.mOcclusionEBO);
		other.mOcclusionQuery = 0;
	}
	return *this;
}

void OcclusionCuller::initialize() {
	// Check if occlusion queries are supported (OpenGL 3.3+)
	const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
	if (glVersion) {
		int major = 0, minor = 0;
		sscanf(glVersion, "%d.%d", &major, &minor);
		mOcclusionQuerySupported = (major > 3 || (major == 3 && minor >= 3));
	}
	
	if (mOcclusionQuerySupported) {
		// Generate query object for occlusion testing
		if (mOcclusionQuery == 0) {
			glGenQueries(1, &mOcclusionQuery);
		}
		// Create persistent filled-box proxy (12 triangles)
		if (!mOcclusionVAO.valid()) {
			// 8 corners of unit cube in model AABB space will be transformed by model matrix; here store as AABB-relative
			// We'll upload 8 unique vertices and 36 indices (12 triangles)
			const float vertices[8 * 3] = {
				0.0f, 0.0f, 0.0f, // 0 min
				1.0f, 0.0f, 0.0f, // 1
				0.0f, 1.0f, 0.0f, // 2
				1.0f, 1.0f, 0.0f, // 3
				0.0f, 0.0f, 1.0f, // 4
				1.0f, 0.0f, 1.0f, // 5
				0.0f, 1.0f, 1.0f, // 6
				1.0f, 1.0f, 1.0f  // 7 max
			};
			const unsigned short indices[36] = {
				// bottom (z=0)
				0,1,2,  2,1,3,
				// top (z=1)
				4,6,5,  6,7,5,
				// left (x=0)
				0,2,4,  2,6,4,
				// right (x=1)
				1,5,3,  3,5,7,
				// front (y=0)
				0,4,1,  1,4,5,
				// back (y=1)
				2,3,6,  3,7,6
			};
			mOcclusionVAO.create();
			mOcclusionVBO.create();
			mOcclusionEBO.create();
			mOcclusionVAO.bind();
			mOcclusionVBO.bind(GL_ARRAY_BUFFER);
			mOcclusionVBO.setData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
			mOcclusionEBO.bind(GL_ELEMENT_ARRAY_BUFFER);
			mOcclusionEBO.setData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		}
	}
}

bool OcclusionCuller::testOcclusion(
	const glm::mat4& modelMatrix,
	const glm::vec3& modelMin,
	const glm::vec3& modelMax,
	const FrameState& frameState,
	Shader& depthShader,
	UniformBuffer& matricesUBO,
	GLStateCache* stateCache) {
	if (!mOcclusionQuerySupported || mOcclusionQuery == 0) return true;
	
	// Set up depth-only rendering state
	if (stateCache) {
		stateCache->colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		stateCache->depthMask(GL_TRUE);
		stateCache->depthFunc(GL_LESS);
	} else {
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);
	}
	
	// Compute AABB transform: modelMatrix * translate(min) * scale(size)
	glm::vec3 size = modelMax - modelMin;
	glm::mat4 aabbXform = modelMatrix * glm::translate(glm::mat4(1.0f), modelMin) * glm::scale(glm::mat4(1.0f), size);
	
	// Update UBO with AABB transform (temporary update for occlusion test)
	if (matricesUBO.valid()) {
		struct MatricesUBO {
			glm::mat4 model, view, proj, viewProj;
			glm::vec4 camPos;
		} matricesData;
		matricesData.model = aabbXform;
		matricesData.view = frameState.view;
		matricesData.proj = frameState.proj;
		matricesData.viewProj = frameState.viewProj;
		matricesData.camPos = glm::vec4(frameState.camPos, 1.0f);
		matricesUBO.updateData(0, sizeof(MatricesUBO), &matricesData);
	}
	
	depthShader.use();
	glBeginQuery(GL_SAMPLES_PASSED, mOcclusionQuery);
	mOcclusionVAO.bind();
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
	glBindVertexArray(0);
	glEndQuery(GL_SAMPLES_PASSED);
	
	// Restore color writes
	if (stateCache) {
		stateCache->colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	} else {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
	
	// Check if query result is available
	GLuint available = 0;
	glGetQueryObjectuiv(mOcclusionQuery, GL_QUERY_RESULT_AVAILABLE, &available);
	if (available) {
		GLuint samplesPassed = 0;
		glGetQueryObjectuiv(mOcclusionQuery, GL_QUERY_RESULT, &samplesPassed);
		mLastOcclusionResult = (samplesPassed > 0);
		return mLastOcclusionResult;
	}
	return mLastOcclusionResult;
}

} // namespace Graphics
