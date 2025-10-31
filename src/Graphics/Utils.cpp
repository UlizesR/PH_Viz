#include "Graphics/Utils.hpp"
#include <glad/glad.h>

namespace Graphics {

void GLStateCache::initialize() {
	glGetIntegerv(0x8B8D /*GL_CURRENT_PROGRAM*/, &mCurrentProgram);
	glGetIntegerv(0x0B74 /*GL_DEPTH_FUNC*/, &mDepthFunc);
	GLboolean dw; glGetBooleanv(0x0B72 /*GL_DEPTH_WRITEMASK*/, &dw); mDepthWrite = static_cast<unsigned char>(dw);
	GLboolean cw[4]; glGetBooleanv(0x0C23 /*GL_COLOR_WRITEMASK*/, cw);
	mColorWrite[0] = cw[0]; mColorWrite[1] = cw[1]; mColorWrite[2] = cw[2]; mColorWrite[3] = cw[3];
	mBlendEnabled = glIsEnabled(0x0BE2 /*GL_BLEND*/);
	mCullFaceEnabled = glIsEnabled(0x0B44 /*GL_CULL_FACE*/);
	mDepthTestEnabled = glIsEnabled(0x0B71 /*GL_DEPTH_TEST*/);
}

void GLStateCache::reset() {
	mCurrentProgram = -1;
	mDepthFunc = 0x0201; // GL_LESS
	mDepthWrite = 1;
	mColorWrite[0]=mColorWrite[1]=mColorWrite[2]=mColorWrite[3]=1;
	mBlendEnabled = mCullFaceEnabled = mDepthTestEnabled = false;
}

void GLStateCache::depthFunc(unsigned int func) {
	if (mDepthFunc != static_cast<int>(func)) { glDepthFunc(func); mDepthFunc = static_cast<int>(func); }
}

void GLStateCache::depthMask(unsigned char enabled) {
	if (mDepthWrite != enabled) { glDepthMask(enabled); mDepthWrite = enabled; }
}

void GLStateCache::enableDepthTest(bool enable) {
	if (mDepthTestEnabled != enable) { if (enable) glEnable(0x0B71 /*GL_DEPTH_TEST*/); else glDisable(0x0B71); mDepthTestEnabled = enable; }
}

void GLStateCache::colorMask(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
	if (mColorWrite[0]!=r || mColorWrite[1]!=g || mColorWrite[2]!=b || mColorWrite[3]!=a) {
		glColorMask(r,g,b,a);
		mColorWrite[0]=r; mColorWrite[1]=g; mColorWrite[2]=b; mColorWrite[3]=a;
	}
}

void GLStateCache::enableCullFace(bool enable) {
	if (mCullFaceEnabled != enable) { if (enable) glEnable(0x0B44 /*GL_CULL_FACE*/); else glDisable(0x0B44); mCullFaceEnabled = enable; }
}

void GLStateCache::enableBlend(bool enable) {
	if (mBlendEnabled != enable) { if (enable) glEnable(0x0BE2 /*GL_BLEND*/); else glDisable(0x0BE2); mBlendEnabled = enable; }
}

} // namespace Graphics


