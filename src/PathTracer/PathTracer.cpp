#include "PathTracer.h"

#include <random>
#include <thread>

// Thread-local RNG (avoid rand() in threads)
static thread_local std::mt19937 g_rng{
    static_cast<unsigned int>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()) ^ 0x9E3779B97F4A7C15ull
    )
};
static inline float Rand01() {
    static thread_local std::uniform_real_distribution<float> U(0.0f, 1.0f);
    return U(g_rng);
}

// Cosine-weighted hemisphere sample in LOCAL space (z = up)
static inline glm::vec3 SampleCosineHemisphereLocal()
{
    float u1 = Rand01();            // in [0,1)
    float u2 = Rand01();
    float r = std::sqrt(u1);
    float phi = 2.0f * 3.1415926535f * u2;

    float x = r * std::cos(phi);
    float y = r * std::sin(phi);
    float z = std::sqrt(std::max(0.0f, 1.0f - u1)); // z >= 0

    return glm::vec3(x, y, z);
}

glm::vec3 PathTracer::TraceRay(Ray _ray, int _depth, bool _albedoOnly)
{
    if (_depth <= 0)
        return glm::vec3(0.0f);

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

    if (_albedoOnly)
    {
        // If we’re not tracing rays, just return the albedo at the hit
        return best.mat->albedo;
	}

    // For now, just return the material’s albedo
    const Material& m = *best.mat;
    
    // Start with emission at the hit
    glm::vec3 L = m.emissionColour * m.emissionStrength;

    // Cosine-weighted diffuse bounce
    glm::vec3 n = glm::normalize(best.n); // Outward geometric normal
    glm::vec3 t, b;
    // Choose a helper to avoid degeneracy
    if (std::fabs(n.z) < 0.999f)
        t = glm::normalize(glm::cross(glm::vec3(0, 0, 1), n));
    else
        t = glm::normalize(glm::cross(glm::vec3(1, 0, 0), n));

    b = glm::cross(n, t);

    // Local cosine-weighted dir -> world
    glm::vec3 dLocal = SampleCosineHemisphereLocal();
    glm::vec3 dWorld = glm::normalize(dLocal.x * t + dLocal.y * b + dLocal.z * n);

    Ray next;
    next.origin = best.p + n * kTMin; // Epsilon along outward normal
    next.direction = dWorld;

    // Simple Lambertian throughput: multiply by albedo
    L += m.albedo * TraceRay(next, _depth - 1);

    return L;
}