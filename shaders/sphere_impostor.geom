#version 330 core
layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

// Uniform Buffer Objects
// Note: OpenGL 3.3 doesn't support 'binding' in layout, so we bind via glUniformBlockBinding
layout(std140) uniform MatricesUBO {
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 viewProj;
	vec4 camPos;
};

uniform float uPointSize;

in vec3 vWorldPos[];
in vec3 vNormal[];
in vec2 vUV[];
in vec3 vColor[];
in float vScalar[];

out vec3 gWorldPos;
out vec3 gNormal;
out vec2 gUV;
out vec3 gColor;
out float gScalar;
out vec2 gSphereCoord;  // Coordinates on the billboard quad (-1 to 1)

void main() {
	vec4 centerPos = gl_in[0].gl_Position;
	vec3 worldCenter = vWorldPos[0];
	
	// Calculate billboard size in clip space
	// Size is in pixels, need to convert to clip space
	vec4 projCenter = viewProj * vec4(worldCenter, 1.0);
	float w = projCenter.w;
	float size = uPointSize / w * 0.002;  // Scale factor for clip space
	
	// Generate billboard quad in clip space (always facing camera)
	// Bottom-left
	gl_Position = vec4(centerPos.xy + vec2(-size, -size), centerPos.zw);
	gWorldPos = worldCenter;
	gNormal = vNormal[0];
	gUV = vUV[0];
	gColor = vColor[0];
	gScalar = vScalar[0];
	gSphereCoord = vec2(-1.0, -1.0);
	EmitVertex();
	
	// Bottom-right
	gl_Position = vec4(centerPos.xy + vec2(size, -size), centerPos.zw);
	gWorldPos = worldCenter;
	gNormal = vNormal[0];
	gUV = vUV[0];
	gColor = vColor[0];
	gScalar = vScalar[0];
	gSphereCoord = vec2(1.0, -1.0);
	EmitVertex();
	
	// Top-left
	gl_Position = vec4(centerPos.xy + vec2(-size, size), centerPos.zw);
	gWorldPos = worldCenter;
	gNormal = vNormal[0];
	gUV = vUV[0];
	gColor = vColor[0];
	gScalar = vScalar[0];
	gSphereCoord = vec2(-1.0, 1.0);
	EmitVertex();
	
	// Top-right
	gl_Position = vec4(centerPos.xy + vec2(size, size), centerPos.zw);
	gWorldPos = worldCenter;
	gNormal = vNormal[0];
	gUV = vUV[0];
	gColor = vColor[0];
	gScalar = vScalar[0];
	gSphereCoord = vec2(1.0, 1.0);
	EmitVertex();
	
	EndPrimitive();
}

