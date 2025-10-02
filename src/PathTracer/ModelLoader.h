#pragma once

#include "tiny_gltf.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <cstdint>
#include <algorithm>

class ModelLoader
{
public:
    ModelLoader();
    explicit ModelLoader(const std::string& _path);
    ModelLoader(const ModelLoader& _copy);
    ModelLoader& operator=(const ModelLoader& _assign);
    virtual ~ModelLoader();

    // Geometry extents
    float get_width()  const;
    float get_height() const;
    float get_length() const;

    struct Vertex
    {
        glm::vec3 position{ 0.0f };
        glm::vec2 texcoord{ 0.0f };
        glm::vec3 normal{ 0.0f };
    };

    struct Face
    {
        Vertex a, b, c;
        int materialGroup = -1; // -1 when no materials; otherwise index into GetMaterialGroups()
    };

    const std::vector<ModelLoader::Face>& GetFaces() const;

    // True if the model has multiple material groups / PBR data.
    bool usesMaterials() const { return m_useMaterials; }

    // CPU image for embedded textures (no OpenGL).
    struct EmbeddedImage
    {
        int width = 0;
        int height = 0;
        int channels = 0;                 // e.g., 3=RGB, 4=RGBA
        std::vector<uint8_t> data;        // raw 8-bit pixels as stored in the glTF
    };

    struct PBRMaterial
    {
        enum class AlphaMode { AlphaOpaque = 0, AlphaMask = 1, AlphaBlend = 2 };

        // Indices into m_embeddedImages (=-1 if absent)
        int baseColorTexIndex = -1;
        int normalTexIndex = -1;
        int metallicRoughnessTexIndex = -1;
        int occlusionTexIndex = -1;
        int emissiveTexIndex = -1;

        // Factors (glTF defaults)
        glm::vec4 baseColorFactor = glm::vec4(1.0f);
        float metallicFactor = 0.0f;
        float roughnessFactor = 1.0f;

        AlphaMode alphaMode = AlphaMode::AlphaOpaque;
        float alphaCutoff = 0.5f;
        bool doubleSided = false;

        // Normal/AO/emissive scalar controls
        float normalScale = 1.0f;
        float occlusionStrength = 1.0f;
        glm::vec3 emissiveFactor = glm::vec3(0.0f);

        // KHR materials extensions (subset)
        float transmissionFactor = 0.0f;
        int   transmissionTexIndex = -1;
        float ior = 1.5f;
    };

    struct MaterialGroup
    {
        std::string materialName;
        std::vector<Face> faces;

        PBRMaterial pbr;
    };

    // Material groups (when multi-material).
    const std::vector<MaterialGroup>& GetMaterialGroups() const { return m_materialGroups; }

    // Embedded images from the glTF (CPU-side, raw bytes).
    const std::vector<EmbeddedImage>& GetEmbeddedImages() const { return m_embeddedImages; }

private:
    // Geometry
    std::vector<Face> m_faces;
    std::vector<MaterialGroup> m_materialGroups;

    // Dimensions
    float m_width = 0.0f;
    float m_height = 0.0f;
    float m_length = 0.0f;

    // Flags
    bool m_useMaterials = false;

    // CPU images (no GL)
    std::vector<EmbeddedImage> m_embeddedImages;

    // Helpers
    void calculate_dimensions();
    bool LoadGLTF(const std::string& path);
};

// ===== Implementation =====

inline ModelLoader::ModelLoader() {}

inline ModelLoader::ModelLoader(const std::string& _path)
{
    // Only accept glTF/glb now.
    std::string ext;
    size_t dot = _path.find_last_of('.');
    if (dot != std::string::npos) ext = _path.substr(dot + 1);

    if (ext == "glb" || ext == "gltf")
    {
        if (!LoadGLTF(_path))
            throw std::runtime_error("Failed to load GLTF model: " + _path);
        calculate_dimensions();
        return;
    }

    throw std::runtime_error("Model only supports .glb/.gltf: " + _path);
}

inline ModelLoader::~ModelLoader() = default;

inline ModelLoader::ModelLoader(const ModelLoader& _copy)
{
    m_faces = _copy.m_faces;
    m_materialGroups = _copy.m_materialGroups;
    m_width = _copy.m_width;
    m_height = _copy.m_height;
    m_length = _copy.m_length;
    m_useMaterials = _copy.m_useMaterials;
    m_embeddedImages = _copy.m_embeddedImages;
}

