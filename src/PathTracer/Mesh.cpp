#include "Mesh.h"

#include <IMGUI/imgui.h>

#include <numeric>

Mesh::Mesh(const std::string& _filePath, std::string _name)
{
	mName = _name;
	mModel = std::make_shared<ModelLoader>(_filePath);

	BuildBVH();
}

static inline glm::vec4 SampleImageNearest(const ModelLoader::EmbeddedImage& img, glm::vec2 uv)
{
    if (img.width <= 0 || img.height <= 0 || img.channels <= 0 || img.data.empty())
        return glm::vec4(1, 1, 1, 1);

    // Wrap repeat
    uv = glm::fract(uv);
    if (uv.x < 0) uv.x += 1.0f;
    if (uv.y < 0) uv.y += 1.0f;

    const int x = int(uv.x * img.width);
    const int y = int(uv.y * img.height);
    const int ix = glm::clamp(x, 0, img.width - 1);
    const int iy = glm::clamp(y, 0, img.height - 1);

    const int ch = img.channels;
    const size_t idx = (size_t(iy) * img.width + size_t(ix)) * size_t(ch);

    auto get = [&](int c)->float {
        return (c < ch) ? (img.data[idx + c] / 255.0f) : (c == 3 ? 1.0f : 0.0f);
        };
    return glm::vec4(get(0), get(1), get(2), get(3));
}

