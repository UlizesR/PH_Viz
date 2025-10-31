#include <iostream>
#include <string>

#include "Graphics/RenderDevice.hpp"
#include "Graphics/Renderer.h"

int main(int argc, char** argv) {
	// Supports both meshes (.obj, .ply, .off with faces) and point clouds (.ply, .off without faces)
	std::string modelPath = (argc >= 2) ? std::string(argv[1]) : std::string("../assets/bunny/data/bun315.ply");

	Graphics::RenderDevice device;
	{
		std::string err;
		if (!device.initialize(err)) { std::cerr << err << "\n"; return 1; }
	}

	Graphics::Renderer renderer;
	{
		std::string err;
		if (!renderer.initializeWithContext(device.window(), modelPath, err)) {
			std::cerr << err << "\n";
			return 1;
		}
	}

	double last = glfwGetTime();
	while (!device.shouldClose()) {
		double now = glfwGetTime();
		float dt = static_cast<float>(now - last);
		last = now;

		renderer.handleInput(dt);
		renderer.render();
		device.swap();
		device.poll();
	}

	renderer.shutdown();
	device.shutdown();
	return 0;
}