inline ModelLoader& ModelLoader::operator=(const ModelLoader& _assign)
{
    if (this == &_assign) return *this;
    m_faces = _assign.m_faces;
    m_materialGroups = _assign.m_materialGroups;
    m_width = _assign.m_width;
    m_height = _assign.m_height;
    m_length = _assign.m_length;
    m_useMaterials = _assign.m_useMaterials;
    m_embeddedImages = _assign.m_embeddedImages;
    return *this;
}

inline bool ModelLoader::LoadGLTF(const std::string& path)
{
    std::cout << "Loading glTF model from: " << path << std::endl;

    tinygltf::TinyGLTF loader;
    tinygltf::Model scene;
    std::string err, warn;

    bool ok = (path.find(".glb") != std::string::npos)
        ? loader.LoadBinaryFromFile(&scene, &err, &warn, path)
        : loader.LoadASCIIFromFile(&scene, &err, &warn, path);

    if (!warn.empty()) std::cerr << "glTF warning: " << warn << "\n";
    if (!err.empty())  std::cerr << "glTF error:   " << err << "\n";
    if (!ok) return false;

    // Embedded images (CPU copies)
    m_embeddedImages.clear();
    m_embeddedImages.reserve(scene.images.size());
    for (const auto& img : scene.images)
    {
        EmbeddedImage e;
        e.width = img.width;
        e.height = img.height;
        e.channels = img.component;
        e.data = img.image;  // copy raw 8-bit data
        m_embeddedImages.emplace_back(std::move(e));
    }

    m_faces.clear();
    m_materialGroups.clear();
    m_useMaterials = false;

    for (const auto& mesh : scene.meshes)
    {
        for (const auto& prim : mesh.primitives)
        {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES)
                throw std::runtime_error("Only TRIANGLES are supported");

            if (!prim.attributes.count("POSITION"))
                throw std::runtime_error("Missing POSITION");

            // POSITION
            const auto& posAcc = scene.accessors.at(prim.attributes.at("POSITION"));
            if (posAcc.type != TINYGLTF_TYPE_VEC3 || posAcc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
                throw std::runtime_error("POSITION must be float vec3");
            const auto& posView = scene.bufferViews.at(posAcc.bufferView);
            const auto& posBuf = scene.buffers.at(posView.buffer);
            const unsigned char* posBase = posBuf.data.data() + posView.byteOffset + posAcc.byteOffset;
            size_t posStride = posView.byteStride ? posView.byteStride : 3 * sizeof(float);

            // NORMAL (optional)
            const float* normBase = nullptr; size_t normStride = 0;
            if (prim.attributes.count("NORMAL"))
            {
                const auto& nAcc = scene.accessors.at(prim.attributes.at("NORMAL"));
                if (nAcc.type == TINYGLTF_TYPE_VEC3 && nAcc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                {
                    const auto& nView = scene.bufferViews.at(nAcc.bufferView);
                    const auto& nBuf = scene.buffers.at(nView.buffer);
                    normBase = reinterpret_cast<const float*>(nBuf.data.data() + nView.byteOffset + nAcc.byteOffset);
                    normStride = nView.byteStride ? nView.byteStride : 3 * sizeof(float);
                }
            }

            // TEXCOORD_0 (optional)
            const float* uvBase = nullptr; size_t uvStride = 0;
            if (prim.attributes.count("TEXCOORD_0"))
            {
                const auto& tAcc = scene.accessors.at(prim.attributes.at("TEXCOORD_0"));
                if (tAcc.type == TINYGLTF_TYPE_VEC2 && tAcc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                {
                    const auto& tView = scene.bufferViews.at(tAcc.bufferView);
                    const auto& tBuf = scene.buffers.at(tView.buffer);
                    uvBase = reinterpret_cast<const float*>(tBuf.data.data() + tView.byteOffset + tAcc.byteOffset);
                    uvStride = tView.byteStride ? tView.byteStride : 2 * sizeof(float);
                }
            }

            // Indices
            if (prim.indices < 0)
                throw std::runtime_error("Indexed geometry required");
            const auto& iAcc = scene.accessors.at(prim.indices);
            if (iAcc.type != TINYGLTF_TYPE_SCALAR)
                throw std::runtime_error("indices accessor must be SCALAR");
            const auto& iView = scene.bufferViews.at(iAcc.bufferView);
            const auto& iBuf = scene.buffers.at(iView.buffer);
            const unsigned char* iBase = iBuf.data.data() + iView.byteOffset + iAcc.byteOffset;
            const size_t icSize = tinygltf::GetComponentSizeInBytes(iAcc.componentType);
            const size_t iStride = iView.byteStride ? iView.byteStride : icSize;

            // Material group setup
            int  matIdx = prim.material;
            bool useMat = (matIdx >= 0);
            if (useMat) m_useMaterials = true;

            size_t groupIndex = 0;
            if (useMat)
            {
                const std::string& mname = scene.materials[matIdx].name;
                auto it = std::find_if(m_materialGroups.begin(), m_materialGroups.end(),
                    [&](const MaterialGroup& g) { return g.materialName == mname; });

                if (it == m_materialGroups.end())
                {
                    MaterialGroup mg;
                    mg.materialName = mname;
                    groupIndex = m_materialGroups.size();
                    m_materialGroups.push_back(mg);
                }
                else groupIndex = size_t(std::distance(m_materialGroups.begin(), it));

                auto& group = m_materialGroups[groupIndex];
                const auto& mat = scene.materials[matIdx];
                const auto& pbr = mat.pbrMetallicRoughness;

                if (pbr.baseColorTexture.index >= 0)
                    group.pbr.baseColorTexIndex = scene.textures[pbr.baseColorTexture.index].source;
                group.pbr.baseColorFactor = glm::make_vec4(pbr.baseColorFactor.data());

                if (pbr.metallicRoughnessTexture.index >= 0)
                    group.pbr.metallicRoughnessTexIndex = scene.textures[pbr.metallicRoughnessTexture.index].source;
                group.pbr.metallicFactor = pbr.metallicFactor;
                group.pbr.roughnessFactor = pbr.roughnessFactor;

                if (mat.normalTexture.index >= 0) group.pbr.normalTexIndex = scene.textures[mat.normalTexture.index].source;
                if (mat.occlusionTexture.index >= 0) group.pbr.occlusionTexIndex = scene.textures[mat.occlusionTexture.index].source;
                if (mat.emissiveTexture.index >= 0) group.pbr.emissiveTexIndex = scene.textures[mat.emissiveTexture.index].source;

                const std::string& am = mat.alphaMode;
                if (am == "BLEND") {
                    group.pbr.alphaMode = PBRMaterial::AlphaMode::AlphaBlend;
                }
                else if (am == "MASK") {
                    group.pbr.alphaMode = PBRMaterial::AlphaMode::AlphaMask;
                    group.pbr.alphaCutoff = float(mat.alphaCutoff); // default 0.5 in spec
                }
                else {
                    group.pbr.alphaMode = PBRMaterial::AlphaMode::AlphaOpaque;
                }

                group.pbr.doubleSided = mat.doubleSided;

                if (mat.normalTexture.index >= 0) {
                    group.pbr.normalScale = static_cast<float>(mat.normalTexture.scale);
                }
                if (mat.occlusionTexture.index >= 0) {
                    group.pbr.occlusionStrength = static_cast<float>(mat.occlusionTexture.strength);
                }
                if (mat.emissiveFactor.size() == 3) {
                    group.pbr.emissiveFactor = glm::vec3(
                        static_cast<float>(mat.emissiveFactor[0]),
                        static_cast<float>(mat.emissiveFactor[1]),
                        static_cast<float>(mat.emissiveFactor[2]));
                }

                // KHR_materials_transmission
                auto extItTr = mat.extensions.find("KHR_materials_transmission");
                if (extItTr != mat.extensions.end()) {
                    const tinygltf::Value& ext = extItTr->second;

                    auto tfIt = ext.Get("transmissionFactor");
                    if (tfIt.IsNumber()) {
                        group.pbr.transmissionFactor = static_cast<float>(tfIt.Get<double>());
                    }

                    auto ttIt = ext.Get("transmissionTexture");
                    if (ttIt.IsObject()) {
                        auto idxIt = ttIt.Get("index");
                        if (idxIt.IsInt()) {
                            int texIndex = idxIt.Get<int>();
                            if (texIndex >= 0 && texIndex < static_cast<int>(scene.textures.size())) {
                                int img = scene.textures[texIndex].source;
                                if (img >= 0 && img < static_cast<int>(scene.images.size())) {
                                    group.pbr.transmissionTexIndex = img;
                                }
                            }
                        }
                    }
                }

                // KHR_materials_ior
                auto extItIor = mat.extensions.find("KHR_materials_ior");
                if (extItIor != mat.extensions.end()) {
                    const tinygltf::Value& ext = extItIor->second;
                    auto iorIt = ext.Get("ior");
                    if (iorIt.IsNumber()) {
                        group.pbr.ior = static_cast<float>(iorIt.Get<double>());
                    }
                }
            }

            auto fetchVert = [&](Vertex& v, size_t vi)
                {
                    const float* p = reinterpret_cast<const float*>(posBase + vi * posStride);
                    v.position = { p[0], p[1], p[2] };
                    if (normBase)
                    {
                        const float* n = reinterpret_cast<const float*>(
                            reinterpret_cast<const unsigned char*>(normBase) + vi * normStride);
                        v.normal = { n[0], n[1], n[2] };
                    }
                    if (uvBase)
                    {
                        const float* t = reinterpret_cast<const float*>(
                            reinterpret_cast<const unsigned char*>(uvBase) + vi * uvStride);
                        v.texcoord = { t[0], t[1] };
                    }
                };

            for (size_t f = 0; f + 2 < iAcc.count; f += 3)
            {
                uint32_t i0 = 0, i1 = 0, i2 = 0;
                switch (iAcc.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    i0 = *(const uint8_t*)(iBase + (f + 0) * iStride);
                    i1 = *(const uint8_t*)(iBase + (f + 1) * iStride);
                    i2 = *(const uint8_t*)(iBase + (f + 2) * iStride);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    i0 = *(const uint16_t*)(iBase + (f + 0) * iStride);
                    i1 = *(const uint16_t*)(iBase + (f + 1) * iStride);
                    i2 = *(const uint16_t*)(iBase + (f + 2) * iStride);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    i0 = *(const uint32_t*)(iBase + (f + 0) * iStride);
                    i1 = *(const uint32_t*)(iBase + (f + 1) * iStride);
                    i2 = *(const uint32_t*)(iBase + (f + 2) * iStride);
                    break;
                default:
                    throw std::runtime_error("Unsupported index type");
                }

                Face face;
                fetchVert(face.a, i0);
                fetchVert(face.b, i1);
                fetchVert(face.c, i2);

                if (useMat) face.materialGroup = static_cast<int>(groupIndex);

                if (useMat) m_materialGroups[groupIndex].faces.push_back(face);
                m_faces.push_back(face);
            }
        }
    }

    return true;
}

inline void ModelLoader::calculate_dimensions()
{
    bool first = true;
    glm::vec3 min_pos, max_pos;

    auto processVertex = [&](const glm::vec3& pos)
        {
            if (first)
            {
                min_pos = pos;
                max_pos = pos;
                first = false;
            }
            else
            {
                min_pos = glm::min(min_pos, pos);
                max_pos = glm::max(max_pos, pos);
            }
        };

    if (!m_useMaterials)
    {
        for (const auto& face : m_faces)
        {
            processVertex(face.a.position);
            processVertex(face.b.position);
            processVertex(face.c.position);
        }
    }
    else
    {
        for (const auto& group : m_materialGroups)
        {
            for (const auto& face : group.faces)
            {
                processVertex(face.a.position);
                processVertex(face.b.position);
                processVertex(face.c.position);
            }
        }
    }

    m_width = max_pos.x - min_pos.x;
    m_height = max_pos.y - min_pos.y;
    m_length = max_pos.z - min_pos.z;
}

inline float ModelLoader::get_width()  const { return m_width; }
inline float ModelLoader::get_height() const { return m_height; }
inline float ModelLoader::get_length() const { return m_length; }

inline const std::vector<ModelLoader::Face>& ModelLoader::GetFaces() const
{
    return m_faces;
}