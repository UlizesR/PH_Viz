#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Graphics {

class Camera3D {
public:
	Camera3D() = default;

	inline void setPerspective(float fovYDegrees, float aspect, float nearZ, float farZ) {
		mFovYDeg = fovYDegrees;
		mAspect = aspect;
		mNearZ = nearZ;
		mFarZ = farZ;
	}

	inline void setLookAt(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up) {
		mEye = eye;
		mTarget = target;
		mUp = up;
	}

	inline void setAspect(float aspect) { mAspect = aspect; }

	inline glm::mat4 getView() const { return glm::lookAt(mEye, mTarget, mUp); }
	inline glm::mat4 getProjection() const { return glm::perspective(glm::radians(mFovYDeg), mAspect, mNearZ, mFarZ); }
	inline glm::mat4 getViewProjection() const { return getProjection() * getView(); }

	inline const glm::vec3& eye() const { return mEye; }
	inline const glm::vec3& target() const { return mTarget; }
	inline const glm::vec3& up() const { return mUp; }

private:
	glm::vec3 mEye = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 mTarget = glm::vec3(0.0f);
	glm::vec3 mUp = glm::vec3(0.0f, 1.0f, 0.0f);

	float mFovYDeg = 45.0f;
	float mAspect = 1.0f;
	float mNearZ = 0.1f;
	float mFarZ = 100.0f;
};

} // namespace Graphics
