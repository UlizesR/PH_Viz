#version 330 core
layout(location = 0) in vec3 aPos;      // Sphere mesh vertex position
layout(location = 1) in vec3 aNormal;  // Sphere mesh vertex normal
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aColor;
layout(location = 4) in float aScalar;

// Instance attributes
layout(location = 5) in vec3 aInstancePos;   // Instance position offset
layout(location = 6) in vec3 aInstanceColor; // Instance color
layout(location = 7) in float aInstanceScalar; // Instance scalar

// Uniform Buffer Objects
// Note: OpenGL 3.3 doesn't support 'binding' in layout, so we bind via glUniformBlockBinding
layout(std140) uniform MatricesUBO {
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 viewProj;
	vec4 camPos;
};

uniform float uSphereRadius;  // Radius scaling for spheres

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vColor;
out float vScalar;

void main() {
	// Transform sphere vertex by radius and translate to instance position
	vec3 worldPos = aInstancePos + aPos * uSphereRadius;
	vec4 worldPos4 = model * vec4(worldPos, 1.0);
	
	vWorldPos = worldPos4.xyz;
	
	// Normal matrix for sphere normal
	mat3 normalMat = mat3(transpose(inverse(model)));
	vNormal = normalize(normalMat * aNormal);
	
	vUV = aUV;
	vColor = aInstanceColor;  // Use instance color instead of vertex color
	vScalar = aInstanceScalar;  // Use instance scalar
	
	// Use cached viewProj for efficiency
	gl_Position = viewProj * worldPos4;
}

