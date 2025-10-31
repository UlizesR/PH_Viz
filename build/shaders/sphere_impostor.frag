#version 330 core
in vec3 gWorldPos;
in vec3 gNormal;
in vec2 gUV;
in vec3 gColor;
in float gScalar;
in vec2 gSphereCoord;  // Coordinates on billboard quad (-1 to 1)

out vec4 FragColor;

// Uniform Buffer Objects
// Note: OpenGL 3.3 doesn't support 'binding' in layout, so we bind via glUniformBlockBinding
layout(std140) uniform MatricesUBO {
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 viewProj;
	vec4 camPos;
};

layout(std140) uniform MaterialUBO {
	vec4 albedo;
	vec4 params;
	vec4 scalars;
	vec4 skyColor;
	vec4 groundColor;
};

layout(std140) uniform LightingUBO {
	vec4 lightDir;
	vec4 lightColor;
};

const float PI = 3.14159265359;

// Colormap function (maps scalar 0..1 to RGB)
vec3 scalarToColor(float t) {
	t = clamp(t, 0.0, 1.0);
	if (t < 0.25) {
		return mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 1.0), t * 4.0);
	} else if (t < 0.5) {
		return mix(vec3(0.0, 1.0, 1.0), vec3(0.0, 1.0, 0.0), (t - 0.25) * 4.0);
	} else if (t < 0.75) {
		return mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 1.0, 0.0), (t - 0.5) * 4.0);
	} else {
		return mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.75) * 4.0);
	}
}

// PBR functions
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
	// Discard pixels outside the sphere circle
	float dist2 = dot(gSphereCoord, gSphereCoord);
	if (dist2 > 1.0) {
		discard;
	}
	
	// Compute sphere normal from sphereCoord
	// This gives us a proper sphere normal for lighting
	float z = sqrt(1.0 - dist2);
	vec3 sphereNormal = normalize(vec3(gSphereCoord, z));
	
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
	
	// Determine base color
	vec3 baseColor = albedo3;
	if (colorModeVal == 1) {
		baseColor = gColor;
	} else if (colorModeVal == 2) {
		float t = (gScalar - scalarMinVal) / max(scalarMaxVal - scalarMinVal, 0.001);
		baseColor = scalarToColor(t);
	}
	
	// Use sphere normal for lighting calculations
	vec3 N = sphereNormal;  // Sphere surface normal
	vec3 V = normalize(camPos3 - gWorldPos);
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

