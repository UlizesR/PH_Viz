#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include "Graphics/Utils.hpp"
#include "Graphics/Shader.h"
#include "Graphics/UBO.hpp"

namespace Graphics {

class OcclusionCuller {
public:
	OcclusionCuller() = default;
	~OcclusionCuller();
	
	OcclusionCuller(const OcclusionCuller&) = delete;
	OcclusionCuller& operator=(const OcclusionCuller&) = delete;
	OcclusionCuller(OcclusionCuller&& other) noexcept;
	OcclusionCuller& operator=(OcclusionCuller&& other) noexcept;
	
	// Initialize occlusion query support and create proxy geometry
	void initialize();
	
	/// Test if object is occluded using hardware occlusion queries.
	/// @param modelMatrix Transform matrix for the object
	/// @param modelMin Minimum AABB corner (object space)
	/// @param modelMax Maximum AABB corner (object space)
	/// @param frameState Pre-computed frame state (view, proj, viewProj, camPos)
	/// @param depthShader Depth-only shader for rendering the occlusion proxy
	/// @param matricesUBO UBO containing matrices (will be temporarily updated)
	/// @param stateCache Optional OpenGL state cache to minimize redundant state changes
	/// @return true if visible (not occluded), false if occluded
	bool testOcclusion(
		const glm::mat4& modelMatrix,
		const glm::vec3& modelMin,
		const glm::vec3& modelMax,
		const FrameState& frameState,
		Shader& depthShader,
		UniformBuffer& matricesUBO,
		[[maybe_unused]] GLStateCache* stateCache = nullptr
	);
	
	bool isSupported() const { return mOcclusionQuerySupported; }
	bool getLastResult() const { return mLastOcclusionResult; }

private:
	unsigned int mOcclusionQuery = 0;
	bool mOcclusionQuerySupported = false;
	bool mLastOcclusionResult = true;
	
	// Persistent occlusion proxy (unit cube representing AABB)
	GlVertexArray mOcclusionVAO;
	GlBuffer mOcclusionVBO;
	GlBuffer mOcclusionEBO;
};

} // namespace Graphics
