#include "Box.h"

#include <IMGUI/imgui.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

Box::Box(std::string _name)
{
    mName = _name;
}

bool Box::RayIntersect(const Ray& _ray, float _tMin, float _tMax, Hit& _out)
{
    // 1) Build rotation (worldFromLocal) from Euler, then its inverse (localFromWorld)
    glm::mat4 R = glm::mat4(1.0f);
    R = glm::rotate(R, glm::radians(mRotation.x), glm::vec3(1, 0, 0));
    R = glm::rotate(R, glm::radians(mRotation.y), glm::vec3(0, 1, 0));
    R = glm::rotate(R, glm::radians(mRotation.z), glm::vec3(0, 0, 1));

    // For pure rotation, inverse is transpose of the 3x3
    glm::mat3 worldFromLocal = glm::mat3(R);
    glm::mat3 localFromWorld = glm::transpose(worldFromLocal);

    // 2) Transform ray into the box's local space (box center at origin)
    glm::vec3 roLocal = localFromWorld * (_ray.origin - mPosition);
    glm::vec3 rdLocal = localFromWorld * _ray.direction;

    // 3) Axis-aligned box extents in local space
    const glm::vec3 halfExtents = mSize * 0.5f;

    // 4) Slab intersection (AABB in local space)
    // Handle near-zeros in direction by using large numbers for invDir
    const float BIG = 1e30f;
    const glm::vec3 invD(
        rdLocal.x != 0.0f ? 1.0f / rdLocal.x : (rdLocal.x > 0.0f ? BIG : -BIG),
        rdLocal.y != 0.0f ? 1.0f / rdLocal.y : (rdLocal.y > 0.0f ? BIG : -BIG),
        rdLocal.z != 0.0f ? 1.0f / rdLocal.z : (rdLocal.z > 0.0f ? BIG : -BIG)
    );

    glm::vec3 t1 = (-halfExtents - roLocal) * invD;
    glm::vec3 t2 = (halfExtents - roLocal) * invD;

    glm::vec3 tMin3 = glm::min(t1, t2);
    glm::vec3 tMax3 = glm::max(t1, t2);

    float tEntry = glm::compMax(tMin3);  // largest of the three mins
    float tExit = glm::compMin(tMax3);  // smallest of the three maxs

    if (tExit < tEntry) return false;            // missed slabs
    if (tExit < _tMin)  return false;            // box entirely behind range
    float tHit = tEntry;
    if (tHit < _tMin) tHit = tExit;              // start inside box -> exit is first valid
    if (tHit < _tMin || tHit > _tMax) return false;

    // 5) Hit point in local and world space
    glm::vec3 pLocal = roLocal + tHit * rdLocal;
    glm::vec3 pWorld = _ray.origin + tHit * _ray.direction;

    // 6) Compute geometric outward normal in local space
    // Determine which face we hit by seeing which coordinate is on a face.
    glm::vec3 nLocal(0.0f);
    const float eps = 1e-4f * glm::max(glm::max(halfExtents.x, halfExtents.y), halfExtents.z);

    if (std::abs(pLocal.x - halfExtents.x) <= eps) nLocal = glm::vec3(1, 0, 0);
    else if (std::abs(pLocal.x + halfExtents.x) <= eps) nLocal = glm::vec3(-1, 0, 0);
    else if (std::abs(pLocal.y - halfExtents.y) <= eps) nLocal = glm::vec3(0, 1, 0);
    else if (std::abs(pLocal.y + halfExtents.y) <= eps) nLocal = glm::vec3(0, -1, 0);
    else if (std::abs(pLocal.z - halfExtents.z) <= eps) nLocal = glm::vec3(0, 0, 1);
    else if (std::abs(pLocal.z + halfExtents.z) <= eps) nLocal = glm::vec3(0, 0, -1);
    else {
        // Fallback: pick the axis with largest t-entry to decide the face
        // (Rarely needed, but robust if we're slightly inside due to FP)
        float txEntry = tMin3.x, tyEntry = tMin3.y, tzEntry = tMin3.z;
        if (tEntry == txEntry) nLocal = glm::vec3((roLocal.x + tEntry * rdLocal.x) > 0.0f ? 1 : -1, 0, 0);
        else if (tEntry == tyEntry) nLocal = glm::vec3(0, (roLocal.y + tEntry * rdLocal.y) > 0.0f ? 1 : -1, 0);
        else                        nLocal = glm::vec3(0, 0, (roLocal.z + tEntry * rdLocal.z) > 0.0f ? 1 : -1);
    }

    // 7) Transform normal back to world and face-forward
    glm::vec3 nWorld = glm::normalize(worldFromLocal * nLocal);
    bool frontFace = glm::dot(_ray.direction, nWorld) < 0.0f;
    glm::vec3 shadingNormal = frontFace ? nWorld : -nWorld;

    // 8) Fill hit record
    _out.t = tHit;
    _out.p = pWorld;
    _out.n = shadingNormal;
    _out.mat = &mMaterial;

    return true;
}

void Box::UpdateUI()
{
    if (ImGui::TreeNode(mName.c_str()))
    {
        ImGui::DragFloat3("Position ", &mPosition[0], 0.1);
		ImGui::DragFloat3("Rotation ", &mRotation[0], 1.0f, 0, 360);
        ImGui::DragFloat3("Size ", &mSize[0], 0.1f);
        ImGui::ColorEdit3("Albedo", &mMaterial.albedo.r);
        ImGui::SliderFloat("Roughness", &mMaterial.roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Metallic", &mMaterial.metallic, 0.0f, 1.0f);
        ImGui::ColorEdit3("Emission Colour", &mMaterial.emissionColour.r);
        ImGui::SliderFloat("Emission Strength", &mMaterial.emissionStrength, 0.0f, 100.0f);
        ImGui::SliderFloat("Index of Refraction", &mMaterial.IOR, 1.0f, 3.0f);
        ImGui::SliderFloat("Transmission", &mMaterial.transmission, 0.0f, 1.0f);
        ImGui::TreePop();
    }
}