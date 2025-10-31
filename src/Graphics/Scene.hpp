#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>  // For sscanf
#include "Graphics/Model.h"
#include "Graphics/Shader.h"
#include "Graphics/RenderUtils.hpp"
#include "Graphics/UBO.hpp"
#include "Graphics/Utils.hpp"
#include "Graphics/Culling/OcclusionCuller.hpp"

namespace Graphics {

struct Material {
	glm::vec3 albedo = glm::vec3(0.85f, 0.82f, 0.80f);
	float metallic = 0.1f;
	float roughness = 0.4f;
	float ao = 1.0f;
	glm::vec3 skyColor = glm::vec3(0.20f, 0.25f, 0.30f);
	glm::vec3 groundColor = glm::vec3(0.05f, 0.04f, 0.03f);
};

struct Light {
	glm::vec3 dir = glm::normalize(glm::vec3(0.4f, 0.8f, 0.3f));
	glm::vec3 color = glm::vec3(5.0f);
};

class Scene {
public:
	Scene() = default;
	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	
	Scene(Scene&& other) noexcept
		: model(std::move(other.model)),
		  material(other.material), light(other.light),
		  modelMatrix(other.modelMatrix), pointSize(other.pointSize),
		  colorMode(other.colorMode), pointCloudMode(other.pointCloudMode),
		  autoLOD(other.autoLOD), sphereRadius(other.sphereRadius),
		  showBoundingBox(other.showBoundingBox), enableFrustumCulling(other.enableFrustumCulling),
		  enableEarlyZPrepass(other.enableEarlyZPrepass), enableSpatialIndexing(other.enableSpatialIndexing),
		  enableOcclusionCulling(other.enableOcclusionCulling),
		  bboxRenderer(std::move(other.bboxRenderer)),
		  mOcclusionCuller(std::move(other.mOcclusionCuller)),
          mMatricesUBO(std::move(other.mMatricesUBO)),
          mMaterialUBO(std::move(other.mMaterialUBO)),
          mLightingUBO(std::move(other.mLightingUBO)) {}
	
	Scene& operator=(Scene&& other) noexcept {
		if (this != &other) {
			model = std::move(other.model);
			material = other.material;
			light = other.light;
			modelMatrix = other.modelMatrix;
			pointSize = other.pointSize;
			colorMode = other.colorMode;
			pointCloudMode = other.pointCloudMode;
			autoLOD = other.autoLOD;
			sphereRadius = other.sphereRadius;
			showBoundingBox = other.showBoundingBox;
			enableFrustumCulling = other.enableFrustumCulling;
			enableEarlyZPrepass = other.enableEarlyZPrepass;
			enableSpatialIndexing = other.enableSpatialIndexing;
			enableOcclusionCulling = other.enableOcclusionCulling;
			bboxRenderer = std::move(other.bboxRenderer);
			mOcclusionCuller = std::move(other.mOcclusionCuller);
            mMatricesUBO = std::move(other.mMatricesUBO);
            mMaterialUBO = std::move(other.mMaterialUBO);
            mLightingUBO = std::move(other.mLightingUBO);
		}
		return *this;
	}

	Model model;
	Material material;
	Light light;
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	float pointSize = 2.0f;  // Point size for point cloud rendering (in pixels)
	ColorMode colorMode = ColorMode::Uniform;  // Color rendering mode
	PointCloudRenderMode pointCloudMode = PointCloudRenderMode::Points;  // Point cloud rendering mode (can be overridden by auto-LOD)
	bool autoLOD = false;  // Auto-select rendering mode based on camera distance (default: manual selection)
	float sphereRadius = 0.01f;  // Radius for instanced spheres
	bool showBoundingBox = false;  // Show AABB and axes
	bool enableFrustumCulling = true;  // Enable frustum culling (skip rendering outside view)
	bool enableEarlyZPrepass = false;  // Enable Early-Z depth prepass (two-pass rendering for better Early-Z efficiency)
	bool enableSpatialIndexing = true;  // Enable spatial indexing (octree) for point cloud culling and LOD
	bool enableOcclusionCulling = false;  // Enable occlusion culling using hardware queries (skip fully occluded objects)
	BoundingBoxRenderer bboxRenderer;  // Renderer for bounding box visualization
	
	// Occlusion culling helper (manages hardware occlusion queries and proxy geometry)
	OcclusionCuller mOcclusionCuller;
	
	// LOD distance thresholds (auto-LOD uses these to select rendering mode)
	static constexpr float LOD_FAR_THRESHOLD = 50.0f;   // >50 units: use GL_POINTS
	static constexpr float LOD_NEAR_THRESHOLD = 10.0f;  // <10 units: use InstancedSpheres
	

	/// Main rendering method. Draws the scene with full shading.
	/// @param shader Main PBR shader for regular meshes
	/// @param sphereImpostorShader Optional shader for sphere impostor rendering (point clouds)
	/// @param instancedSphereShader Optional shader for instanced sphere rendering (point clouds)
	/// @param frameState Pre-computed frame state (view, proj, viewProj, camPos)
	/// @param wireframe Enable wireframe rendering (for regular meshes only)
	/// @param profilingData Optional pointer to ProfilingData struct for performance metrics
	/// @param stateCache Optional OpenGL state cache to minimize redundant state changes
    void draw(Shader& shader, Shader* sphereImpostorShader, Shader* instancedSphereShader, const FrameState& frameState, bool wireframe = false, void* profilingData = nullptr, [[maybe_unused]] GLStateCache* stateCache = nullptr);
	
