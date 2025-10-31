#pragma once

#include <vector>
#include <cstdint>
#include <glad/glad.h>

namespace Graphics {

class DynamicLines {
public:
	DynamicLines() = default;
	~DynamicLines() { destroy(); }
	DynamicLines(const DynamicLines&) = delete;
	DynamicLines& operator=(const DynamicLines&) = delete;
	DynamicLines(DynamicLines&& o) noexcept { steal(o); }
	DynamicLines& operator=(DynamicLines&& o) noexcept { if (this != &o) { destroy(); steal(o); } return *this; }

	// positions: xyz per vertex; count = number of floats (must be multiple of 3)
	void initPositionsOnly(std::size_t capacityFloats) {
		mIndexed = false;
		ensureCapacity(capacityFloats, 0);
		setupVAO_PosOnly();
	}

	// positions: xyz per vertex; indices: uint32 pairs; capacities in floats/indices
	void initIndexed(std::size_t posCapacityFloats, std::size_t indexCapacity) {
		mIndexed = true;
		ensureCapacity(posCapacityFloats, indexCapacity);
		setupVAO_Indexed();
	}

	// Update non-indexed lines
	void updatePositions(const float* positionsXYZ, std::size_t floatCount) {
		if (mIndexed) return; // wrong mode
		if (floatCount > mPosCapacity) ensureCapacity(floatCount, 0);
		glBindVertexArray(mVao);
		glBindBuffer(GL_ARRAY_BUFFER, mVbo);
		glBufferData(GL_ARRAY_BUFFER, mPosCapacity * sizeof(float), nullptr, GL_DYNAMIC_DRAW); // orphan
		glBufferSubData(GL_ARRAY_BUFFER, 0, floatCount * sizeof(float), positionsXYZ);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
		mPosCount = floatCount;
	}

	// Update indexed lines: positions and indices (any one can be nullptr to keep previous)
	void updateIndexed(const float* positionsXYZ, std::size_t floatCount, const uint32_t* indices, std::size_t indexCount) {
		if (!mIndexed) return;
		glBindVertexArray(mVao);
		if (positionsXYZ) {
			if (floatCount > mPosCapacity) ensureCapacity(floatCount, mIdxCapacity);
			glBindBuffer(GL_ARRAY_BUFFER, mVbo);
			glBufferData(GL_ARRAY_BUFFER, mPosCapacity * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
			glBufferSubData(GL_ARRAY_BUFFER, 0, floatCount * sizeof(float), positionsXYZ);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			mPosCount = floatCount;
		}
		if (indices) {
			if (indexCount > mIdxCapacity) ensureCapacity(mPosCapacity, indexCount);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIdxCapacity * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexCount * sizeof(uint32_t), indices);
			mIdxCount = indexCount;
		}
		glBindVertexArray(0);
	}

	void draw() const {
		if (mVao == 0) return;
		glBindVertexArray(mVao);
		if (mIndexed) {
			if (mIdxCount == 0) { glBindVertexArray(0); return; }
			glDrawElements(GL_LINES, (GLsizei)mIdxCount, GL_UNSIGNED_INT, 0);
		} else {
			if (mPosCount == 0) { glBindVertexArray(0); return; }
			GLsizei verts = (GLsizei)(mPosCount / 3);
			glDrawArrays(GL_LINES, 0, verts);
		}
		glBindVertexArray(0);
	}

	void destroy() {
		if (mEbo) { glDeleteBuffers(1, &mEbo); mEbo = 0; }
		if (mVbo) { glDeleteBuffers(1, &mVbo); mVbo = 0; }
		if (mVao) { glDeleteVertexArrays(1, &mVao); mVao = 0; }
		mPosCapacity = mIdxCapacity = mPosCount = mIdxCount = 0;
	}

private:
	void steal(DynamicLines& o) {
		mVao = o.mVao; o.mVao = 0;
		mVbo = o.mVbo; o.mVbo = 0;
		mEbo = o.mEbo; o.mEbo = 0;
		mPosCapacity = o.mPosCapacity; o.mPosCapacity = 0;
		mIdxCapacity = o.mIdxCapacity; o.mIdxCapacity = 0;
		mPosCount = o.mPosCount; o.mPosCount = 0;
		mIdxCount = o.mIdxCount; o.mIdxCount = 0;
		mIndexed = o.mIndexed; o.mIndexed = false;
	}

	void ensureCapacity(std::size_t posFloats, std::size_t idxCount) {
		if (mVao == 0) glGenVertexArrays(1, &mVao);
		if (mVbo == 0) glGenBuffers(1, &mVbo);
		if (idxCount > 0 && mEbo == 0) glGenBuffers(1, &mEbo);
		mPosCapacity = std::max(mPosCapacity, posFloats);
		mIdxCapacity = std::max(mIdxCapacity, idxCount);
	}

	void setupVAO_PosOnly() {
		glBindVertexArray(mVao);
		glBindBuffer(GL_ARRAY_BUFFER, mVbo);
		glBufferData(GL_ARRAY_BUFFER, mPosCapacity * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void setupVAO_Indexed() {
		glBindVertexArray(mVao);
		glBindBuffer(GL_ARRAY_BUFFER, mVbo);
		glBufferData(GL_ARRAY_BUFFER, mPosCapacity * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIdxCapacity * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

private:
	GLuint mVao = 0, mVbo = 0, mEbo = 0;
	std::size_t mPosCapacity = 0, mIdxCapacity = 0;
	std::size_t mPosCount = 0, mIdxCount = 0;
	bool mIndexed = false;
};

} // namespace Graphics
