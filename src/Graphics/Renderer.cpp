#include "Renderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cfloat>
#include <cstring>  // For strstr, sscanf

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "Graphics/RenderUtils.hpp"
#include "Graphics/UI/Inspector.hpp"
#include "Graphics/Utils.hpp"

namespace Graphics {

void Renderer::framebufferSizeCallbackForwarder(GLFWwindow* window, int w, int h) {
	Renderer* self = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
	if (self) self->onResize(w, h);
}

static bool readTextFile(const std::string& path, std::string& out) {
	std::ifstream f(path);
	if (!f.is_open()) return false;
	std::ostringstream ss; ss << f.rdbuf();
	out = ss.str();
	return true;
}

static unsigned long long fileStamp(const std::string& path) {
	std::error_code ec;
	auto ft = std::filesystem::last_write_time(path, ec);
	if (ec) return 0ULL;
	return static_cast<unsigned long long>(ft.time_since_epoch().count());
}

bool Renderer::initializeWithContext(GLFWwindow* window, const std::string& modelPath, std::string& outError) {
	if (!window) { outError = "Renderer: window is null"; return false; }
	mWindow = window;
	glfwSetWindowUserPointer(mWindow, this);
	glfwSetFramebufferSizeCallback(mWindow, framebufferSizeCallbackForwarder);
	glfwGetFramebufferSize(mWindow, &mWidth, &mHeight);
	mAspect = (mHeight > 0) ? (float)mWidth / (float)mHeight : 1.0f;

	// Initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();

	const char* glsl_version = "#version 330";
	ImGui_ImplGlfw_InitForOpenGL(mWindow, true);
	ImGui_ImplOpenGL3_Init(glsl_version);
	mImGuiInitialized = true;

	if (!initializeScene(modelPath, outError)) {
		// Cleanup ImGui if scene init fails
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		mImGuiInitialized = false;
		return false;
	}

	return true;
}

void APIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	(void)source; (void)type; (void)id; (void)severity; (void)length; (void)userParam;
	std::cerr << "GL DEBUG: " << message << '\n';
}

