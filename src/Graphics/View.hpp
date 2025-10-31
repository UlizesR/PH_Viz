#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "Camera.hpp"
#include "RenderUtils.hpp"

namespace Graphics {

class View {
public:
	Camera3D camera;

	float moveSpeed = 2.0f;
	float mouseSensitivity = 0.12f;
	float yaw = -90.0f;
	float pitch = 0.0f;
	bool mouseInitialized = false;
	double lastX = 0.0, lastY = 0.0;
	bool cursorCaptured = false;

	// Camera presets
	std::vector<CameraPreset> presets;
	static constexpr int MAX_PRESETS = 10;

	void setPerspectiveForAspect(float aspect) { camera.setPerspective(45.0f, aspect, 0.1f, 100.0f); }
	void setLookAt(const glm::vec3& eye, const glm::vec3& target) { camera.setLookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f)); }
	
	void savePreset(int slot) {
		if (slot < 0 || slot >= MAX_PRESETS) return;
		if (presets.size() <= static_cast<size_t>(slot)) {
			presets.resize(slot + 1);
		}
		glm::vec3 eye = camera.eye();
		glm::vec3 target = camera.target();
		glm::vec3 up = camera.up();
		presets[slot].save(eye, target, up, yaw, pitch);
	}
	
	void restorePreset(int slot) {
		if (slot < 0 || static_cast<size_t>(slot) >= presets.size()) return;
		const CameraPreset& p = presets[slot];
		glm::vec3 eye, target, up;
		float y, pitchVal;
		p.restore(eye, target, up, y, pitchVal);
		camera.setLookAt(eye, target, up);
		yaw = y;
		pitch = pitchVal;
	}

	void handleInput(GLFWwindow* window, float deltaTime) {
		glm::vec3 up(0.0f, 1.0f, 0.0f);
		glm::vec3 eye = camera.eye();

		int left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
		if (left == GLFW_PRESS) {
			if (!cursorCaptured) { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); cursorCaptured = true; }
			double x, y; glfwGetCursorPos(window, &x, &y);
			if (!mouseInitialized) { lastX = x; lastY = y; mouseInitialized = true; }
			else {
				double dx = x - lastX, dy = y - lastY; lastX = x; lastY = y;
				const double dead = 0.4;
				if (std::abs(dx) > dead || std::abs(dy) > dead) {
					yaw += static_cast<float>(dx) * mouseSensitivity;
					pitch -= static_cast<float>(dy) * mouseSensitivity;
					if (pitch > 89.0f) pitch = 89.0f;
					if (pitch < -89.0f) pitch = -89.0f;
				}
			}
		} else {
			if (cursorCaptured) { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); cursorCaptured = false; }
			mouseInitialized = false;
		}

		glm::vec3 front;
		float yr = glm::radians(yaw), pr = glm::radians(pitch);
		front.x = std::cos(yr) * std::cos(pr);
		front.y = std::sin(pr);
		front.z = std::sin(yr) * std::cos(pr);
		front = glm::normalize(front);
		glm::vec3 right = glm::normalize(glm::cross(front, up));

		float speedFactor = 1.0f;
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) speedFactor *= 4.0f;
		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) speedFactor *= 0.25f;
		float spd = moveSpeed * speedFactor * deltaTime;

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) eye += front * spd;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) eye -= front * spd;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) eye -= right * spd;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) eye += right * spd;
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) eye += up * spd;
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) eye -= up * spd;

		camera.setLookAt(eye, eye + front, up);
	}
};

} // namespace Graphics
