#pragma once

#include "RayObject.h"

#include "ModelLoader.h"

#include "tiny_gltf.h"

#include <memory>

class Mesh : public RayObject
{
public:
	Mesh(const std::string& _filePath, std::string _name);
	~Mesh() {}

	bool RayIntersect(const Ray& _ray, float _tMin, float _tMax, Hit& _out) override;

	void UpdateUI() override;

	void SetScale(const glm::vec3& _scale) { mScale = _scale; }
	glm::vec3 GetScale() { return mScale; }

private:
	glm::vec3 mScale = glm::vec3(1.0f);

	std::shared_ptr<ModelLoader> mModel;

    // BVH build helpers
    void BuildBVH();
    uint32_t BuildNode(uint32_t start, uint32_t count); // Returns node index

    // Compute aabb for a range of faces (by indices)
    void RangeBounds(uint32_t start, uint32_t count, glm::vec3& outMin, glm::vec3& outMax) const;

    // Intersection helpers (for traversal)
    static inline bool RayAabb(const Ray& r, const glm::vec3& bmin, const glm::vec3& bmax, float tMax, float& t0, float& t1);

    static inline bool RayTriMT(const Ray& r, const ModelLoader::Face& f, float& t, float& u, float& v);

    // BVH (flattened, index-based)
    struct BvhNode
    {
        glm::vec3 bmin; // Node AABB
        glm::vec3 bmax;
        uint32_t leftFirst; // Inner: index of left child; leaf: start in mFaceIdx
        uint32_t rightChild;
        uint32_t count; // Inner: 0; leaf: number of faces in leaf
    };

    std::vector<BvhNode> mNodes; // Nodes in a flat array
    std::vector<uint32_t> mFaceIdx; // Permutation of [0..numFaces), leaves are contiguous ranges

    // Precomputed per-face bounds & centroids (object space)
    std::vector<glm::vec3> mFaceBMin;
    std::vector<glm::vec3> mFaceBMax;
    std::vector<glm::vec3> mFaceCentroid;

    unsigned mLeafThreshold = 2; // Max faces per leaf


    void FillMaterialAt(int materialGroup, const glm::vec2& uv, Material& outMat) const;
};