#include "PathTracer.h"

glm::vec3 PathTracer::TraceRay(Ray _ray, glm::vec3 _camPos, int _depth)
{
    const float kTMin = 1e-4f; // Avoid self-intersection
    const float kTMax = 1e30f;

    Hit best{};
    bool hitSomething = false;
    float closestT = kTMax;

    for (const auto& obj : rayObjects)
    {
        Hit h{};
        if (obj->RayIntersect(_ray, kTMin, closestT, h))
        {
            // If the object filled h.t < closestT, it’s the new best hit
            hitSomething = true;
            closestT = h.t;
            best = h;
        }
    }

    if (!hitSomething)
        return mBackgroundColour;

    // For now, just return the material’s albedo
    const Material& m = *best.mat;
    glm::vec3 color = m.albedo;

    return color;
}