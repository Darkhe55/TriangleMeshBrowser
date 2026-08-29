// src/model/PMXLoader.cpp
// PMX 2.0 / 2.1 (MikuMikuDance) 二进制格式:
//   magic "PMX " + version(f32) + 全局头(编码/各类索引字节数) +
//   模型名与注释(长度前缀文本) + 顶点 + 面索引 + 纹理 + 材质 + 骨骼 + [...]
// 解析几何 + 纹理/材质/骨骼; 骨骼之后的块(变形/刚体等)忽略。
//
// 坐标系: 将 MMD 的左手系(+Y 上)转换到本查看器的右手系(+Z 上),
// 顶点/法线做变换 (x, y, z) -> (x, z, y)。
#include "PMXLoader.h"
#include "../utils/FileUtils.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace prism {

namespace {

// 带越界检查的顺序读取游标
struct Reader {
    const std::uint8_t* data;
    size_t size;
    size_t pos = 0;
    std::string ctx;  // 错误信息前缀(含文件路径)

    void need(size_t n) const {
        if (pos + n > size)
            throw std::runtime_error("PMX: unexpected end of file: " + ctx);
    }
    std::uint8_t u8() {
        need(1);
        return data[pos++];
    }
    std::uint16_t u16() {
        need(2);
        std::uint16_t v; std::memcpy(&v, data + pos, 2); pos += 2; return v;  // little-endian
    }
    std::uint32_t u32() {
        need(4);
        std::uint32_t v; std::memcpy(&v, data + pos, 4); pos += 4; return v;
    }
    std::int32_t i32() {
        need(4);
        std::int32_t v; std::memcpy(&v, data + pos, 4); pos += 4; return v;
    }
    float f32() {
        need(4);
        float v; std::memcpy(&v, data + pos, 4); pos += 4; return v;
    }
    glm::vec3 vec3() { return {f32(), f32(), f32()}; }
    void skip(size_t n) {
        need(n);
        pos += n;
    }
};

// 按编码标志解码长度前缀文本: 0 = UTF-16LE, 1 = UTF-8
std::string readText(Reader& r, std::uint8_t encoding) {
    const std::int32_t len = r.i32();
    if (len < 0) throw std::runtime_error("PMX: negative text length: " + r.ctx);
    if (len == 0) return {};
    r.need(static_cast<size_t>(len));
    std::string out;
    if (encoding == 0) {
        // UTF-16LE → wstring → UTF-8
        std::wstring w;
        w.resize(static_cast<size_t>(len) / 2);
        for (size_t i = 0; i < w.size(); ++i)
            w[i] = static_cast<wchar_t>(r.data[r.pos + 2 * i] | (r.data[r.pos + 2 * i + 1] << 8));
        out = wideToUtf8(w);
    } else {
        out.assign(reinterpret_cast<const char*>(r.data + r.pos), static_cast<size_t>(len));
    }
    r.pos += static_cast<size_t>(len);
    return out;
}

// 按索引字节数(1/2/4)读取一个无符号索引
std::uint32_t readIndex(Reader& r, int byteSize) {
    switch (byteSize) {
    case 1: return r.u8();
    case 2: return r.u16();
    case 4: return r.u32();
    default:
        throw std::runtime_error("PMX: unsupported index byte size: " + r.ctx);
    }
}

// 按索引字节数(1/2/4)读取一个无符号索引,全 0xFF 视为 -1(无引用)
int readRefIndex(Reader& r, int byteSize) {
    switch (byteSize) {
    case 1: {
        std::uint8_t v = r.u8();
        return v == 0xFF ? -1 : static_cast<int>(v);
    }
    case 2: {
        std::uint16_t v = r.u16();
        return v == 0xFFFF ? -1 : static_cast<int>(v);
    }
    case 4: {
        std::uint32_t v = r.u32();
        return v == 0xFFFFFFFFu ? -1 : static_cast<int>(v);
    }
    default:
        throw std::runtime_error("PMX: unsupported index byte size: " + r.ctx);
    }
}

// 路径分隔符归一化 (PMX 内常用反斜杠)
std::string normalizeTexPath(std::string p) {
    for (auto& c : p) if (c == '\\') c = '/';
    return p;
}

} // namespace

