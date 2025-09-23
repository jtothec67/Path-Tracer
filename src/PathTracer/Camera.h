#pragma once

#include "Ray.h"

class Camera
{
public:
	Camera(glm::ivec2 _winSize);
	Camera(glm::vec3 _position, glm::vec3 _rotation, glm::ivec2 _winSize);
	~Camera() {}

	Ray GetRay(glm::ivec2 _windowPos, glm::ivec2 _windowSize);

	void CalculateMatrices(glm::ivec2 _winSize);

	// Only recalculate matricies if the cameras position has changed
	void SetPosition(glm::vec3 _position) { mPosition = _position; CalculateMatrices(mLastWinSize); }
	glm::vec3 GetPosition() { return mPosition; }

	void SetRotation(glm::vec3 _rotation) { mRotation = _rotation; CalculateMatrices(mLastWinSize); }
	glm::vec3 GetRotation() { return mRotation; }

	void SetFov(float _fov) { mFov = _fov; CalculateMatrices(mLastWinSize); }
	float GetFov() { return mFov; }

	void SetNearPlane(float _near) { mNearPlane = _near; CalculateMatrices(mLastWinSize); }
	float GetNearPlane() { return mNearPlane; }

	void SetFarPlane(float _far) { mFarPlane = _far; CalculateMatrices(mLastWinSize); }
	float GetFarPlane() { return mFarPlane; }

	glm::vec3 GetForward();
	glm::vec3 GetRight();
	glm::vec3 GetUp();

private:
	glm::vec3 mPosition{ 0.f };
	glm::vec3 mRotation{ 0.f };

	float mFov = 60.f;
	float mNearPlane = 0.1f ;
	float mFarPlane = 100.f ;

	glm::ivec2 mLastWinSize{ 0, 0 };

	glm::mat4 mView{ 1.f };
	glm::mat4 mProj{ 1.f };

	glm::mat4 mInvView{ 1.f };
	glm::mat4 mInvProj{ 1.f };
};