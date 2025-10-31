#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Graphics {

/// OpenGL shader program manager. Handles compilation, linking, and uniform setting.
/// Supports vertex/fragment shaders and vertex/geometry/fragment shader combinations.
/// Automatically binds UBOs to binding points for OpenGL 3.3 compatibility.
class Shader {
public:
	Shader() = default;
	~Shader();

	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;
	Shader(Shader&& other) noexcept;
	Shader& operator=(Shader&& other) noexcept;

	/// Compile shader from source strings. Creates and links a vertex/fragment shader program.
	/// @param vertexSrc Vertex shader source code
	/// @param fragmentSrc Fragment shader source code
	/// @param outError Error message if compilation/linking fails
	/// @return true if successful, false on error
	bool compileFromSource(const char* vertexSrc, const char* fragmentSrc, std::string& outError);
	
	/// Compile shader from source strings. Creates and links a vertex/geometry/fragment shader program.
	/// @param vertexSrc Vertex shader source code
	/// @param geometrySrc Geometry shader source code
	/// @param fragmentSrc Fragment shader source code
	/// @param outError Error message if compilation/linking fails
	/// @return true if successful, false on error
	bool compileFromSource(const char* vertexSrc, const char* geometrySrc, const char* fragmentSrc, std::string& outError);
	
	/// Make this shader active (equivalent to glUseProgram).
	/// Also binds UBOs to their binding points.
	void use() const;
	
	/// Get the OpenGL program ID.
	/// @return GL program handle
	unsigned int id() const { return mProgram; }

	/// Set a mat4 uniform. Returns -1 if uniform not found (no error).
	/// @param name Uniform name in shader
	/// @param value Matrix value
	void setMat4(const std::string& name, const glm::mat4& value) const;
	
	/// Set a mat3 uniform. Returns -1 if uniform not found (no error).
	/// @param name Uniform name in shader
	/// @param value Matrix value
	void setMat3(const std::string& name, const glm::mat3& value) const;
	
	/// Set a vec3 uniform. Returns -1 if uniform not found (no error).
	/// @param name Uniform name in shader
	/// @param value Vector value
	void setVec3(const std::string& name, const glm::vec3& value) const;
	
	/// Set a float uniform. Returns -1 if uniform not found (no error).
	/// @param name Uniform name in shader
	/// @param value Float value
	void setFloat(const std::string& name, float value) const;
	
	/// Set an int uniform. Returns -1 if uniform not found (no error).
	/// @param name Uniform name in shader
	/// @param value Integer value
	void setInt(const std::string& name, int value) const;
	
	/// Set a bool uniform (stored as int in GLSL). Returns -1 if uniform not found (no error).
	/// @param name Uniform name in shader
	/// @param value Boolean value
	void setBool(const std::string& name, bool value) const;

private:
	void bindUBOs();  // Bind UBO blocks to binding points (OpenGL 3.3 compatibility)
	unsigned int mProgram = 0;
	mutable std::unordered_map<std::string, int> mUniformLocationCache;
	int getUniformLocation(const std::string& name) const;
};

} // namespace Graphics