bool Renderer::initializeScene(const std::string& modelPath, std::string& outError) {
	// Log OpenGL information
	const char* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
	const char* glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
	const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
	std::cout << "=== PH_Viz Initialization ===" << std::endl;
	std::cout << "OpenGL Vendor: " << (glVendor ? glVendor : "Unknown") << std::endl;
	std::cout << "OpenGL Renderer: " << (glRenderer ? glRenderer : "Unknown") << std::endl;
	std::cout << "OpenGL Version: " << (glVersion ? glVersion : "Unknown") << std::endl;
	
	// Log configuration thresholds
	std::cout << "\n=== Configuration Thresholds ===" << std::endl;
	std::cout << "Min Vertices for Threading: " << Config::MinVerticesForThreading << std::endl;
	std::cout << "Min Meshes for Threading: " << Config::MinMeshesForThreading << std::endl;
	std::cout << "Point Cloud Min Points for Octree: " << Config::PointCloudMinPointsForOctree << std::endl;
	std::cout << "Octree Max Depth: " << Config::OctreeMaxDepth << std::endl;
	std::cout << "Octree Points Per Node: " << Config::OctreePointsPerNode << std::endl;
	std::cout << "Vertex Optimization Min Verts: " << Config::VertexOptimizationMinVerts << std::endl;
	std::cout << std::endl;
	
	if (glDebugMessageCallback) {
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(glDebugCallback, nullptr);
		std::cout << "GL Debug output enabled" << std::endl;
	}

	mVertPath = "shaders/pbr.vert";
	mFragPath = "shaders/pbr.frag";
	mVertStamp = fileStamp(mVertPath);
	mFragStamp = fileStamp(mFragPath);

	std::string vertSrc, fragSrc;
	if (!readTextFile(mVertPath, vertSrc)) { outError = "Failed to read shaders/pbr.vert"; return false; }
	if (!readTextFile(mFragPath, fragSrc)) { outError = "Failed to read shaders/pbr.frag"; return false; }
	std::string err;
	if (!mShader.compileFromSource(vertSrc.c_str(), fragSrc.c_str(), err)) { outError = "Shader error: " + err; return false; }
	
	// Load line shader for bounding box
	std::string lineVertSrc, lineFragSrc;
	if (!readTextFile("shaders/line.vert", lineVertSrc)) { outError = "Failed to read shaders/line.vert"; return false; }
	if (!readTextFile("shaders/line.frag", lineFragSrc)) { outError = "Failed to read shaders/line.frag"; return false; }
	if (!mLineShader.compileFromSource(lineVertSrc.c_str(), lineFragSrc.c_str(), err)) { outError = "Line shader error: " + err; return false; }
	
	// Load sphere impostor shader (with geometry shader)
	std::string sphereImpVertSrc, sphereImpGeomSrc, sphereImpFragSrc;
	if (!readTextFile("shaders/pbr.vert", sphereImpVertSrc)) { outError = "Failed to read shaders/pbr.vert"; return false; }
	if (!readTextFile("shaders/sphere_impostor.geom", sphereImpGeomSrc)) { outError = "Failed to read shaders/sphere_impostor.geom"; return false; }
	if (!readTextFile("shaders/sphere_impostor.frag", sphereImpFragSrc)) { outError = "Failed to read shaders/sphere_impostor.frag"; return false; }
	if (!mSphereImpostorShader.compileFromSource(sphereImpVertSrc.c_str(), sphereImpGeomSrc.c_str(), sphereImpFragSrc.c_str(), err)) { outError = "Sphere impostor shader error: " + err; return false; }
	
	// Load instanced sphere shader
	std::string instSphereVertSrc, instSphereFragSrc;
	if (!readTextFile("shaders/instanced_sphere.vert", instSphereVertSrc)) { outError = "Failed to read shaders/instanced_sphere.vert"; return false; }
	if (!readTextFile("shaders/pbr.frag", instSphereFragSrc)) { outError = "Failed to read shaders/pbr.frag"; return false; }
	if (!mInstancedSphereShader.compileFromSource(instSphereVertSrc.c_str(), instSphereFragSrc.c_str(), err)) { outError = "Instanced sphere shader error: " + err; return false; }
	
	// Load depth-only shader for Early-Z prepass
	std::string depthVertSrc, depthFragSrc;
	if (!readTextFile("shaders/depth_only.vert", depthVertSrc)) { outError = "Failed to read shaders/depth_only.vert"; return false; }
	if (!readTextFile("shaders/depth_only.frag", depthFragSrc)) { outError = "Failed to read shaders/depth_only.frag"; return false; }
	if (!mDepthOnlyShader.compileFromSource(depthVertSrc.c_str(), depthFragSrc.c_str(), err)) { outError = "Depth-only shader error: " + err; return false; }
	if (!mScene.model.loadFromFile(modelPath, err)) { outError = "Failed to load model: " + err; return false; }
	mScene.model.uploadToGPU();
	
	// Initialize UBOs
	mScene.initializeUBOs();
	
	// Initialize occlusion query
	mScene.initializeOcclusionQuery();
	
	// Initialize OpenGL state cache
	mGLStateCache.initialize();

	// Center and scale the model, oriented facing up (+Y) and towards camera (+Z)
	glm::vec3 c = mScene.model.center();
	glm::vec3 size = mScene.model.max() - mScene.model.min();
	float maxAxis = std::max(size.x, std::max(size.y, size.z));
	float s = (maxAxis > 0.0f) ? (1.0f / maxAxis) : 1.0f;
	glm::mat4 T = glm::translate(glm::mat4(1.0f), -c);
	glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(s));
	// No rotation - model keeps its original orientation, centered at origin
	mScene.modelMatrix = S * T;

	mScene.updateBoundingBox();  // Initialize bounding box renderer

	mView.setPerspectiveForAspect(mAspect);
	glm::vec3 startEye = glm::vec3(0.0f, 0.0f, 2.0f);
	glm::vec3 target = glm::vec3(0.0f);
	mView.setLookAt(startEye, target);
	glm::vec3 front = glm::normalize(target - startEye);
	mView.yaw = glm::degrees(std::atan2(front.z, front.x));
	mView.pitch = glm::degrees(std::asin(front.y));
	mView.mouseInitialized = false;
	
	// Initialize profiling
	initializeProfiling();
	
	return true;
}

void Renderer::onResize(int w, int h) {
	mWidth = w; mHeight = h; mAspect = (h > 0) ? (float)w / (float)h : 1.0f;
	mView.camera.setAspect(mAspect);
	glViewport(0, 0, w, h);
}

void Renderer::shutdown() {
	// Clean up profiling queries
	if (mGPUTimingSupported) {
		if (mGPUTimestampQuery[0] != 0) glDeleteQueries(2, mGPUTimestampQuery);
	}
	
	mScene.model.destroyGPU();
	if (mImGuiInitialized) {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		mImGuiInitialized = false;
	}
}

