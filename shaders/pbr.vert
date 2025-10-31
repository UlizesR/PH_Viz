#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aColor;  // Vertex color
layout(location = 4) in float aScalar;  // Scalar value

// Uniform Buffer Objects (more efficient than individual uniforms)
// Note: OpenGL 3.3 doesn't support 'binding' in layout, so we bind via glUniformBlockBinding
layout(std140) uniform MatricesUBO {
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 viewProj;
	vec4 camPos;
};

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vColor;  // Pass vertex color to fragment shader
out float vScalar;  // Pass scalar to fragment shader

void main() {
	vec4 worldPos = model * vec4(aPos, 1.0);
	vWorldPos = worldPos.xyz;
	// Normal matrix = inverse transpose of upper-left 3x3
	mat3 normalMat = mat3(transpose(inverse(model)));
	vNormal = normalize(normalMat * aNormal);
	vUV = aUV;
	vColor = aColor;
	vScalar = aScalar;
	// Use cached viewProj for efficiency
	gl_Position = viewProj * worldPos;
}