bool Mesh::RayIntersect(const Ray& _ray, float _tMin, float _tMax, Hit& _out)
{
    // --- Build instance transform on the fly (strategy #2) ---
    // If you already cache mObjToWorld/mWorldToObj, use those instead.
    const glm::vec3 pos = mPosition;
    const glm::vec3 rot = mRotation; // degrees
    const glm::vec3 scl = mScale;

    glm::mat4 M(1.0f);
    M = glm::translate(M, pos);
    M = glm::rotate(M, glm::radians(rot.x), glm::vec3(1, 0, 0));
    M = glm::rotate(M, glm::radians(rot.y), glm::vec3(0, 1, 0));
    M = glm::rotate(M, glm::radians(rot.z), glm::vec3(0, 0, 1));
    M = glm::scale(M, scl);

    const glm::mat4 Minv = glm::inverse(M);
    const glm::mat3 MinvT = glm::transpose(glm::mat3(Minv)); // for normals

    // Transform ray to object space
    Ray rObj;
    rObj.origin = glm::vec3(Minv * glm::vec4(_ray.origin, 1.0f));
    rObj.direction = glm::vec3(Minv * glm::vec4(_ray.direction, 0.0f));

    // Normalize for stability; t in object space is now "units of rObj.dir"
    float dirLen = glm::length(rObj.direction);
    if (dirLen == 0.0f) return false;
    rObj.direction /= dirLen;

    // --- BVH traversal (iterative stack) ---
    if (mNodes.empty()) return false;

    const auto& faces = mModel->GetFaces();

    const float tMinObj = _tMin * dirLen;
    const float tMaxObj = _tMax * dirLen;

    float closestT = tMaxObj;
    int bestFace = -1;
    float bestU = 0.f, bestV = 0.f;

    struct StackItem { uint32_t node; };
    // Small fixed stack is enough for typical trees; fallback to vector if you prefer.
    StackItem stack[64];
    int sp = 0;
    stack[sp++] = { 0u }; // root

    while (sp)
    {
        const uint32_t nodeIdx = stack[--sp].node;
        const BvhNode& node = mNodes[nodeIdx];

        float t0, t1;
        if (!RayAabb(rObj, node.bmin, node.bmax, closestT, t0, t1)) continue;

        if (node.count > 0) // leaf
        {
            const uint32_t start = node.leftFirst;
            const uint32_t end = start + node.count;
            for (uint32_t i = start; i < end; ++i)
            {
                const uint32_t fi = mFaceIdx[i];
                const auto& f = faces[fi];

                float t, u, v;
                if (!RayTriMT(rObj, f, t, u, v)) continue;
                if (t < tMinObj || t >= closestT) continue; // object-space near/closest

                // Alpha MASK cutout (object-space, before accepting)
                if (f.materialGroup >= 0)
                {
                    const auto& groups = mModel->GetMaterialGroups();
                    const auto& pbr = groups[size_t(f.materialGroup)].pbr;
                    if (pbr.alphaMode == ModelLoader::PBRMaterial::AlphaMode::AlphaMask)
                    {
                        const float w = 1.0f - u - v;
                        const glm::vec2 uv = w * f.a.texcoord + u * f.b.texcoord + v * f.c.texcoord;

                        float alpha = pbr.baseColorFactor.a;
                        if (pbr.baseColorTexIndex >= 0)
                        {
                            const auto& img = mModel->GetEmbeddedImages()[size_t(pbr.baseColorTexIndex)];
                            const glm::vec4 tex = SampleImageNearest(img, uv);
                            alpha *= tex.a;
                        }
                        if (alpha < pbr.alphaCutoff) continue; // skip transparent texel
                    }
                }

                closestT = t;
                bestFace = int(fi);
                bestU = u; bestV = v;
            }
        }
        else
        {
            // Inner: push children (near first if you want)
            const uint32_t left = node.leftFirst;
            const uint32_t right = node.rightChild;

            float lt0, lt1, rt0, rt1;
            bool hitL = RayAabb(rObj, mNodes[left].bmin, mNodes[left].bmax, closestT, lt0, lt1);
            bool hitR = RayAabb(rObj, mNodes[right].bmin, mNodes[right].bmax, closestT, rt0, rt1);

            if (hitL && hitR) {
                if (lt0 < rt0) { stack[sp++] = { right }; stack[sp++] = { left }; }
                else { stack[sp++] = { left }; stack[sp++] = { right }; }
            }
            else if (hitL) { stack[sp++] = { left }; }
            else if (hitR) { stack[sp++] = { right }; }
        }
    }

    if (bestFace < 0) return false;

    // --- Fill Hit from the best face ---
    const auto& f = faces[static_cast<size_t>(bestFace)];
    const float u = bestU, v = bestV, w = 1.0f - u - v;

    // Interpolate in object space
    const glm::vec3 pObj = w * f.a.position + u * f.b.position + v * f.c.position;
    const glm::vec3 nObjS = glm::normalize(w * f.a.normal + u * f.b.normal + v * f.c.normal);
    const glm::vec3 e1 = f.b.position - f.a.position;
    const glm::vec3 e2 = f.c.position - f.a.position;
    const glm::vec3 nObjG = glm::normalize(glm::cross(e1, e2));
    const glm::vec2 uv = w * f.a.texcoord + u * f.b.texcoord + v * f.c.texcoord;

    // Transform back to world
    const glm::vec3 pW = glm::vec3(M * glm::vec4(pObj, 1.0f));

    glm::vec3 nObj = glm::normalize(nObjS); // start with interpolated normal

    if (f.materialGroup >= 0) {
        const auto& groups = mModel->GetMaterialGroups();
        const auto& pbr = groups[size_t(f.materialGroup)].pbr;

        if (pbr.normalTexIndex >= 0) {
            // --- Build TBN (object space) from this triangle + its UVs ---
            const glm::vec3 p0 = f.a.position, p1 = f.b.position, p2 = f.c.position;
            const glm::vec2 uv0 = f.a.texcoord, uv1 = f.b.texcoord, uv2 = f.c.texcoord;

            const glm::vec3 dp1 = p1 - p0;
            const glm::vec3 dp2 = p2 - p0;
            const glm::vec2 duv1 = uv1 - uv0;
            const glm::vec2 duv2 = uv2 - uv0;

            float det = duv1.x * duv2.y - duv1.y * duv2.x;

            // Fallback basis if UVs are degenerate
            glm::vec3 T, B;
            if (std::abs(det) > 1e-8f) {
                float r = 1.0f / det;
                glm::vec3 t = (dp1 * duv2.y - dp2 * duv1.y) * r;
                // Gram–Schmidt: make T orthogonal to N
                t = glm::normalize(t - nObj * glm::dot(nObj, t));
                T = t;
                B = glm::normalize(glm::cross(nObj, T));
            }
            else {
                // Build any orthonormal basis from N
                glm::vec3 up = (std::abs(nObj.z) < 0.999f) ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
                T = glm::normalize(glm::cross(up, nObj));
                B = glm::cross(nObj, T);
            }

            // --- Sample normal map (linear) with your existing sampler ---
            auto wrapRepeat = [](glm::vec2 uv) {
                return glm::vec2(uv.x - std::floor(uv.x), uv.y - std::floor(uv.y));
                };
            glm::vec2 uvWrapped = wrapRepeat(uv);

            const auto& img = mModel->GetEmbeddedImages()[size_t(pbr.normalTexIndex)];
            glm::vec4 tex = SampleImageNearest(img, uvWrapped); // 0..1, RGB is the normal

            // Unpack to tangent-space normal; glTF normal maps use +Z outward
            glm::vec3 n_ts = glm::vec3(tex.r * 2.0f - 1.0f,
                tex.g * 2.0f - 1.0f,
                tex.b * 2.0f - 1.0f);

            // Apply glTF normalScale to x,y only
            n_ts.x *= pbr.normalScale;
            n_ts.y *= pbr.normalScale;
            n_ts = glm::normalize(n_ts);

            // Tangent -> object
            // Columns T, B, N form the tangent frame
            glm::mat3 TBN(T, B, nObj);
            nObj = glm::normalize(TBN * n_ts);
        }
    }

    glm::vec3 nW = glm::normalize(MinvT * nObj);
    glm::vec3 ngW = glm::normalize(MinvT * nObjG);

    // Front/back
    const bool frontFace = glm::dot(_ray.direction, ngW) < 0.0f;
    if (!frontFace) nW = -nW; // orient shading normal if that’s your convention

    // Populate material (sampled at UV)
    static thread_local Material tlsMat; // per-thread scratch
    if (f.materialGroup >= 0)
    {
        FillMaterialAt(f.materialGroup, uv, tlsMat);
    }
    else
    {
        // No materials: sensible defaults
        tlsMat = Material{};
    }



    // Output
    const float tWorld = closestT / dirLen;
    _out.t = tWorld;
    _out.p = pW;
    _out.n = nW;
    _out.frontFace = frontFace;
    _out.mat = &tlsMat;

	//std::cout << "Hit mesh\n";
    return true;
}

