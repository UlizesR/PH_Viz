#pragma once

#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Graphics {

class RenderDevice {
public:
	RenderDevice() = default;
	~RenderDevice() { shutdown(); }
	RenderDevice(const RenderDevice&) = delete;
	RenderDevice& operator=(const RenderDevice&) = delete;

	bool initialize(std::string& outError) {
		glfwSetErrorCallback([](int code, const char* desc){ (void)code; fprintf(stderr, "GLFW Error: %s\n", desc); });
		if (!glfwInit()) { outError = "Failed to initialize GLFW"; return false; }
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
		mWindow = glfwCreateWindow(1280, 720, "PH_Viz", nullptr, nullptr);
		if (!mWindow) { outError = "Failed to create GLFW window"; glfwTerminate(); return false; }
		glfwMakeContextCurrent(mWindow);
		glfwSwapInterval(1);
		glfwSetFramebufferSizeCallback(mWindow, [](GLFWwindow* /*w*/, int width, int height){ glViewport(0, 0, width, height); });
		glfwGetFramebufferSize(mWindow, &mFbW, &mFbH);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { outError = "Failed to initialize GLAD"; return false; }
		glEnable(GL_DEPTH_TEST);
		glClearDepth(1.0);
		glDepthFunc(GL_LESS);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
		glEnable(GL_FRAMEBUFFER_SRGB);
		glClearColor(0.08f, 0.09f, 0.11f, 1.0f);  // Set once during init, not every frame
		return true;
	}

	void shutdown() {
		if (mWindow) { glfwDestroyWindow(mWindow); mWindow = nullptr; glfwTerminate(); }
	}

	GLFWwindow* window() const { return mWindow; }
	int fbWidth() const { return mFbW; }
	int fbHeight() const { return mFbH; }
	bool shouldClose() const { return glfwWindowShouldClose(mWindow); }
	void swap() { glfwSwapBuffers(mWindow); }
	void poll() { glfwPollEvents(); glfwGetFramebufferSize(mWindow, &mFbW, &mFbH); }

private:
	GLFWwindow* mWindow = nullptr;
	int mFbW = 0, mFbH = 0;
};

} // namespace Graphics