	/// Depth-only pass for Early-Z prepass. Renders only depth buffer, no color.
	/// This populates the depth buffer first, so the main pass can skip expensive fragment shader work on occluded fragments.
	/// @param depthShader Minimal depth-only shader
	/// @param frameState Pre-computed frame state (view, proj, viewProj, camPos)
	/// @param stateCache Optional OpenGL state cache to minimize redundant state changes
    void drawDepthOnly(Shader& depthShader, const FrameState& frameState, [[maybe_unused]] GLStateCache* stateCache = nullptr);
	
	void drawBoundingBox(unsigned int lineShaderId, const glm::mat4& view, const glm::mat4& proj) const {
		if (showBoundingBox && bboxRenderer.valid()) {
			bboxRenderer.draw(lineShaderId, modelMatrix, view, proj);
		}
	}
	
	void updateBoundingBox() {
		if (!model.meshes().empty()) {
			bboxRenderer.create(model.min(), model.max());
		}
	}

	void initializeOcclusionQuery() {
		mOcclusionCuller.initialize();
	}
	
	/// Test if the scene's bounding box is occluded using hardware occlusion queries.
	/// @param frameState Pre-computed frame state (view, proj, viewProj, camPos)
	/// @param depthShader Depth shader for occlusion proxy rendering
	/// @param stateCache Optional OpenGL state cache to minimize redundant state changes
	/// @return true if visible (not occluded), false if occluded
	bool testOcclusion(const FrameState& frameState, Shader& depthShader, [[maybe_unused]] GLStateCache* stateCache = nullptr);

	void initializeUBOs() {
		mMatricesUBO.create();
		mMaterialUBO.create();
		mLightingUBO.create();
		
		// Allocate UBO buffers
		mMatricesUBO.setData(GL_UNIFORM_BUFFER, sizeof(MatricesUBO), nullptr, GL_DYNAMIC_DRAW);
		mMaterialUBO.setData(GL_UNIFORM_BUFFER, sizeof(MaterialUBO), nullptr, GL_DYNAMIC_DRAW);
		mLightingUBO.setData(GL_UNIFORM_BUFFER, sizeof(LightingUBO), nullptr, GL_DYNAMIC_DRAW);
		
		// Bind to binding points (persistent binding - shaders can reference these)
		// Note: UBOs are ready to use, but current shaders still use individual uniforms
		// Shaders can be updated to use UBOs for better performance
		mMatricesUBO.bindBase(0);   // binding = 0
		mMaterialUBO.bindBase(1);   // binding = 1
		mLightingUBO.bindBase(2);   // binding = 2
	}

private:
	// Helper function to set up shader-specific uniforms (UBOs handle most uniforms)
	void setupShaderUniforms(Shader& shader, float pointSizeOrRadius = 0.0f) const {
		// UBOs handle matrices, material, and lighting
		// Only set shader-specific uniforms (point size, sphere radius, etc.)
		if (pointSizeOrRadius > 0.0f) {
			shader.setFloat("uPointSize", pointSizeOrRadius);
		}
	}
	
	void updateUBOs(const glm::mat4& modelMat, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos) {
		// Update matrices UBO
		MatricesUBO matricesData;
		matricesData.model = modelMat;
		matricesData.view = view;
		matricesData.proj = proj;
		matricesData.viewProj = proj * view;
		matricesData.camPos = glm::vec4(camPos, 1.0f);
		mMatricesUBO.updateData(0, sizeof(MatricesUBO), &matricesData);
		
		// Update material UBO
		MaterialUBO materialData;
		materialData.albedo = glm::vec4(material.albedo, material.metallic);
		// Pack params: roughness, ao, colorMode (as float bits), scalarMin
		materialData.params.x = material.roughness;
		materialData.params.y = material.ao;
		materialData.params.z = static_cast<float>(static_cast<int>(colorMode));
		materialData.params.w = model.scalarMin();
		materialData.scalars.x = model.scalarMax();
		materialData.scalars.y = 0.0f;
		materialData.scalars.z = 0.0f;
		materialData.scalars.w = 0.0f;
		materialData.skyColor = glm::vec4(material.skyColor, 0.0f);
		materialData.groundColor = glm::vec4(material.groundColor, 0.0f);
		mMaterialUBO.updateData(0, sizeof(MaterialUBO), &materialData);
		
		// Update lighting UBO
		LightingUBO lightingData;
		lightingData.lightDir = glm::vec4(light.dir, 0.0f);
		lightingData.lightColor = glm::vec4(light.color, 0.0f);
		mLightingUBO.updateData(0, sizeof(LightingUBO), &lightingData);
	}

private:
	// Uniform Buffer Objects
	UniformBuffer mMatricesUBO;   // binding = 0
	UniformBuffer mMaterialUBO;  // binding = 1
	UniformBuffer mLightingUBO;   // binding = 2
};

} // namespace Graphics
