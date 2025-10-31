#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aColor;  // Vertex color
layout(location = 4) in float aScalar;  // Scalar value

// Uniform Buffer Objects (alternative to individual uniforms - more efficient)
layout(std140, binding = 0) uniform MatricesUBO {
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 viewProj;
	vec4 camPos;
};

// Can still use individual uniforms if needed for shaders that don't use UBOs yet
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vColor;
out float vScalar;

void main() {
	// Use UBO if available, fallback to individual uniforms
	mat4 m = (model[0][0] != 0.0) ? model : uModel;
	mat4 v = (view[0][0] != 0.0) ? view : uView;
	mat4 p = (proj[0][0] != 0.0) ? proj : uProj;
	
	vec4 worldPos = m * vec4(aPos, 1.0);
	vWorldPos = worldPos.xyz;
	
	// Normal matrix = inverse transpose of upper-left 3x3
	mat3 normalMat = mat3(transpose(inverse(m)));
	vNormal = normalize(normalMat * aNormal);
	vUV = aUV;
	vColor = aColor;
	vScalar = aScalar;
	gl_Position = p * v * worldPos;
}