void Mesh::UpdateUI()
{
    if (ImGui::TreeNode(mName.c_str()))
    {
        ImGui::DragFloat3("Position ", &mPosition[0], 0.1);
        ImGui::DragFloat3("Rotation ", &mRotation[0], 1.0f);
		ImGui::DragFloat3("Scale ", &mScale[0], 0.1f);
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

void Mesh::BuildBVH()
{
    const auto& faces = mModel->GetFaces();
    const size_t N = faces.size();
    if (N == 0) throw std::runtime_error("Mesh: no faces in model");

    // Init index permutation
    mFaceIdx.resize(N);
    std::iota(mFaceIdx.begin(), mFaceIdx.end(), 0u);

    // Precompute per-face bounds & centroids
    mFaceBMin.resize(N);
    mFaceBMax.resize(N);
    mFaceCentroid.resize(N);

    for (size_t i = 0; i < N; ++i)
    {
        const auto& f = faces[i];

        glm::vec3 p0 = f.a.position;
        glm::vec3 p1 = f.b.position;
        glm::vec3 p2 = f.c.position;

        glm::vec3 bmin = glm::min(p0, glm::min(p1, p2));
        glm::vec3 bmax = glm::max(p0, glm::max(p1, p2));

        mFaceBMin[i] = bmin;
        mFaceBMax[i] = bmax;
        mFaceCentroid[i] = (p0 + p1 + p2) / 3.0f;
    }

    // Reserve a rough number of nodes (binary tree upper bound)
    mNodes.clear();
    mNodes.reserve(static_cast<size_t>(2 * N));

    // Build root
    BuildNode(/*start=*/0, /*count=*/static_cast<uint32_t>(N));
}

uint32_t Mesh::BuildNode(uint32_t start, uint32_t count)
{
    const uint32_t nodeIndex = (uint32_t)mNodes.size();
    mNodes.push_back(BvhNode{}); // placeholder
    BvhNode& node = mNodes.back();

    glm::vec3 bmin, bmax;
    RangeBounds(start, count, bmin, bmax);
    node.bmin = bmin;
    node.bmax = bmax;

    if (count <= mLeafThreshold) {
        node.leftFirst = start;
        node.count = count;          // LEAF
        node.rightChild = 0;
        return nodeIndex;
    }

    // choose split axis (using node bounds extent is fine)
    glm::vec3 extent = bmax - bmin;
    int axis = 0;
    if (extent.y > extent.x && extent.y >= extent.z) axis = 1;
    else if (extent.z > extent.x && extent.z >= extent.y) axis = 2;

    const uint32_t mid = start + count / 2;
    std::nth_element(mFaceIdx.begin() + start,
        mFaceIdx.begin() + mid,
        mFaceIdx.begin() + start + count,
        [&](uint32_t ia, uint32_t ib) {
            return mFaceCentroid[ia][axis] < mFaceCentroid[ib][axis];
        });

    uint32_t leftCount = mid - start;
    uint32_t rightCount = count - leftCount;
    if (leftCount == 0 || rightCount == 0) {
        leftCount = count / 2;
        rightCount = count - leftCount;
    }

    node.count = 0; // INNER

    // Build children and record both indices explicitly
    const uint32_t leftIdx = BuildNode(start, leftCount);
    const uint32_t rightIdx = BuildNode(start + leftCount, rightCount);

    node.leftFirst = leftIdx;
    node.rightChild = rightIdx;

    return nodeIndex;
}

void Mesh::RangeBounds(uint32_t start, uint32_t count, glm::vec3& outMin, glm::vec3& outMax) const
{
    // Initialize with the first face in the range
    const glm::vec3 firstMin = mFaceBMin[mFaceIdx[start]];
    const glm::vec3 firstMax = mFaceBMax[mFaceIdx[start]];

    glm::vec3 bmin = firstMin;
    glm::vec3 bmax = firstMax;

    for (uint32_t i = start + 1; i < start + count; ++i)
    {
        const uint32_t fi = mFaceIdx[i];
        bmin = glm::min(bmin, mFaceBMin[fi]);
        bmax = glm::max(bmax, mFaceBMax[fi]);
    }

    outMin = bmin;
    outMax = bmax;
}

// Intersection helpers (slab + MT)

bool Mesh::RayAabb(const Ray& r, const glm::vec3& bmin, const glm::vec3& bmax, float tMax, float& t0, float& t1)
{
    // Slab test with lazy recip; caller can pass current closest tMax for pruning
    const glm::vec3 invD = glm::vec3(1.0f) / r.direction;

    glm::vec3 t0s = (bmin - r.origin) * invD;
    glm::vec3 t1s = (bmax - r.origin) * invD;

    glm::vec3 tsmaller = glm::min(t0s, t1s);
    glm::vec3 tbigger = glm::max(t0s, t1s);

    t0 = std::max(std::max(tsmaller.x, tsmaller.y), std::max(tsmaller.z, 0.0f));
    t1 = std::min(std::min(tbigger.x, tbigger.y), std::min(tbigger.z, tMax));

    return t1 >= t0;
}

bool Mesh::RayTriMT(const Ray& r, const ModelLoader::Face& f, float& t, float& u, float& v)
{
    // Möller–Trumbore
    const glm::vec3 v0 = f.a.position;
    const glm::vec3 v1 = f.b.position;
    const glm::vec3 v2 = f.c.position;

    const glm::vec3 e1 = v1 - v0;
    const glm::vec3 e2 = v2 - v0;
    const glm::vec3 p = glm::cross(r.direction, e2);
    const float det = glm::dot(e1, p);

    const float eps = 1e-8f;
    if (fabsf(det) < eps) return false;           // parallel or degenerate

    const float invDet = 1.0f / det;
    const glm::vec3 tvec = r.origin - v0;

    u = glm::dot(tvec, p) * invDet;
    if (u < 0.0f || u > 1.0f) return false;

    const glm::vec3 q = glm::cross(tvec, e1);
    v = glm::dot(r.direction, q) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;

    t = glm::dot(e2, q) * invDet;
    return t > eps;
}

void Mesh::FillMaterialAt(int materialGroup, const glm::vec2& uv, Material& outMat) const
{
    const auto& groups = mModel->GetMaterialGroups();
    const auto& g = groups[static_cast<size_t>(materialGroup)].pbr;
    const auto& imgs = mModel->GetEmbeddedImages();

    // Base color (linearize if you want; here we treat as already linear for simplicity)
    glm::vec4 base = g.baseColorFactor;
    if (g.baseColorTexIndex >= 0) {
        base *= SampleImageNearest(imgs[static_cast<size_t>(g.baseColorTexIndex)], uv);
    }
    outMat.albedo = glm::vec3(base);

    // Metallic-roughness (glTF: G=roughness, B=metallic)
    float rough = g.roughnessFactor;
    float metal = g.metallicFactor;
    if (g.metallicRoughnessTexIndex >= 0) {
        glm::vec4 mr = SampleImageNearest(imgs[static_cast<size_t>(g.metallicRoughnessTexIndex)], uv);
        rough = glm::clamp(mr.g * rough, 0.001f, 1.0f);
        metal = glm::clamp(mr.b * metal, 0.0f, 1.0f);
    }
    outMat.roughness = rough;
    outMat.metallic = metal;

    // Emission
    glm::vec3 emiss = g.emissiveFactor;
    if (g.emissiveTexIndex >= 0) {
        emiss *= glm::vec3(SampleImageNearest(imgs[static_cast<size_t>(g.emissiveTexIndex)], uv));
    }
    outMat.emissionColour = emiss;
    outMat.emissionStrength = glm::length(emiss); // or keep as color-only if you prefer

    // Transmission / IOR
    float tr = g.transmissionFactor;
    if (g.transmissionTexIndex >= 0) {
        tr *= SampleImageNearest(imgs[static_cast<size_t>(g.transmissionTexIndex)], uv).r;
    }
    outMat.transmission = glm::clamp(tr, 0.0f, 1.0f);
    outMat.IOR = g.ior;
}