void Renderer::checkShaderHotReload() {
	const double now = glfwGetTime();
	const double minInterval = 0.2; // 200ms debounce
	bool f5Down = glfwGetKey(mWindow, GLFW_KEY_F5) == GLFW_PRESS;
	bool trigger = false;
	if (f5Down && !mPrevF5Down && (now - mLastReloadSec) > minInterval) trigger = true;
	mPrevF5Down = f5Down;

	unsigned long long v = fileStamp(mVertPath);
	unsigned long long f = fileStamp(mFragPath);
	if ((now - mLastReloadSec) > minInterval && v && f && (v != mVertStamp || f != mFragStamp)) trigger = true;

	if (!trigger) return;

	std::string vs, fs, err;
	if (!readTextFile(mVertPath, vs)) return;
	if (!readTextFile(mFragPath, fs)) return;
	Shader newShader;
	if (newShader.compileFromSource(vs.c_str(), fs.c_str(), err)) {
		mShader = std::move(newShader);
		mShader.use();
		mVertStamp = v; mFragStamp = f;
		mLastReloadSec = now;
		std::cout << "Shaders reloaded\n";
	} else {
		std::cerr << "Shader reload error: " << err << "\n";
	}
}

void Renderer::handleInput(float deltaTime) {
	bool imguiWantsInput = false;
	if (mImGuiInitialized) {
		ImGuiIO& io = ImGui::GetIO();
		imguiWantsInput = io.WantCaptureKeyboard || io.WantCaptureMouse;
	}
	
	// Only handle camera input if ImGui doesn't want to capture keyboard/mouse
	if (!imguiWantsInput) {
		mView.handleInput(mWindow, deltaTime);
	}

	// Wireframe toggle on F2 edge (only if not captured by ImGui)
	// Note: Wireframe only applies to regular meshes, not point clouds
	if (!imguiWantsInput) {
		bool f2Down = glfwGetKey(mWindow, GLFW_KEY_F2) == GLFW_PRESS;
		if (f2Down && !mPrevF2Down) {
			mWireframe = !mWireframe;
			// Only apply polygon mode for regular meshes (point clouds ignore wireframe)
			if (!mScene.model.isPointCloud()) {
				glPolygonMode(GL_FRONT_AND_BACK, mWireframe ? GL_LINE : GL_FILL);
			}
		}
		mPrevF2Down = f2Down;
		
		// Camera presets: Ctrl+1-9,0 to save, 1-9,0 to restore
		bool ctrlPressed = (glfwGetKey(mWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || 
		                    glfwGetKey(mWindow, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
		for (int i = 0; i < 10; ++i) {
			int key = (i == 9) ? GLFW_KEY_0 : (GLFW_KEY_1 + i);
			if (glfwGetKey(mWindow, key) == GLFW_PRESS && !imguiWantsInput) {
				if (ctrlPressed) {
					mView.savePreset(i);
				} else {
					mView.restorePreset(i);
				}
			}
		}
		
		// Point size controls: +/- or Page Up/Down
		if (mScene.model.isPointCloud()) {
			float pointSizeStep = 0.5f;
			if (glfwGetKey(mWindow, GLFW_KEY_EQUAL) == GLFW_PRESS || 
			    glfwGetKey(mWindow, GLFW_KEY_PAGE_UP) == GLFW_PRESS) {
				mScene.pointSize = std::min(mScene.pointSize + pointSizeStep, 20.0f);
			}
			if (glfwGetKey(mWindow, GLFW_KEY_MINUS) == GLFW_PRESS || 
			    glfwGetKey(mWindow, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) {
				mScene.pointSize = std::max(mScene.pointSize - pointSizeStep, 0.5f);
			}
		}
	}

	checkShaderHotReload();
}

void Renderer::render() {
	// Track CPU frame time
	static double lastFrameTime = glfwGetTime();
	double frameStartTime = glfwGetTime();
	double cpuFrameTime = (frameStartTime - lastFrameTime) * 1000.0;  // Convert to ms
	lastFrameTime = frameStartTime;
	
	// Reset profiling data
	mProfilingData.drawCalls = 0;
	mProfilingData.triangles = 0;
	mProfilingData.points = 0;
	
	// ImGui NewFrame
	if (mImGuiInitialized) {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	glm::mat4 view = mView.camera.getView();
	glm::mat4 proj = mView.camera.getProjection();
	
	// Start GPU timing query (if supported)
	// Use double-buffered queries: one for start, one for end
	if (mGPUTimingSupported && mGPUTimestampQuery[0] != 0 && mGPUTimestampQuery[1] != 0) {
		// Check if previous frame's query result is available
		GLuint available = 0;
		glGetQueryObjectuiv(mGPUTimestampQuery[1], GL_QUERY_RESULT_AVAILABLE, &available);
		if (available) {
			GLuint64 startTime = 0, endTime = 0;
			glGetQueryObjectui64v(mGPUTimestampQuery[0], GL_QUERY_RESULT, &startTime);
			glGetQueryObjectui64v(mGPUTimestampQuery[1], GL_QUERY_RESULT, &endTime);
			if (endTime > startTime) {
				// Convert nanoseconds to milliseconds
				mProfilingData.gpuFrameTime = (endTime - startTime) / 1e6;
			}
		}
		// Start new frame timing: query at start
		glQueryCounter(mGPUTimestampQuery[0], GL_TIMESTAMP);
	}
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // glClearColor set once in RenderDevice init
	
	// Compute frame state once (view, proj, viewProj, camPos)
	FrameState frameState(view, proj, mView.camera.eye());
	
	// Occlusion culling: test if model is occluded (before main rendering)
	// This should ideally be done in a separate pass before Early-Z prepass
	// For now, we'll test after Early-Z prepass but before main render
	if (mScene.enableOcclusionCulling && !mScene.model.isPointCloud()) {
		mScene.testOcclusion(frameState, mDepthOnlyShader, &mGLStateCache);
	}
	
	// Early-Z depth prepass: render depth buffer first (if enabled)
	// This allows the main pass to skip expensive fragment shader work on occluded fragments
	if (mScene.enableEarlyZPrepass && !mScene.model.isPointCloud()) {
		// Depth-only pass: render only depth buffer, no color writes
		mGLStateCache.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);  // Disable color writes
		mGLStateCache.depthMask(GL_TRUE);   // Enable depth writes (write to depth buffer)
		mGLStateCache.depthFunc(GL_LESS);  // Standard depth test
		
		mScene.drawDepthOnly(mDepthOnlyShader, frameState, &mGLStateCache);
		
		// Re-enable color writes for main pass
		mGLStateCache.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		// Keep depth test enabled for main pass (Early-Z will skip occluded fragments)
		// Use GL_LEQUAL to allow fragments at same depth as prepass (exact matches)
		mGLStateCache.depthFunc(GL_LEQUAL);  // Same or closer passes depth test
		mGLStateCache.depthMask(GL_FALSE);  // Don't write depth in main pass (already written in prepass)
	} else {
		// Normal single-pass rendering: restore default depth state
		mGLStateCache.depthMask(GL_TRUE);   // Enable depth writes
		mGLStateCache.depthFunc(GL_LESS);  // Standard depth test
	}
	
	// Main pass: render with full shading
	// Early-Z will automatically skip fragments that failed depth test in prepass
	mScene.draw(mShader, &mSphereImpostorShader, &mInstancedSphereShader, frameState, mWireframe, &mProfilingData, &mGLStateCache);
	mScene.drawBoundingBox(mLineShader.id(), view, proj);
	
	// End GPU timing query: query at end of frame
	if (mGPUTimingSupported && mGPUTimestampQuery[1] != 0) {
		glQueryCounter(mGPUTimestampQuery[1], GL_TIMESTAMP);
	}

    // Render ImGui UI
    if (mImGuiInitialized) {
        Graphics::UI::drawSceneUI(*this);
        Graphics::UI::drawProfilingUI(*this);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
	
	// Update profiling data with CPU frame time
	mProfilingData.cpuFrameTime = cpuFrameTime;
}

void Renderer::initializeProfiling() {
	// Check if GPU timing is supported (OpenGL 3.3+)
	const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
	if (glVersion) {
		int major = 0, minor = 0;
		sscanf(glVersion, "%d.%d", &major, &minor);
		mGPUTimingSupported = (major > 3 || (major == 3 && minor >= 3));
		
		// Also check for ARB_timer_query extension (for older OpenGL versions)
		const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
		if (extensions && strstr(extensions, "ARB_timer_query")) {
			mGPUTimingSupported = true;
		}
	}
	
	if (mGPUTimingSupported && glQueryCounter) {
		glGenQueries(2, mGPUTimestampQuery);
		if (mGPUTimestampQuery[0] != 0 && mGPUTimestampQuery[1] != 0) {
			mProfilingData.gpuTimingAvailable = true;
			// Queries will be started in render() loop
		}
	}
	
	// Try to get GPU memory info (vendor-specific extensions)
	// NVIDIA: GL_NVX_gpu_memory_info
	// AMD: GL_ATI_meminfo
	// Intel: No standard extension
	const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
	const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
	if (extensions) {
		if (strstr(extensions, "GL_NVX_gpu_memory_info") && vendor && strstr(vendor, "NVIDIA")) {
			// NVIDIA GPU memory tracking available
			// We can query it later if needed
		}
	}
}

void Renderer::updateProfiling(double cpuFrameTime) {
	mProfilingData.cpuFrameTime = cpuFrameTime;
}

// UI moved to Graphics/UI/Inspector

bool Renderer::shouldClose() const { return glfwWindowShouldClose(mWindow); }

} // namespace Graphics
