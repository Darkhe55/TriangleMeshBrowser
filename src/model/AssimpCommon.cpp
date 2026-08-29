// src/model/AssimpCommon.cpp
// Assimp 公共导入: 场景树扁平化 + 按材质分段聚合索引 + 材质/纹理解析
// 材质面索引范围保证连续 (按材质序重排),与 PMX 材质的存储约定一致
#include "AssimpCommon.h"
#include "../utils/FileUtils.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <stb_image.h>
#include <stb_image_write.h>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <vector>

namespace prism {

namespace {

glm::vec3 toGlm(const aiVector3D& v) {
    return glm::vec3(v.x, v.y, v.z);
}

glm::mat4 toGlm(const aiMatrix4x4& m) {
    // aiMatrix4x4 命名规则: 字母=行 数字=列; 按列组装成 glm 列主序矩阵
    return glm::mat4(
        glm::vec4(m.a1, m.b1, m.c1, m.d1),
        glm::vec4(m.a2, m.b2, m.c2, m.d2),
        glm::vec4(m.a3, m.b3, m.c3, m.d3),
        glm::vec4(m.a4, m.b4, m.c4, m.d4));
}

// Y-up(右手) -> Z-up(右手): 绕 X 轴 +90°
glm::vec3 yUpToZUp(const glm::vec3& v) {
    return glm::vec3(v.x, -v.z, v.y);
}

// 读取场景元数据中的 FBX 上轴 (0=X 1=Y 2=Z),缺省按规范视为 +Y
int sceneUpAxis(const aiScene* scene) {
    int up = 1;
    if (scene->mMetaData) scene->mMetaData->Get("UpAxis", up);
    return up;
}

// 嵌入式贴图("*N" 引用)落盘为临时 PNG,返回绝对路径;失败返回空串
std::string writeEmbeddedTexture(const aiScene* scene, int texIdx,
                                 const std::filesystem::path& modelDir,
                                 const std::string& fileName) {
    if (texIdx < 0 || texIdx >= static_cast<int>(scene->mNumTextures)) return "";
    const aiTexture* tex = scene->mTextures[texIdx];
    try {
        const std::string stem = std::filesystem::path(fileName).stem().string();
        const std::filesystem::path out =
            joinPath(modelDir, stem + "_embedded" + std::to_string(texIdx) + ".png");
        if (tex->mHeight == 0) {
            // 压缩格式 (png/jpg 原始字节),经 stb 解码后写 PNG
            int w = 0, h = 0, comp = 0;
            stbi_uc* px = stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(tex->pcData),
                static_cast<int>(tex->mWidth), &w, &h, &comp, 4);
            if (!px) return "";
            const int ok = stbi_write_png(out.string().c_str(), w, h, 4, px, w * 4);
            stbi_image_free(px);
            if (!ok) return "";
        } else {
            // 原始 RGBA 像素
            const int w = static_cast<int>(tex->mWidth);
            const int h = static_cast<int>(tex->mHeight);
            if (!stbi_write_png(out.string().c_str(), w, h, 4, tex->pcData, w * 4))
                return "";
        }
        return out.string();
    } catch (...) {
        return "";
    }
}

// 单个网格: 顶点直接追加,面索引按材质分桶 (保证同材质索引连续)
bool appendMesh(Mesh& out, const aiMesh* am, const glm::mat4& world, bool convert,
                std::map<unsigned, std::vector<std::uint32_t>>& matIndices) {
    const glm::mat3 rot = glm::mat3(world);
    const std::uint32_t base = static_cast<std::uint32_t>(out.vertices_.size());
    bool missingNormals = false;

    out.vertices_.reserve(base + am->mNumVertices);
    for (unsigned i = 0; i < am->mNumVertices; ++i) {
        VertexPN v;
        glm::vec3 p = glm::vec3(world * glm::vec4(toGlm(am->mVertices[i]), 1.f));
        v.position = convert ? yUpToZUp(p) : p;
        if (am->HasNormals()) {
            glm::vec3 n = rot * toGlm(am->mNormals[i]);
            v.normal = glm::normalize(convert ? yUpToZUp(n) : n);
        } else {
            v.normal = glm::vec3(0.f, 0.f, 1.f);
            missingNormals = true;
        }
        if (am->HasTextureCoords(0)) {
            const aiVector3D& uv = am->mTextureCoords[0][i];
            v.uv = glm::vec2(uv.x, uv.y);
        }
        out.vertices_.push_back(v);
    }

    std::vector<std::uint32_t>& bucket = matIndices[am->mMaterialIndex];
    for (unsigned f = 0; f < am->mNumFaces; ++f) {
        const aiFace& face = am->mFaces[f];
        if (face.mNumIndices != 3) continue;  // 已做三角化,此处兜底
        for (unsigned k = 0; k < 3; ++k) bucket.push_back(base + face.mIndices[k]);
    }
    return missingNormals;
}

bool walkNode(Mesh& out, const aiScene* scene, const aiNode* node,
              const glm::mat4& parent, bool convert,
              std::map<unsigned, std::vector<std::uint32_t>>& matIndices) {
    const glm::mat4 world = parent * toGlm(node->mTransformation);
    bool missing = false;
    for (unsigned i = 0; i < node->mNumMeshes; ++i) {
        missing |= appendMesh(out, scene->mMeshes[node->mMeshes[i]], world, convert,
                              matIndices);
    }
    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        missing |= walkNode(out, scene, node->mChildren[i], world, convert, matIndices);
    }
    return missing;
}

