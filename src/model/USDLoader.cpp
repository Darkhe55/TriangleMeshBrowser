// src/model/USDLoader.cpp
// USD/USDZ 加载: 全部文件读入内存后经 tinyusdz 解析 (支持中文路径),
// Tydra RenderSceneConverter 三角化 + 重建统一索引,再按节点世界矩阵
// 合并为单一索引网格;材质取 UsdPreviewSurface.diffuseColor / displayColor。
#include "USDLoader.h"
#include "../utils/FileUtils.h"

#include <tinyusdz.hh>
#include <tydra/render-data.hh>
#include <tydra/render-data-converter.hh>

#include <glm/glm.hpp>

#include <stdexcept>
#include <string>

namespace prism {
namespace {

namespace td = tinyusdz::tydra;

// USD 为行主序行向量左乘矩阵 (p' = p * M);逐元素拷贝进
// glm (列主序列向量) 恰好得到等价变换 (转置语义相互抵消)。
glm::mat4 toGlm(const tinyusdz::value::matrix4d& m) {
    glm::mat4 g(1.f);
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            g[c][r] = static_cast<float>(m.m[c][r]);
        }
    }
    return g;
}

// 提取网格基础色: 优先绑定材质的 diffuseColor, 其次 displayColor
glm::vec3 meshColor(const td::RenderScene& scene, const td::RenderMesh& rm,
                    float* alpha) {
    *alpha = 1.f;
    if (rm.material_id >= 0 &&
        static_cast<size_t>(rm.material_id) < scene.materials.size()) {
        const td::RenderMaterial& mat = scene.materials[rm.material_id];
        if (mat.surfaceShader.has_value()) {
            const auto& s = *mat.surfaceShader;
            if (!s.opacity.is_texture()) {
                *alpha = s.opacity.value;
            }
            const auto& d = s.diffuseColor.value;  // float[3]
            return glm::vec3(d[0], d[1], d[2]);
        }
    }
    if (rm.has_authored_displayColor) {
        return glm::vec3(rm.displayColor.r, rm.displayColor.g,
                         rm.displayColor.b);
    }
    return glm::vec3(0.18f);
}

struct ConvertCtx {
    Mesh& out;
    const td::RenderScene& scene;
    bool missingNormals = false;
};

// 将一个 RenderMesh 以指定世界矩阵追加到合并网格 (每网格一个材质段)
bool appendRenderMesh(ConvertCtx& ctx, int meshId, const glm::mat4& world) {
    if (meshId < 0 || static_cast<size_t>(meshId) >= ctx.scene.meshes.size()) {
        return false;
    }
    const td::RenderMesh& rm = ctx.scene.meshes[meshId];
    if (rm.points.empty() || !rm.is_triangulated()) {
        return false;
    }
    const std::vector<std::uint32_t>& fvi = rm.triangulatedFaceVertexIndices;
    if (fvi.empty() || fvi.size() % 3 != 0) {
        return false;
    }

    // 法线可用: 逐顶点、无独立索引、数量与顶点一致
    const bool hasNormals =
        !rm.normals.empty() && rm.normals.indices.empty() &&
        rm.normals.format == td::VertexAttributeFormat::Vec3 &&
        rm.normals.vertex_count() == rm.points.size();
    if (!hasNormals) {
        ctx.missingNormals = true;
    }
    const glm::mat3 nmat = glm::transpose(glm::inverse(glm::mat3(world)));
    const float* ndata = hasNormals
        ? reinterpret_cast<const float*>(rm.normals.buffer())
        : nullptr;

    const std::uint32_t base = static_cast<std::uint32_t>(ctx.out.vertices_.size());
    ctx.out.vertices_.reserve(base + rm.points.size());
    for (size_t i = 0; i < rm.points.size(); ++i) {
        const auto& p = rm.points[i];
        VertexPN v{};
        // glm 中 mat4*vec3 存在重载歧义;显式升为齐次 vec4(w=1) 再取 xyz
        v.position = glm::vec3(world * glm::vec4(p[0], p[1], p[2], 1.f));
        if (hasNormals) {
            const glm::vec3 n(ndata[i * 3], ndata[i * 3 + 1], ndata[i * 3 + 2]);
            const glm::vec3 tn = nmat * n;
            v.normal = glm::length(tn) > 1e-12f ? glm::normalize(tn) : glm::vec3(0.f, 1.f, 0.f);
        }
        ctx.out.vertices_.push_back(v);
    }

    float alpha = 1.f;
    PmxMaterial pm;
    pm.name = rm.prim_name;
    pm.diffuse = meshColor(ctx.scene, rm, &alpha);
    pm.alpha = alpha;
    pm.toonFlag = 1;
    pm.edgeColor = glm::vec3(0.1f);
    pm.edgeSize = 1.f;
    pm.firstIndex = static_cast<std::uint32_t>(ctx.out.indices_.size());
    ctx.out.indices_.reserve(pm.firstIndex + fvi.size());
    for (std::uint32_t idx : fvi) {
        ctx.out.indices_.push_back(base + idx);
    }
    pm.indexCount = static_cast<std::uint32_t>(fvi.size());
    ctx.out.triangleCount += pm.indexCount / 3;
    ctx.out.pmxMaterials.push_back(std::move(pm));
    return true;
}