std::unique_ptr<Mesh> loadPMX(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> buf = readFileBinary(path);
    Reader r{buf.data(), buf.size(), 0, wideToUtf8(path.wstring())};

    // --- 魔数与版本 ---
    r.need(8);
    if (std::memcmp(r.data, "PMX ", 4) != 0)
        throw std::runtime_error("PMX: bad magic (not a PMX file): " + r.ctx);
    r.pos += 4;
    const float version = r.f32();
    if (!(version >= 2.0f && version < 3.0f))
        throw std::runtime_error("PMX: unsupported version (need 2.0/2.1): " + r.ctx);

    // --- 全局头: 编码 / 附加UV数 / 各类索引字节数 ---
    const std::uint8_t globalCount = r.u8();
    if (globalCount < 8)
        throw std::runtime_error("PMX: malformed global header: " + r.ctx);
    r.need(globalCount);
    const std::uint8_t encoding     = r.data[r.pos + 0];  // 0=UTF-16LE, 1=UTF-8
    const std::uint8_t extraUvCount = r.data[r.pos + 1];  // 0..4
    const std::uint8_t vIdxSize     = r.data[r.pos + 2];  // 顶点索引字节数 1/2/4
    const std::uint8_t texIdxSize   = r.data[r.pos + 3];  // 纹理索引字节数 1/2/4
    const std::uint8_t matIdxSize   = r.data[r.pos + 4];  // 材质索引字节数 1/2/4 (未使用)
    const std::uint8_t boneIdxSize  = r.data[r.pos + 5];
    r.pos += globalCount;  // 跳过变形/刚体索引字节数与 2.1 软体标志

    // --- 模型名/注释: 日文名, 英文名, 日文注释, 英文注释 ---
    const std::string nameJp = readText(r, encoding);
    readText(r, encoding);
    readText(r, encoding);
    readText(r, encoding);

    auto mesh = std::make_unique<Mesh>();
    mesh->name = nameJp.empty() ? getFileName(path) : nameJp;
    mesh->indexed_ = true;

    // --- 顶点 ---
    // 布局: 位置(3f) 法线(3f) UV(2f) 附加UV(4f×N) 变形类型(1B)+载荷 边缘缩放(1f)
    const std::int32_t vCount = r.i32();
    if (vCount <= 0)
        throw std::runtime_error("PMX: no vertices: " + r.ctx);
    mesh->vertices_.reserve(static_cast<size_t>(vCount));
    for (std::int32_t i = 0; i < vCount; ++i) {
        VertexPN v;
        v.position = r.vec3();
        v.normal   = r.vec3();
        v.uv       = { r.f32(), r.f32() };                 // UV
        r.skip(static_cast<size_t>(extraUvCount) * 4 * sizeof(float));  // 附加 UV
        const std::uint8_t deform = r.u8();
        switch (deform) {
        case 0:  r.skip(boneIdxSize);                 break;  // BDEF1
        case 1:  r.skip(2 * boneIdxSize + 4);         break;  // BDEF2
        case 2:  r.skip(4 * boneIdxSize + 16);        break;  // BDEF4
        case 3:  r.skip(2 * boneIdxSize + 4 + 36);    break;  // SDEF
        case 4:  r.skip(4 * boneIdxSize + 16);        break;  // QDEF (2.1)
        default:
            throw std::runtime_error("PMX: unknown vertex deform type: " + r.ctx);
        }
        r.skip(sizeof(float));  // 边缘缩放

        // 左手系 Y 朝上 → 右手系 Z 朝上: (x, y, z) -> (x, z, y)
        v.position = { v.position.x, v.position.z, v.position.y };
        v.normal   = { v.normal.x,   v.normal.z,   v.normal.y };
        if (glm::length(v.normal) < 1e-8f) v.normal = glm::vec3(0.f, 0.f, 1.f);
        mesh->vertices_.push_back(v);
    }

    // --- 面索引: 计数为顶点索引个数,必为 3 的倍数 ---
    const std::int32_t idxCount = r.i32();
    if (idxCount <= 0 || idxCount % 3 != 0)
        throw std::runtime_error("PMX: invalid face index count: " + r.ctx);
    mesh->indices_.reserve(static_cast<size_t>(idxCount));
    for (std::int32_t i = 0; i < idxCount; ++i) {
        const std::uint32_t idx = readIndex(r, vIdxSize);
        if (idx >= mesh->vertices_.size())
            throw std::runtime_error("PMX: face index out of range: " + r.ctx);
        mesh->indices_.push_back(idx);
    }

    mesh->triangleCount = static_cast<std::uint32_t>(mesh->indices_.size() / 3);

    // --- 纹理: 长度为文本的相对路径(相对于 PMX 文件所在目录) ---
    if (r.pos < r.size) {
        const std::int32_t texCount = r.i32();
        if (texCount < 0)
            throw std::runtime_error("PMX: negative texture count: " + r.ctx);
        mesh->pmxTexturePaths.reserve(static_cast<size_t>(texCount));
        for (std::int32_t i = 0; i < texCount; ++i) {
            mesh->pmxTexturePaths.push_back(normalizeTexPath(readText(r, encoding)));
        }
    }

    // --- 材质: 每个材质占一段连续的面索引 ---
    if (r.pos < r.size) {
        const std::int32_t matCount = r.i32();
        if (matCount < 0)
            throw std::runtime_error("PMX: negative material count: " + r.ctx);
        mesh->pmxMaterials.reserve(static_cast<size_t>(matCount));
        std::uint32_t cursor = 0;
        const std::uint32_t totalIdx = static_cast<std::uint32_t>(mesh->indices_.size());
        for (std::int32_t i = 0; i < matCount; ++i) {
            PmxMaterial m;
            m.name = readText(r, encoding);
            readText(r, encoding);  // 英文名
            m.diffuse = r.vec3(); m.alpha = r.f32();
            m.specular = r.vec3(); m.specularCoef = r.f32();
            m.ambient = r.vec3();
            m.drawFlags = r.u8();
            m.edgeColor = r.vec3(); m.edgeAlpha = r.f32();
            m.edgeSize = r.f32();
            m.texIndex = readRefIndex(r, texIdxSize);
            m.sphIndex = readRefIndex(r, texIdxSize);
            m.sphMode  = static_cast<int>(r.u8());
            m.toonFlag = static_cast<int>(r.u8());
            if (m.toonFlag == 0) {
                m.toonIndex = readRefIndex(r, texIdxSize);   // 纹理引用 (共享 toon 无本地文件,忽略)
            } else {
                r.skip(1);                                   // 共享 toon 编号 0..9
            }
            readText(r, encoding);  // 备注
            const std::int32_t faceVerts = r.i32();
            m.firstIndex = cursor;
            m.indexCount = (faceVerts > 0)
                ? std::min(static_cast<std::uint32_t>(faceVerts), totalIdx - cursor)
                : 0;
            cursor += m.indexCount;
            mesh->pmxMaterials.push_back(std::move(m));
        }
        (void)matIdxSize;
    }

    // --- 骨骼: 仅存名称/位置/父骨骼,解析失败不影响几何与材质 ---
    if (r.pos < r.size) {
        try {
            const std::int32_t boneCount = r.i32();
            if (boneCount >= 0) {
                mesh->pmxBones.reserve(static_cast<size_t>(boneCount));
                for (std::int32_t i = 0; i < boneCount; ++i) {
                    PmxBone b;
                    b.name = readText(r, encoding);
                    readText(r, encoding);  // 英文名
                    glm::vec3 pos = r.vec3();
                    b.position = { pos.x, pos.z, pos.y };  // 同顶点坐标变换
                    readRefIndex(r, boneIdxSize);           // 父骨骼 (存储层暂不使用)
                    r.skip(4);                              // 变形层级
                    const std::uint16_t flags = r.u16();
                    if (flags & 0x0001) readRefIndex(r, boneIdxSize);  // 连接骨骼
                    else                r.skip(12);                    // 偏移向量
                    if (flags & 0x0180) { readRefIndex(r, boneIdxSize); r.skip(4); }  // 旋转/移动授予
                    if (flags & 0x0400) r.skip(12);                    // 固定轴方向
                    if (flags & 0x0800) r.skip(4);                     // 外部父变换 key
                    if (flags & 0x1020) {                              // IK
                        readRefIndex(r, boneIdxSize);                  // IK 目标
                        r.skip(4 + 4);                                 // 迭代次数 / 角度限制
                        const std::uint8_t links = r.u8();
                        for (std::uint8_t k = 0; k < links; ++k) {
                            readRefIndex(r, boneIdxSize);
                            if (r.u8() != 0) r.skip(24);               // 角度上下限
                        }
                    }
                    mesh->pmxBones.push_back(std::move(b));
                }
            }
        } catch (...) {
            mesh->pmxBones.clear();
        }
    }

    mesh->computeBBox();
    return mesh;
}

} // namespace prism
