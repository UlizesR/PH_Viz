#include "Graphics/Scene.hpp"

namespace Graphics {

void Scene::draw(Shader& shader, Shader* sphereImpostorShader, Shader* instancedSphereShader, const FrameState& frameState, bool wireframe, void* profilingData, GLStateCache* /*stateCache*/) {
	struct ProfilingData {
		double cpuFrameTime;
		double gpuFrameTime;
		unsigned int drawCalls;
		unsigned int triangles;
		unsigned int points;
		size_t gpuMemoryUsed;
		bool gpuTimingAvailable;
	};
	ProfilingData* profData = static_cast<ProfilingData*>(profilingData);

	if (enableFrustumCulling) {
		Frustum frustum; frustum.extractFromMatrix(frameState.viewProj);
		if (!frustum.intersectsTransformedAABB(model.min(), model.max(), modelMatrix)) return;
	}

	if (enableOcclusionCulling && !model.isPointCloud()) {
		if (!mOcclusionCuller.isSupported() || !mOcclusionCuller.getLastResult()) return;
	}

	if (mMatricesUBO.valid()) updateUBOs(modelMatrix, frameState.view, frameState.proj, frameState.camPos);

	Shader* activeShader = &shader;
	if (model.isPointCloud()) {
		PointCloudRenderMode actualMode = pointCloudMode;
		if (autoLOD) {
			glm::vec3 modelCenter = glm::vec3(modelMatrix * glm::vec4(model.center(), 1.0f));
			float distance = glm::length(frameState.camPos - modelCenter);
			if (distance > LOD_FAR_THRESHOLD) actualMode = PointCloudRenderMode::Points;
			else if (distance < LOD_NEAR_THRESHOLD) actualMode = PointCloudRenderMode::InstancedSpheres;
			else actualMode = PointCloudRenderMode::SphereImpostors;
		}

		switch (actualMode) {
			case PointCloudRenderMode::Points:
				activeShader = &shader; activeShader->use();
				if (enableSpatialIndexing && model.hasSpatialIndex()) {
					auto visibleIndices = model.spatialIndex().getVisiblePoints(frameState.viewProj, frameState.camPos);
					if (!visibleIndices.empty()) {
						model.drawPointsSubset(visibleIndices, pointSize);
						if (profData) { profData->drawCalls++; profData->points += static_cast<unsigned int>(visibleIndices.size()); }
					}
				} else {
					model.drawPoints(pointSize);
					if (profData) { profData->drawCalls++; profData->points += model.meshes()[0].vertexCount; }
				}
				break;
			case PointCloudRenderMode::SphereImpostors:
				if (sphereImpostorShader) { activeShader = sphereImpostorShader; activeShader->use(); setupShaderUniforms(*activeShader, pointSize); model.drawSphereImpostors(pointSize); if (profData) { profData->drawCalls++; profData->points += model.meshes()[0].vertexCount; } }
				break;
			case PointCloudRenderMode::InstancedSpheres:
				if (instancedSphereShader) { activeShader = instancedSphereShader; activeShader->use(); setupShaderUniforms(*activeShader); activeShader->setFloat("uSphereRadius", sphereRadius); model.drawInstancedSpheres(sphereRadius); if (profData) { profData->drawCalls++; profData->points += model.meshes()[0].vertexCount; } }
				break;
		}
	} else {
		activeShader = &shader; activeShader->use();
		activeShader->setFloat("uWireframe", wireframe ? 1.0f : 0.0f);
		activeShader->setVec3("uWireframeColor", glm::vec3(1.0f, 0.5f, 0.0f));
		model.draw();
		if (profData) {
			profData->drawCalls++;
			for (const auto& mesh : model.meshes()) profData->triangles += mesh.indexCount / 3;
		}
	}
}

void Scene::drawDepthOnly(Shader& depthShader, const FrameState& frameState, GLStateCache* /*stateCache*/) {
	if (enableFrustumCulling) {
		Frustum frustum; frustum.extractFromMatrix(frameState.viewProj);
		if (!frustum.intersectsTransformedAABB(model.min(), model.max(), modelMatrix)) return;
	}
	if (mMatricesUBO.valid()) updateUBOs(modelMatrix, frameState.view, frameState.proj, frameState.camPos);
	depthShader.use();
	if (!model.isPointCloud()) model.draw();
}

bool Scene::testOcclusion(const FrameState& frameState, Shader& depthShader, GLStateCache* stateCache) {
	if (!enableOcclusionCulling) return true;
	return mOcclusionCuller.testOcclusion(modelMatrix, model.min(), model.max(), frameState, depthShader, mMatricesUBO, stateCache);
}

} // namespace Graphics