// 漫反射贴图: 优先 DIFFUSE,回退 BASE_COLOR (新版 Assimp 的 glTF 映射)
bool resolveDiffuseTex(const aiMaterial* mat, aiString& path) {
    if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == aiReturn_SUCCESS) return true;
    return mat->GetTexture(aiTextureType_BASE_COLOR, 0, &path) == aiReturn_SUCCESS;
}

// 按材质序填充纹理路径与材质列表,面索引范围连续
void collectMaterials(Mesh& out, const aiScene* scene,
                      const std::filesystem::path& modelDir,
                      const std::string& fileName,
                      const std::map<unsigned, std::vector<std::uint32_t>>& matIndices) {
    out.pmxTexturePaths.reserve(scene->mNumMaterials);
    out.pmxMaterials.reserve(scene->mNumMaterials);
    std::map<unsigned, int> matTexIndex;

    for (unsigned i = 0; i < scene->mNumMaterials; ++i) {
        const aiMaterial* mat = scene->mMaterials[i];
        aiString path;
        if (resolveDiffuseTex(mat, path)) {
            const std::string s = path.C_Str();
            std::string entry;
            if (!s.empty() && s[0] == '*') {
                entry = writeEmbeddedTexture(scene, std::atoi(s.c_str() + 1),
                                             modelDir, fileName);
            } else if (!s.empty()) {
                entry = s;  // 外部贴图: 相对模型目录,由加载方解析
            }
            if (!entry.empty()) {
                matTexIndex[i] = static_cast<int>(out.pmxTexturePaths.size());
                out.pmxTexturePaths.push_back(entry);
            }
        }
    }

    for (unsigned i = 0; i < scene->mNumMaterials; ++i) {
        const auto it = matIndices.find(i);
        if (it == matIndices.end() || it->second.empty()) continue;  // 无几何的材质跳过

        const aiMaterial* mat = scene->mMaterials[i];
        PmxMaterial pm;
        pm.name = mat->GetName().C_Str();

        // 漫反射/透明度: 优先 PBR baseColor,回退传统 diffuse
        aiColor4D c4{};
        if (mat->Get(AI_MATKEY_BASE_COLOR, c4) == aiReturn_SUCCESS ||
            mat->Get(AI_MATKEY_COLOR_DIFFUSE, c4) == aiReturn_SUCCESS) {
            pm.diffuse = glm::vec3(c4.r, c4.g, c4.b);
            pm.alpha = c4.a;
        }
        float opacity = 1.f;
        if (mat->Get(AI_MATKEY_OPACITY, opacity) == aiReturn_SUCCESS) pm.alpha = opacity;

        // 高光: 传统 specular 颜色/强度;PBR metallic 作兜底
        aiColor3D spec{};
        if (mat->Get(AI_MATKEY_COLOR_SPECULAR, spec) == aiReturn_SUCCESS) {
            pm.specular = glm::vec3(spec.r, spec.g, spec.b);
        }
        float shininess = 0.f;
        if (mat->Get(AI_MATKEY_SHININESS, shininess) == aiReturn_SUCCESS) {
            pm.specularCoef = shininess;
        }
        if (pm.specularCoef <= 0.f) {
            float metallic = 0.f;
            if (mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == aiReturn_SUCCESS &&
                metallic > 0.f) {
                if (pm.specular == glm::vec3(0.f)) pm.specular = glm::vec3(metallic);
                pm.specularCoef = 16.f;
            }
        }

        const auto texIt = matTexIndex.find(i);
        pm.texIndex = (texIt != matTexIndex.end()) ? texIt->second : -1;
        pm.toonFlag = 1;   // 无 Toon 数据
        pm.sphMode  = 0;   // 无 Sphere 数据
        pm.edgeColor = glm::vec3(0.1f);  // 通用反壳边缘默认色
        pm.edgeSize  = 1.f;
        pm.firstIndex = static_cast<std::uint32_t>(out.indices_.size());
        out.indices_.insert(out.indices_.end(), it->second.begin(), it->second.end());
        pm.indexCount = static_cast<std::uint32_t>(it->second.size());
        out.triangleCount += pm.indexCount / 3;
        out.pmxMaterials.push_back(std::move(pm));
    }
}

} // namespace

