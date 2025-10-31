#version 330 core
layout(location = 0) in vec3 aPos;

// Uniform Buffer Objects
layout(std140) uniform MatricesUBO {
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 viewProj;
	vec4 camPos;
};

void main() {
	// Transform position and output to clip space
	vec4 worldPos = model * vec4(aPos, 1.0);
	// Use cached viewProj for efficiency
	gl_Position = viewProj * worldPos;
}

