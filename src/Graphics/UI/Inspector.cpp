#include "Graphics/UI/Inspector.hpp"

namespace Graphics::UI {

void drawSceneUI(Renderer& r) {
	Scene& scene = r.scene();
	bool isPointCloud = scene.model.isPointCloud();
	ImGui::SetNextWindowSizeConstraints(ImVec2(280, -1), ImVec2(FLT_MAX, FLT_MAX));
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	ImGui::Begin("PH_Viz", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	bool& wireframe = r.wireframe();
	if (ImGui::Checkbox("Wireframe", &wireframe)) {
		if (!scene.model.isPointCloud()) glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
	}
	if (isPointCloud) { ImGui::SameLine(); ImGui::TextDisabled("(disabled for point clouds)"); }
	ImGui::Spacing();
	const char* colorModeNames[] = { "Uniform", "Vertex RGB", "Scalar" };
	int currentMode = static_cast<int>(scene.colorMode);
	if (ImGui::Combo("Color Mode", &currentMode, colorModeNames, 3)) { scene.colorMode = static_cast<ColorMode>(currentMode); }
	ImGui::Spacing();
	if (ImGui::Checkbox("Show BBox", &scene.showBoundingBox)) { if (scene.showBoundingBox && !scene.bboxRenderer.valid()) scene.updateBoundingBox(); }
	ImGui::Spacing();
	ImGui::Checkbox("Frustum Culling", &scene.enableFrustumCulling); ImGui::SameLine(); ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Skip rendering objects outside the camera view.\nImproves performance for large models.");
	if (isPointCloud) {
		ImGui::Spacing();
		ImGui::Checkbox("Spatial Indexing", &scene.enableSpatialIndexing); ImGui::SameLine(); ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use octree for view-dependent culling and LOD.\nOnly works for point clouds with >= threshold.\nBig speedups for large clouds.");
		ImGui::Spacing();
		if (ImGui::Checkbox("Auto LOD", &scene.autoLOD)) {}
		ImGui::Spacing();
		const char* renderModeNames[] = { "GL_POINTS (Fast)", "Sphere Impostors", "Instanced Spheres" };
		int currentRenderMode = static_cast<int>(scene.pointCloudMode);
		if (!scene.autoLOD) ImGui::Combo("Point Mode", &currentRenderMode, renderModeNames, 3);
	}
	if (!isPointCloud) {
		ImGui::Spacing();
		ImGui::Checkbox("Occlusion Culling", &scene.enableOcclusionCulling); ImGui::SameLine(); ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use hardware occlusion queries to skip fully occluded objects.");
		ImGui::Spacing();
		ImGui::Checkbox("Early-Z Prepass", &scene.enableEarlyZPrepass); ImGui::SameLine(); ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Two-pass rendering: depth-only then full shading.");
	}
	ImGui::End();
}

void drawProfilingUI(Renderer& r) {
	const auto& prof = r.profilingData();
	ImGui::SetNextWindowPos(ImVec2(10, 350), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(280, -1), ImVec2(FLT_MAX, FLT_MAX));
	ImGui::Begin("Performance Profiling", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("Frame Time:"); ImGui::SameLine();
	float fps = prof.cpuFrameTime > 0.0 ? (1000.0f / static_cast<float>(prof.cpuFrameTime)) : 0.0f;
	ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%.1f FPS", fps);
	ImGui::Text("CPU: %.2f ms", prof.cpuFrameTime);
	if (prof.gpuTimingAvailable && prof.gpuFrameTime > 0.0) {
		ImGui::Text("GPU: %.2f ms", prof.gpuFrameTime);
		ImGui::Text("GPU/CPU Ratio: %.2f%%", (prof.gpuFrameTime / prof.cpuFrameTime) * 100.0f);
	} else ImGui::TextDisabled("GPU: N/A");
	ImGui::Separator();
	ImGui::Text("Rendering:");
	ImGui::Text("Draw Calls: %u", prof.drawCalls);
	if (prof.triangles > 0) ImGui::Text("Triangles: %u", prof.triangles);
	if (prof.points > 0) ImGui::Text("Points: %u", prof.points);
	ImGui::Separator();
	ImGui::Text("Memory:");
	if (prof.gpuMemoryUsed > 0) {
		float mb = static_cast<float>(prof.gpuMemoryUsed) / (1024.0f * 1024.0f);
		ImGui::Text("GPU: %.2f MB", mb);
	} else ImGui::TextDisabled("GPU: N/A");
	ImGui::End();
}

}


