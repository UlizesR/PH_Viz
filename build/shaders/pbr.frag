#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec3 vColor;  // Vertex color from PLY RGB
in float vScalar;  // Scalar value for color mapping

out vec4 FragColor;

// Uniform Buffer Objects (more efficient than individual uniforms)
// Note: OpenGL 3.3 doesn't support 'binding' in layout, so we bind via glUniformBlockBinding
layout(std140) uniform MatricesUBO {
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 viewProj;
	vec4 camPos;
};

layout(std140) uniform MaterialUBO {
	vec4 albedo;        // albedo.xyz + metallic in .w
	vec4 params;        // roughness, ao, colorMode (as float), scalarMin
	vec4 scalars;       // scalarMax + padding
	vec4 skyColor;
	vec4 groundColor;
};

layout(std140) uniform LightingUBO {
	vec4 lightDir;      // lightDir.xyz + padding
	vec4 lightColor;     // lightColor.xyz + padding
};

// Wireframe (not in UBO as it's mesh-specific)
uniform float uWireframe;     // 0 or 1
uniform vec3 uWireframeColor; // orange

const float PI = 3.14159265359;

// Colormap function (maps scalar 0..1 to RGB)
vec3 scalarToColor(float t) {
	t = clamp(t, 0.0, 1.0);
	// Simple colormap: blue -> cyan -> green -> yellow -> red
	if (t < 0.25) {
		return mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 1.0), t * 4.0);  // blue to cyan
	} else if (t < 0.5) {
		return mix(vec3(0.0, 1.0, 1.0), vec3(0.0, 1.0, 0.0), (t - 0.25) * 4.0);  // cyan to green
	} else if (t < 0.75) {
		return mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 1.0, 0.0), (t - 0.5) * 4.0);  // green to yellow
	} else {
		return mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.75) * 4.0);  // yellow to red
	}
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;
	return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx1 = geometrySchlickGGX(NdotV, roughness);
	float ggx2 = geometrySchlickGGX(NdotL, roughness);
	return ggx1 * ggx2;
}

void main() {
	// Wireframe mode: output orange color
	if (uWireframe > 0.5) {
		FragColor = vec4(uWireframeColor, 1.0);
		return;
	}

	// Extract values from UBOs
	vec3 camPos3 = camPos.xyz;
	vec3 lightDir3 = normalize(lightDir.xyz);
	vec3 lightColor3 = lightColor.xyz;
	vec3 albedo3 = albedo.xyz;
	float metallicVal = albedo.w;
	float roughnessVal = params.x;
	float aoVal = params.y;
	int colorModeVal = int(params.z);
	float scalarMinVal = params.w;
	float scalarMaxVal = scalars.x;
	vec3 skyColor3 = skyColor.xyz;
	vec3 groundColor3 = groundColor.xyz;

	// Determine base color based on color mode
	vec3 baseColor = albedo3;
	if (colorModeVal == 1) {
		// VertexRGB mode: use vertex color from PLY RGB
		baseColor = vColor;
	} else if (colorModeVal == 2) {
		// Scalar mode: map scalar to colormap
		float t = (vScalar - scalarMinVal) / max(scalarMaxVal - scalarMinVal, 0.001);
		baseColor = scalarToColor(t);
	}
	// else colorModeVal == 0: Uniform (use albedo3)

	vec3 N = normalize(vNormal);
	vec3 V = normalize(camPos3 - vWorldPos);
	vec3 L = lightDir3;
	vec3 H = normalize(V + L);

	float NdotL = max(dot(N, L), 0.0);
	float NdotV = max(dot(N, V), 0.0);

	vec3 F0 = mix(vec3(0.04), baseColor, metallicVal);
	vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
	float D = distributionGGX(N, H, max(0.045, roughnessVal));
	float G = geometrySmith(N, V, L, max(0.045, roughnessVal));

	vec3 numerator = D * G * F;
	float denom = max(4.0 * NdotV * NdotL, 0.001);
	vec3 specular = numerator / denom;

	vec3 kS = F;
	vec3 kD = (vec3(1.0) - kS) * (1.0 - metallicVal);

	vec3 irradiance = lightColor3 * NdotL;
	vec3 diffuse = (baseColor / PI) * irradiance;

	// Hemisphere ambient
	float t = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
	vec3 hemi = mix(groundColor3, skyColor3, t);
	vec3 ambient = hemi * baseColor;

	vec3 color = ambient + (kD * diffuse + specular * irradiance) * aoVal;

	// Gamma correction
	color = pow(color, vec3(1.0/2.2));
	FragColor = vec4(color, 1.0);
}