std::unique_ptr<Mesh> importWithAssimp(const std::vector<std::uint8_t>& data,
                                       const std::string& hint,
                                       const std::filesystem::path& modelDir,
                                       const std::string& fileName,
                                       bool respectSceneUpAxis) {
    Assimp::Importer importer;
    const unsigned flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FlipUVs |          // glTF/FBX UV 原点在左上,翻转为 OpenGL 约定
        aiProcess_LimitBoneWeights;
    const aiScene* scene = importer.ReadFileFromMemory(
        data.data(), data.size(), flags, hint.c_str());
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mNumMeshes == 0) {
        throw std::runtime_error("Assimp parse failed: " +
                                 std::string(importer.GetErrorString()));
    }

    // FBX 文件自带轴向信息,仅 +Y 朝上需要转换; glTF 恒为 +Y
    const bool convert = !respectSceneUpAxis || sceneUpAxis(scene) == 1;

    auto mesh = std::make_unique<Mesh>();
    mesh->indexed_ = true;
    mesh->name = fileName;

    std::map<unsigned, std::vector<std::uint32_t>> matIndices;
    const bool missingNormals =
        walkNode(*mesh, scene, scene->mRootNode, glm::mat4(1.f), convert, matIndices);
    collectMaterials(*mesh, scene, modelDir, fileName, matIndices);

    // 无材质的极端情况: 直接把分桶索引写回,避免几何丢失
    if (mesh->pmxMaterials.empty() && !matIndices.empty()) {
        for (auto& kv : matIndices) {
            mesh->indices_.insert(mesh->indices_.end(), kv.second.begin(), kv.second.end());
            mesh->triangleCount += static_cast<std::uint32_t>(kv.second.size()) / 3;
        }
    }

    if (mesh->vertices_.empty() || mesh->indices_.empty()) {
        throw std::runtime_error("No triangle geometry found in file: " + fileName);
    }
    if (missingNormals) mesh->computeNormals();
    mesh->computeBBox();
    return mesh;
}

} // namespace prism
