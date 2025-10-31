#pragma once

#include <string>
#include <glm/glm.hpp>

#include "Graphics/Shader.h"
#include "Graphics/Scene.hpp"
#include "Graphics/View.hpp"
#include "Graphics/Utils.hpp"

struct GLFWwindow;

namespace Graphics {

class Renderer {
public:
	~Renderer() { shutdown(); }
	Renderer() = default;
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&& other) noexcept {
		mWindow = other.mWindow; other.mWindow = nullptr;
		mShader = std::move(other.mShader);
		mLineShader = std::move(other.mLineShader);
		mSphereImpostorShader = std::move(other.mSphereImpostorShader);
		mInstancedSphereShader = std::move(other.mInstancedSphereShader);
		mDepthOnlyShader = std::move(other.mDepthOnlyShader);
		mScene = std::move(other.mScene);
		mView = std::move(other.mView);
		mWidth = other.mWidth; mHeight = other.mHeight; mAspect = other.mAspect;
		mPrevF5Down = other.mPrevF5Down; other.mPrevF5Down = false;
		mVertPath = std::move(other.mVertPath); mFragPath = std::move(other.mFragPath);
		mVertStamp = other.mVertStamp; mFragStamp = other.mFragStamp;
		mLastReloadSec = other.mLastReloadSec; other.mLastReloadSec = 0.0;
		mWireframe = other.mWireframe; other.mWireframe = false;
		mPrevF2Down = other.mPrevF2Down; other.mPrevF2Down = false;
		mImGuiInitialized = other.mImGuiInitialized; other.mImGuiInitialized = false;
	}
	Renderer& operator=(Renderer&& other) noexcept {
		if (this != &other) {
			shutdown();
			mWindow = other.mWindow; other.mWindow = nullptr;
			mShader = std::move(other.mShader);
			mLineShader = std::move(other.mLineShader);
			mSphereImpostorShader = std::move(other.mSphereImpostorShader);
			mInstancedSphereShader = std::move(other.mInstancedSphereShader);
			mScene = std::move(other.mScene);
			mView = std::move(other.mView);
			mWidth = other.mWidth; mHeight = other.mHeight; mAspect = other.mAspect;
			mPrevF5Down = other.mPrevF5Down; other.mPrevF5Down = false;
			mVertPath = std::move(other.mVertPath); mFragPath = std::move(other.mFragPath);
			mVertStamp = other.mVertStamp; mFragStamp = other.mFragStamp;
			mLastReloadSec = other.mLastReloadSec; other.mLastReloadSec = 0.0;
			mWireframe = other.mWireframe; other.mWireframe = false;
			mPrevF2Down = other.mPrevF2Down; other.mPrevF2Down = false;
			mImGuiInitialized = other.mImGuiInitialized; other.mImGuiInitialized = false;
		}
		return *this;
	}

	/// Initialize the renderer with an existing GLFW window and OpenGL context.
	/// Loads the specified model file and sets up shaders, ImGui, and OpenGL state.
	/// @param window GLFW window with valid OpenGL context
	/// @param modelPath Path to 3D model file (.obj, .ply, .off)
	/// @param outError Error message if initialization fails
	/// @return true if successful, false on error
	bool initializeWithContext(GLFWwindow* window, const std::string& modelPath, std::string& outError);
	
	/// Clean up all resources (shaders, models, ImGui, OpenGL objects).
	/// Called automatically by destructor.
	void shutdown();

	/// Handle keyboard and mouse input for camera controls and hotkeys.
	/// Processes WASD/QE movement, mouse look, wireframe toggle (F2), shader reload (F5), etc.
	/// @param deltaTime Time since last frame (seconds)
	void handleInput(float deltaTime);
	
	/// Render one frame. Performs Early-Z prepass, occlusion culling, main rendering, and UI.
	/// Updates profiling data and handles GPU timing queries.
	void render();

	/// Check if the window should close (user clicked X button).
	/// @return true if window should close
	bool shouldClose() const;
	
	/// Get the GLFW window handle.
	/// @return GLFW window pointer
	GLFWwindow* getWindow() const { return mWindow; }
	
	/// Performance profiling data structure
	struct ProfilingData {
		double cpuFrameTime = 0.0;      // CPU frame time (ms)
		double gpuFrameTime = 0.0;      // GPU frame time (ms)
		unsigned int drawCalls = 0;     // Number of draw calls per frame
		unsigned int triangles = 0;    // Number of triangles rendered
		unsigned int points = 0;       // Number of points rendered (for point clouds)
		size_t gpuMemoryUsed = 0;      // GPU memory used (bytes) - if available
		bool gpuTimingAvailable = false; // Whether GPU timing queries are available
	};
	
	/// Get the scene (for UI access).
	/// @return Reference to scene
	Scene& scene() { return mScene; }
	const Scene& scene() const { return mScene; }
	
	/// Get profiling data (for UI access).
	/// @return Reference to profiling data
	ProfilingData& profilingData() { return mProfilingData; }
	const ProfilingData& profilingData() const { return mProfilingData; }
	
	/// Get wireframe state (for UI access).
	/// @return Reference to wireframe flag
	bool& wireframe() { return mWireframe; }

private:
	static void framebufferSizeCallbackForwarder(GLFWwindow* window, int w, int h);
	bool initializeScene(const std::string& modelPath, std::string& outError);
    void onResize(int w, int h);
    void checkShaderHotReload();

private:
	GLFWwindow* mWindow = nullptr;
	Shader mShader;
	Shader mLineShader;  // Simple shader for bounding box/axes
	Shader mSphereImpostorShader;  // Shader for sphere impostors (with geometry shader)
	Shader mInstancedSphereShader;  // Shader for instanced spheres
	Shader mDepthOnlyShader;  // Depth-only shader for Early-Z prepass
	Scene mScene;
	View mView;

	int mWidth = 0;
	int mHeight = 0;
	float mAspect = 1.0f;

	bool mPrevF5Down = false;
	double mLastReloadSec = 0.0;
	bool mWireframe = false;
	bool mPrevF2Down = false;
	bool mImGuiInitialized = false;

	// Shader hot-reload
	std::string mVertPath;
	std::string mFragPath;
	unsigned long long mVertStamp = 0;
	unsigned long long mFragStamp = 0;
	
	// Performance profiling
	ProfilingData mProfilingData;
	
	
	// GPU timing queries
	unsigned int mGPUTimestampQuery[2] = {0, 0};  // Double-buffered queries [0]=start, [1]=end
	bool mGPUTimingSupported = false;
	
	// OpenGL state cache (reduces redundant state changes)
	GLStateCache mGLStateCache;
	
	void initializeProfiling();
	void updateProfiling(double cpuFrameTime);
	void renderProfilingUI();
};

} // namespace Graphics
