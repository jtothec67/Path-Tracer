#include "PathTracer.h"

#include <random>
#include <thread>
#include <iostream>

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
            if (h.t >= closestT)
				continue;
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
        // Make colours darker if they are further away to allow perspective for same colours
		// Colours stop getting darker at a distance of 20 units
		glm::vec3 albedo = best.mat->albedo;
		float dist = glm::clamp(closestT / 20.f, 0.0f, 0.8f);

		return albedo * (1.0f - dist);
	}
    
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

    // View direction at hit (pointing away from surface)
    glm::vec3 wo = glm::normalize(-_ray.direction);
    glm::vec3 woL(glm::dot(wo, t), glm::dot(wo, b), glm::dot(wo, n));
    float cosNo = std::max(0.0f, woL.z);

    // Base PBR params
    glm::vec3 F0 = glm::mix(glm::vec3(0.04f), m.albedo, glm::vec3(m.metallic));
    float roughness = glm::clamp(m.roughness, 0.0f, 1.0f);
    float alpha = std::max(1e-4f, roughness * roughness);

    // --------- Stage A: Transmission super-lobe coin flip ----------
    const float pT = glm::clamp(m.transmission, 0.0f, 1.0f);
    if (pT > 0.0f && Rand01() < pT)
    {
        // Interface Fresnel (dielectric) using current medium -> target medium
        float eta_i = _ray.currentIOR;
        float eta_m = m.IOR;
        float eta_t = best.frontFace ? eta_m : 1.0f; // entering vs exiting to air
        float eta = eta_i / eta_t;

        float cos_i = glm::clamp(glm::dot(-_ray.direction, n), 0.0f, 1.0f);
        float r0 = (eta_i - eta_t) / (eta_i + eta_t);
        r0 *= r0;
        float F = r0 + (1.0f - r0) * std::pow(1.0f - cos_i, 5.0f);

        // TIR check
        float sin2_t = eta * eta * std::max(0.0f, 1.0f - cos_i * cos_i);
        bool  TIR = (sin2_t > 1.0f);

        // ===== Stage B (one sample): reflect vs refract with clamped sub-prob =====
        const float F_MIN = 0.05f; // tune: 0.02–0.1 generally stable
        float pR = TIR ? 1.0f : F;   // no F_MIN clamp

        if (Rand01() < pR)
        {
            // --- Rough REFLECTION (GGX) inside interface branch ---
            glm::vec3 hL;
            if (roughness <= 1e-4f) {
                hL = glm::vec3(0, 0, 1);
            }
            else {
                float u1 = Rand01(), u2 = Rand01();
                float phi = 2.0f * 3.1415926535f * u1;
                float a2 = alpha * alpha;
                float tan2t = a2 * u2 / std::max(1e-6f, 1.0f - u2);
                float cosT = 1.0f / std::sqrt(1.0f + tan2t);
                float sinT = std::sqrt(std::max(0.0f, 1.0f - cosT * cosT));
                hL = glm::normalize(glm::vec3(sinT * std::cos(phi), sinT * std::sin(phi), cosT));
            }

            // reflect woL around hL (same as your GGX)
            glm::vec3 wiL = glm::reflect(-woL, glm::normalize(hL));
            if (wiL.z <= 0.0f) return L;

            glm::vec3 wi = glm::normalize(wiL.x * t + wiL.y * b + wiL.z * n);

            float cosNi = std::max(0.0f, wiL.z);
            float cosNh = std::max(0.0f, hL.z);
            float cosVh = std::max(0.0f, glm::dot(woL, hL));

            // Use your GGX reflect BRDF weight
            glm::vec3 one(1.0f);
            glm::vec3 Fmicro = F0 + (one - F0) * std::pow(1.0f - glm::clamp(cosVh, 0.0f, 1.0f), 5.0f);

            float a2 = alpha * alpha;
            float d = cosNh * cosNh * (a2 - 1.0f) + 1.0f;
            float D = a2 / (3.1415926535f * d * d);

            auto G1 = [&](float c) {
                c = glm::clamp(c, 0.0f, 1.0f);
                float s = std::sqrt(std::max(0.0f, 1.0f - c * c));
                float t = (c > 0.0f) ? (s / c) : 0.0f;
                float r = std::sqrt(1.0f + (alpha * alpha) * (t * t));
                return 2.0f / (1.0f + r);
                };
            float G = G1(cosNo) * G1(cosNi);

            float denom = std::max(1e-6f, cosNo * cosNh);
            glm::vec3 weight = (Fmicro * (G * cosVh)) / denom;

            // Divide by interface selection probability (NOT specProb)
            float selPdf = std::max(1e-3f, pT * pR);   // pR is your clamped reflection sub-prob
            weight /= selPdf;

            Ray next;
            next.origin = best.p + wi * kTMin;     // offset along chosen dir
            next.direction = wi;
            next.currentIOR = _ray.currentIOR;

            L += weight * TraceRay(next, _depth - 1);
            return L;
        }
        else
        {
            // Refraction (only if not TIR)
            if (!TIR)
            {
                float cos_t = std::sqrt(std::max(0.0f, 1.0f - sin2_t));
                //glm::vec3 tdir = glm::normalize(eta * (_ray.direction - cos_i * n) - cos_t * n);
                glm::vec3 tdir = glm::normalize(glm::refract(_ray.direction, n, eta));


                float selPdf = std::max(1e-3f, pT * (1.0f - pR));
                float weight = (1.0f - F) / selPdf;  // importance correction

                Ray next;
                next.origin = best.p + tdir * kTMin; // offset along chosen dir
                next.direction = tdir;
                next.currentIOR = eta_t;                 // toggle medium

                L += weight * TraceRay(next, _depth - 1);
                return L;
            }
            // If TIR, we would have gone to reflection path above (pR==1).
        }
    }

    // Fresnel-Schlick at the view angle
    glm::vec3 one(1.0f);
    float cosThetaV = glm::clamp(cosNo, 0.0f, 1.0f);
    glm::vec3 Fv = F0 + (one - F0) * std::pow(1.0f - cosThetaV, 5.0f);

    float specProb = glm::clamp((Fv.x + Fv.y + Fv.z) * (1.0f / 3.0f), 0.05f, 0.95f);

    if (Rand01() < specProb)
    {
        // Specular / GGX reflection

        // Sample GGX half-vector in LOCAL space (z=up)
        glm::vec3 hL;
        if (roughness <= 1e-4f)
        {
            // Perfect mirror
            hL = glm::vec3(0, 0, 1);
        }
        else
        {
            float u1 = Rand01();
            float u2 = Rand01();
            float phi = 2.0f * 3.1415926535f * u1;

            float a2 = alpha * alpha;
            float tan2t = a2 * u2 / std::max(1e-6f, 1.0f - u2);
            float cosT = 1.0f / std::sqrt(1.0f + tan2t);
            float sinT = std::sqrt(std::max(0.0f, 1.0f - cosT * cosT));
            hL = glm::normalize(glm::vec3(sinT * std::cos(phi), sinT * std::sin(phi), cosT));
        }

        // Reflect woL around hL
        glm::vec3 wiL = glm::reflect(-woL, glm::normalize(hL));
        if (wiL.z <= 0.0f)
            return L; // below the surface -> no contribution this sample

        // World-space outgoing
        glm::vec3 wi = glm::normalize(wiL.x * t + wiL.y * b + wiL.z * n);

        // BRDF terms
        float cosNi = std::max(0.0f, wiL.z);
        float cosNh = std::max(0.0f, hL.z);
        float cosVh = std::max(0.0f, glm::dot(woL, hL));

        // Fresnel-Schlick at v·h
        glm::vec3 F = F0 + (one - F0) * std::pow(1.0f - glm::clamp(cosVh, 0.0f, 1.0f), 5.0f);

        // GGX NDF D
        float a2 = alpha * alpha;
        float d = cosNh * cosNh * (a2 - 1.0f) + 1.0f;
        float D = a2 / (3.1415926535f * d * d);

        // Smith GGX masking-shadowing G (separable G1)
        auto G1 = [&](float cosNw)
            {
            cosNw = glm::clamp(cosNw, 0.0f, 1.0f);
            float sinNw = std::sqrt(std::max(0.0f, 1.0f - cosNw * cosNw));
            float tanNw = (cosNw > 0.0f) ? (sinNw / cosNw) : 0.0f;
            float root = std::sqrt(1.0f + (alpha * alpha) * (tanNw * tanNw));
            return 2.0f / (1.0f + root);
            };
        float G = G1(cosNo) * G1(cosNi);

        // Importance sampling via half-vector:
        // weight = (fr * cos) / pdf = F * G * cosVh / (cosNo * cosNh)
        float denom = std::max(1e-6f, cosNo * cosNh);
        glm::vec3 weight = (F * (G * cosVh)) / denom;

        // Divide by lobe selection probability
        weight /= std::max(1e-3f, specProb);
        weight /= std::max(1e-3f, 1.0f - pT);

        Ray next;
        next.origin = best.p + n * kTMin;
        next.direction = wi;

        L += weight * TraceRay(next, _depth - 1);
    }
    else
    {
        // Diffuse (Lambertian)
        glm::vec3 dLocal = SampleCosineHemisphereLocal();
        glm::vec3 dWorld = glm::normalize(dLocal.x * t + dLocal.y * b + dLocal.z * n);

        Ray next;
        next.origin = best.p + n * kTMin;
        next.direction = dWorld;

        // Cosine-weighted Lambert: throughput *= albedo
        // Account for lobe choice by dividing by (1 - specProb)
        glm::vec3 weight = ((1.0f - m.metallic) * m.albedo) / std::max(1e-3f, (1.0f - specProb));
        weight /= std::max(1e-3f, 1.0f - pT);

        L += weight * TraceRay(next, _depth - 1);
    }

    return L;
}