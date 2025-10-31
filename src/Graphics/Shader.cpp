#include "Shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Graphics {

static bool compileShaderStage(GLenum type, const char* src, unsigned int& outId, std::string& outErr) {
	outId = glCreateShader(type);
	glShaderSource(outId, 1, &src, nullptr);
	glCompileShader(outId);
	int success = 0;
	glGetShaderiv(outId, GL_COMPILE_STATUS, &success);
	if (!success) {
		char log[2048];
		glGetShaderInfoLog(outId, sizeof(log), nullptr, log);
		outErr.assign(log);
		glDeleteShader(outId);
		outId = 0;
		return false;
	}
	return true;
}

Shader::~Shader() {
	if (mProgram != 0) {
		glDeleteProgram(mProgram);
	}
}

Shader::Shader(Shader&& other) noexcept {
	mProgram = other.mProgram;
	other.mProgram = 0;
	mUniformLocationCache = std::move(other.mUniformLocationCache);
}

Shader& Shader::operator=(Shader&& other) noexcept {
	if (this != &other) {
		if (mProgram) glDeleteProgram(mProgram);
		mProgram = other.mProgram;
		other.mProgram = 0;
		mUniformLocationCache = std::move(other.mUniformLocationCache);
	}
	return *this;
}

bool Shader::compileFromSource(const char* vertexSrc, const char* fragmentSrc, std::string& outError) {
	unsigned int vs = 0, fs = 0;
	if (!compileShaderStage(GL_VERTEX_SHADER, vertexSrc, vs, outError)) return false;
	if (!compileShaderStage(GL_FRAGMENT_SHADER, fragmentSrc, fs, outError)) {
		glDeleteShader(vs);
		return false;
	}

	mProgram = glCreateProgram();
	glAttachShader(mProgram, vs);
	glAttachShader(mProgram, fs);
	glLinkProgram(mProgram);

	int linked = 0;
	glGetProgramiv(mProgram, GL_LINK_STATUS, &linked);
	glDeleteShader(vs);
	glDeleteShader(fs);
	if (!linked) {
		char log[2048];
		glGetProgramInfoLog(mProgram, sizeof(log), nullptr, log);
		outError.assign(log);
		glDeleteProgram(mProgram);
		mProgram = 0;
		return false;
	}

	// Bind UBOs for OpenGL 3.3 compatibility (no layout binding qualifier)
	bindUBOs();

	mUniformLocationCache.clear();
	return true;
}

bool Shader::compileFromSource(const char* vertexSrc, const char* geometrySrc, const char* fragmentSrc, std::string& outError) {
	unsigned int vs = 0, gs = 0, fs = 0;
	if (!compileShaderStage(GL_VERTEX_SHADER, vertexSrc, vs, outError)) return false;
	if (!compileShaderStage(GL_GEOMETRY_SHADER, geometrySrc, gs, outError)) {
		glDeleteShader(vs);
		return false;
	}
	if (!compileShaderStage(GL_FRAGMENT_SHADER, fragmentSrc, fs, outError)) {
		glDeleteShader(vs);
		glDeleteShader(gs);
		return false;
	}

	mProgram = glCreateProgram();
	glAttachShader(mProgram, vs);
	glAttachShader(mProgram, gs);
	glAttachShader(mProgram, fs);
	glLinkProgram(mProgram);

	int linked = 0;
	glGetProgramiv(mProgram, GL_LINK_STATUS, &linked);
	glDeleteShader(vs);
	glDeleteShader(gs);
	glDeleteShader(fs);
	if (!linked) {
		char log[2048];
		glGetProgramInfoLog(mProgram, sizeof(log), nullptr, log);
		outError.assign(log);
		glDeleteProgram(mProgram);
		mProgram = 0;
		return false;
	}

	// Bind UBOs for OpenGL 3.3 compatibility (no layout binding qualifier)
	bindUBOs();

	mUniformLocationCache.clear();
	return true;
}

void Shader::use() const {
	static unsigned int sActiveProgram = 0;
	if (sActiveProgram != mProgram) {
		glUseProgram(mProgram);
		sActiveProgram = mProgram;
	}
}

int Shader::getUniformLocation(const std::string& name) const {
	auto it = mUniformLocationCache.find(name);
	if (it != mUniformLocationCache.end()) return it->second;
	int loc = glGetUniformLocation(mProgram, name.c_str());
	mUniformLocationCache.emplace(name, loc);
	return loc;
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const {
	int loc = getUniformLocation(name);
	if (loc < 0) return;
	glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setMat3(const std::string& name, const glm::mat3& value) const {
	int loc = getUniformLocation(name);
	if (loc < 0) return;
	glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
	int loc = getUniformLocation(name);
	if (loc < 0) return;
	glUniform3fv(loc, 1, &value.x);
}

void Shader::setFloat(const std::string& name, float value) const {
	int loc = getUniformLocation(name);
	if (loc < 0) return;
	glUniform1f(loc, value);
}

void Shader::setInt(const std::string& name, int value) const {
	int loc = getUniformLocation(name);
	if (loc < 0) return;
	glUniform1i(loc, value);
}

void Shader::setBool(const std::string& name, bool value) const {
	setInt(name, value ? 1 : 0);
}

void Shader::bindUBOs() {
	if (mProgram == 0) return;
	
	// Bind UBO blocks to binding points (OpenGL 3.3 compatibility)
	// MatricesUBO -> binding 0
	unsigned int matricesIndex = glGetUniformBlockIndex(mProgram, "MatricesUBO");
	if (matricesIndex != GL_INVALID_INDEX) {
		glUniformBlockBinding(mProgram, matricesIndex, 0);
	}
	
	// MaterialUBO -> binding 1
	unsigned int materialIndex = glGetUniformBlockIndex(mProgram, "MaterialUBO");
	if (materialIndex != GL_INVALID_INDEX) {
		glUniformBlockBinding(mProgram, materialIndex, 1);
	}
	
	// LightingUBO -> binding 2
	unsigned int lightingIndex = glGetUniformBlockIndex(mProgram, "LightingUBO");
	if (lightingIndex != GL_INVALID_INDEX) {
		glUniformBlockBinding(mProgram, lightingIndex, 2);
	}
}

} // namespace Graphics
