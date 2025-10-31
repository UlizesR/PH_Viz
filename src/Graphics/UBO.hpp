#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>

namespace Graphics {

// UBO structure layouts (std140 alignment - each member must be 16-byte aligned)
// Matrices UBO (binding = 0)
struct MatricesUBO {
	glm::mat4 model;      // 64 bytes (16-byte aligned)
	glm::mat4 view;       // 64 bytes (16-byte aligned)
	glm::mat4 proj;       // 64 bytes (16-byte aligned)
	glm::mat4 viewProj;   // 64 bytes (16-byte aligned)
	glm::vec4 camPos;     // 16 bytes (16-byte aligned) - use vec4 for proper alignment
};
static_assert(sizeof(MatricesUBO) == 64 * 4 + 16, "MatricesUBO size mismatch");

// Material UBO (binding = 1)
struct MaterialUBO {
	glm::vec4 albedo;     // 16 bytes (albedo.xyz + metallic in .w)
	glm::vec4 params;     // 16 bytes (roughness, ao, colorMode as int, scalarMin)
	glm::vec4 scalars;    // 16 bytes (scalarMax + padding)
	glm::vec4 skyColor;   // 16 bytes
	glm::vec4 groundColor; // 16 bytes
};
static_assert(sizeof(MaterialUBO) == 16 * 5, "MaterialUBO size mismatch");

// Lighting UBO (binding = 2)
struct LightingUBO {
	glm::vec4 lightDir;   // 16 bytes (includes padding)
	glm::vec4 lightColor; // 16 bytes (includes padding)
};
static_assert(sizeof(LightingUBO) == 16 + 16, "LightingUBO size mismatch");

// UBO wrapper class
class UniformBuffer {
public:
	UniformBuffer() = default;
	~UniformBuffer() { destroy(); }
	
	UniformBuffer(const UniformBuffer&) = delete;
	UniformBuffer& operator=(const UniformBuffer&) = delete;
	
	UniformBuffer(UniformBuffer&& other) noexcept : mBuffer(other.mBuffer) {
		other.mBuffer = 0;
	}
	
	UniformBuffer& operator=(UniformBuffer&& other) noexcept {
		if (this != &other) {
			destroy();
			mBuffer = other.mBuffer;
			other.mBuffer = 0;
		}
		return *this;
	}
	
	void create() {
		if (mBuffer == 0) {
			glGenBuffers(1, &mBuffer);
		}
	}
	
	void destroy() {
		if (mBuffer != 0) {
			glDeleteBuffers(1, &mBuffer);
			mBuffer = 0;
		}
	}
	
	void bind(GLenum target) const {
		if (mBuffer != 0) {
			glBindBuffer(target, mBuffer);
		}
	}
	
	void setData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) const {
		if (mBuffer != 0) {
			glBindBuffer(target, mBuffer);
			glBufferData(target, size, data, usage);
		}
	}
	
	void updateData(GLintptr offset, GLsizeiptr size, const void* data) const {
		if (mBuffer != 0) {
			glBindBuffer(GL_UNIFORM_BUFFER, mBuffer);
			glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
		}
	}
	
	void bindBase(GLuint bindingPoint) const {
		if (mBuffer != 0) {
			glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, mBuffer);
		}
	}
	
	unsigned int id() const { return mBuffer; }
	bool valid() const { return mBuffer != 0; }

private:
	unsigned int mBuffer = 0;
};

} // namespace Graphics

