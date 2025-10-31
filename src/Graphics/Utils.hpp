#pragma once

#include <cstdint>
#include <cstring>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace Graphics {

// ============================================================================
// GL Resource Wrappers (RAII for OpenGL objects)
// ============================================================================

using GlId = unsigned int;

class GlBuffer {
public:
	GlBuffer() = default;
	~GlBuffer() { destroy(); }
	GlBuffer(const GlBuffer&) = delete;
	GlBuffer& operator=(const GlBuffer&) = delete;
	GlBuffer(GlBuffer&& other) noexcept { mId = other.mId; other.mId = 0; }
	GlBuffer& operator=(GlBuffer&& other) noexcept { if (this != &other) { destroy(); mId = other.mId; other.mId = 0; } return *this; }

	void create() { if (mId == 0) glGenBuffers(1, &mId); }
	void destroy() { if (mId) { glDeleteBuffers(1, &mId); mId = 0; } }
	bool valid() const { return mId != 0; }
	GlId id() const { return mId; }

	void bind(unsigned int target) const { glBindBuffer(target, mId); }
	void setData(unsigned int target, std::intptr_t size, const void* data, unsigned int usage) const { glBufferData(target, size, data, usage); }
	void updateData(unsigned int target, std::intptr_t offset, std::intptr_t size, const void* data) const { glBufferSubData(target, offset, size, data); }
	void bindBase(unsigned int target, unsigned int index) const { glBindBufferBase(target, index, mId); }

private:
	GlId mId = 0;
};

class GlVertexArray {
public:
	GlVertexArray() = default;
	~GlVertexArray() { destroy(); }
	GlVertexArray(const GlVertexArray&) = delete;
	GlVertexArray& operator=(const GlVertexArray&) = delete;
	GlVertexArray(GlVertexArray&& other) noexcept { mId = other.mId; other.mId = 0; }
	GlVertexArray& operator=(GlVertexArray&& other) noexcept { if (this != &other) { destroy(); mId = other.mId; other.mId = 0; } return *this; }

	void create() { if (mId == 0) glGenVertexArrays(1, &mId); }
	void destroy() { if (mId) { glDeleteVertexArrays(1, &mId); mId = 0; } }
	bool valid() const { return mId != 0; }
	GlId id() const { return mId; }

	void bind() const { glBindVertexArray(mId); }

private:
	GlId mId = 0;
};

// ============================================================================
// Configuration Constants
// ============================================================================

namespace Config {
static constexpr unsigned int MinVerticesForThreading = 10000;
static constexpr unsigned int MinMeshesForThreading   = 2;
static constexpr unsigned int PointCloudMinPointsForOctree = 100000;
static constexpr unsigned int OctreeMaxDepth               = 12;
static constexpr unsigned int OctreePointsPerNode          = 1000;
static constexpr unsigned int VertexOptimizationMinVerts   = 10000;
} // namespace Config

namespace Half {
inline uint16_t floatToHalf(float f) {
	uint32_t bits;
	std::memcpy(&bits, &f, sizeof(float));
	uint32_t sign = ((bits >> 31) << 15) & 0x8000;
	uint32_t exp = ((bits >> 23) & 0xFF);
	uint32_t mantissa = (bits >> 13) & 0x3FF;
	if (exp == 0xFF) {
		if (mantissa == 0) return static_cast<uint16_t>(sign | 0x7C00);
		return static_cast<uint16_t>(sign | 0x7E00);
	}
	if (exp == 0) return static_cast<uint16_t>(sign);
	int32_t exp32 = static_cast<int32_t>(exp) - 127 + 15;
	if (exp32 > 31) return static_cast<uint16_t>(sign | 0x7C00);
	if (exp32 < 0)  return static_cast<uint16_t>(sign);
	return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exp32) << 10) | mantissa);
}
} // namespace Half

struct OptimizedVertex {
	uint16_t pos[3];
	float    normal[3];
	uint16_t uv[2];
	float    color[3];
	float    scalar;
};

// OpenGL state cache to avoid redundant state changes
class GLStateCache {
public:
	GLStateCache() = default;
	~GLStateCache() = default;
	GLStateCache(const GLStateCache&) = delete;
	GLStateCache& operator=(const GLStateCache&) = delete;

	void initialize();
	void reset();

	// Depth testing
	void depthFunc(unsigned int func);
	void depthMask(unsigned char enabled);
	void enableDepthTest(bool enable = true);

	// Color writes
	void colorMask(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

	// Face culling
	void enableCullFace(bool enable = true);

	// Blending
	void enableBlend(bool enable = true);

private:
	int mCurrentProgram = -1;
	int mDepthFunc = 0x0201; // GL_LESS
	unsigned char mDepthWrite = 1; // GL_TRUE
	unsigned char mColorWrite[4] = {1,1,1,1};
	bool mBlendEnabled = false;
	bool mCullFaceEnabled = false;
	bool mDepthTestEnabled = false;
};

// ============================================================================
// Frame State (per-frame computed state passed through rendering pipeline)
// ============================================================================

struct FrameState {
	glm::mat4 view;      // View matrix (camera transform)
	glm::mat4 proj;       // Projection matrix
	glm::mat4 viewProj;   // Pre-computed view * projection (for efficiency)
	glm::vec3 camPos;     // Camera position in world space
	
	FrameState(const glm::mat4& v, const glm::mat4& p, const glm::vec3& pos)
		: view(v), proj(p), viewProj(p * v), camPos(pos) {}
};

} // namespace Graphics