// 递归遍历节点树,按各节点世界矩阵合并网格 (实例节点交由 instances 处理)
void walkNodes(ConvertCtx& ctx, const td::Node& node) {
    if (node.nodeType == td::NodeType::Mesh && !node.is_instance) {
        appendRenderMesh(ctx, node.id, toGlm(node.global_matrix));
    }
    for (const auto& child : node.children) {
        walkNodes(ctx, child);
    }
}

} // namespace

std::unique_ptr<Mesh> loadUSD(const std::filesystem::path& filepath) {
    // 内存读取 (支持中文路径)
    const std::vector<std::uint8_t> buf = readFileBinary(filepath);
    if (!tinyusdz::IsUSD(buf.data(), buf.size())) {
        throw std::runtime_error("USD: file is not a valid USD/USDZ asset");
    }
    const bool isUsdz = tinyusdz::IsUSDZ(buf.data(), buf.size());

    tinyusdz::Stage stage;
    std::string warn, err;
    if (!tinyusdz::LoadUSDFromMemory(buf.data(), buf.size(),
                                     filepath.string(), &stage, &warn, &err)) {
        throw std::runtime_error("USD parse failed: " + err);
    }

    td::RenderSceneConverter converter;
    td::RenderSceneConverterEnv env(stage);
    env.mesh_config.triangulate = true;
    env.mesh_config.build_vertex_indices = true;   // 属性重建为可单索引
    env.scene_config.load_texture_assets = false;  // 查看器不渲染纹理

    // USDZ: 容器内资源解析;普通 USD: 文件目录作搜索路径
    tinyusdz::USDZAsset usdzAsset;  // 生命周期须覆盖转换全程
    if (isUsdz) {
        if (!tinyusdz::ReadUSDZAssetInfoFromMemory(
                buf.data(), buf.size(), /*asset_on_memory=*/true,
                &usdzAsset, &warn, &err)) {
            throw std::runtime_error("USDZ: cannot read container index: " + err);
        }
        tinyusdz::AssetResolutionResolver arr;
        if (!tinyusdz::SetupUSDZAssetResolution(arr, &usdzAsset)) {
            throw std::runtime_error("USDZ: cannot setup asset resolution");
        }
        env.asset_resolver = arr;
    } else {
        env.set_search_paths({filepath.parent_path().string()});
    }

    td::RenderScene renderScene;
    if (!converter.ConvertToRenderScene(env, &renderScene)) {
        throw std::runtime_error("USD convert failed: " + converter.GetError());
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->indexed_ = true;
    mesh->name = getFileName(filepath);

    ConvertCtx ctx{*mesh, renderScene, false};
    if (renderScene.default_root_node < renderScene.nodes.size()) {
        walkNodes(ctx, renderScene.nodes[renderScene.default_root_node]);
    }
    // USD 实例化: 共享网格 + 各自世界矩阵
    for (const td::RenderInstance& inst : renderScene.instances) {
        appendRenderMesh(ctx, inst.mesh_id, toGlm(inst.global_matrix));
    }

    if (mesh->vertices_.empty() || mesh->indices_.empty()) {
        throw std::runtime_error("USD: no triangle geometry found in file");
    }

    // 上轴: 元数据声明 Y (或按规范缺省 = Y) -> 转 +Z 朝上: (x, y, z) -> (x, -z, y)
    // upAxis 为 TypedAttributeWithFallback<Axis>;get_value() 返回实际值或 fallback(Y)
    const auto& upAxis = stage.metas().upAxis;
    const bool yUp = upAxis.get_value() != tinyusdz::Axis::Z;
    if (yUp) {
        const glm::mat3 rot(1.f, 0.f, 0.f,
                            0.f, 0.f, -1.f,
                            0.f, 1.f, 0.f);
        for (VertexPN& v : mesh->vertices_) {
            v.position = rot * v.position;
            v.normal = rot * v.normal;
        }
    }

    if (ctx.missingNormals) mesh->computeNormals();
    mesh->computeBBox();
    return mesh;
}

} // namespace prism
