#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(glm::ivec2 _winSize)
{
    mLastWinSize = _winSize;

    CalculateMatrices(_winSize);
}

Camera::Camera(glm::vec3 _position, glm::vec3 _rotation, glm::ivec2 _winSize)
{
    mPosition = _position;
    mRotation = _rotation;

    mLastWinSize = _winSize;

    CalculateMatrices(_winSize);
}

void Camera::CalculateMatrices(glm::ivec2 _winSize)
{
    glm::mat4 R = glm::mat4(1.f);
    R = glm::rotate(R, glm::radians(mRotation.x), { 1,0,0 });
    R = glm::rotate(R, glm::radians(mRotation.y), { 0,1,0 });
    R = glm::rotate(R, glm::radians(mRotation.z), { 0,0,1 });

    glm::mat4 worldFromCam = glm::translate(glm::mat4(1.f), mPosition) * R;
    mView = glm::inverse(worldFromCam);
    mInvView = worldFromCam;

    mProj = glm::perspective(glm::radians(mFov), float(_winSize.x) / float(_winSize.y),  mNearPlane, mFarPlane);
    mInvProj = glm::inverse(mProj);
}

// This function generates a ray from the camera through a point on the screen
Ray Camera::GetRay(glm::ivec2 _windowPos, glm::ivec2 _windowSize)
{
    // 1) NDC
    float nx = ((_windowPos.x + 0.5f) / float(_windowSize.x)) * 2.f - 1.f;
    float ny = ((_windowPos.y + 0.5f) / float(_windowSize.y)) * 2.f - 1.f;

    // 2) Clip near/far
    glm::vec4 clipNear(nx, ny, -1.f, 1.f);
    glm::vec4 clipFar(nx, ny, 1.f, 1.f);

    // 3) Camera space
    glm::vec4 camNear = mInvProj * clipNear; camNear /= camNear.w;
    glm::vec4 camFar = mInvProj * clipFar;  camFar /= camFar.w;

    // 4) World space: origin at camera, direction uses rotation only
    glm::vec3 originWorld = glm::vec3(mInvView * glm::vec4(0, 0, 0, 1));
    glm::vec3 dirCam = glm::normalize(glm::vec3(camFar - camNear));
    glm::vec3 dirWorld = glm::normalize(glm::mat3(mInvView) * dirCam);

    return Ray{ originWorld, dirWorld };
}

// Helper functions to get the camera's forward, right, and up vectors
glm::vec3 Camera::GetForward()
{
    glm::vec3 forward = glm::vec3(mView[0][2], mView[1][2], mView[2][2]);
    return glm::normalize(forward);
}

glm::vec3 Camera::GetRight()
{
    glm::vec3 right = glm::vec3(mView[0][0], mView[1][0], mView[2][0]);
    return glm::normalize(right);
}

glm::vec3 Camera::GetUp()
{
    glm::vec3 up = glm::vec3(mView[0][1], mView[1][1], mView[2][1]);
    return glm::normalize(up);
}