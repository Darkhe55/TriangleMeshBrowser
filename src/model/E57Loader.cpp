// src/model/E57Loader.cpp
// 轻量自包含的 E57 (ASTM E2807) 点云读取器, 替代 libE57Format + Xerces-c (~2.6MB 体积优化)。
//   - XML 元数据段用 pugixml 解析 (assimp 的传递依赖, 已链接)
//   - 二进制段手工解码: 分页 → 压缩向量段头 → 顺序扫描数据包 → BitPack 位解码
// 二进制布局与 libE57Format 3.3.0 的行为一致:
//   - 1024 字节物理页, 逻辑空间 = 每页去掉尾部 4 字节 CRC (读取时不校验)
//   - 文件头/段头/包头/位流字均为小端
//   - 编解码参数 (位宽/最小值/scale/offset) 直接取自 XML prototype,
//     位宽为 0 的字段按常量处理 (与参考实现一致, 无需解析 codecs 节点)
// 仅支持笛卡尔坐标 (cartesianX/Y/Z, 可选 RGB) 的扫描; 球坐标扫描跳过。
#include "E57Loader.h"

#include <pugixml.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace prism {
namespace {

constexpr std::uint64_t kPhysPageSize = 1024;
constexpr std::uint64_t kLogPageSize = kPhysPageSize - 4;  // 页尾 4 字节为 CRC

// 包类型 (压缩向量段内)
constexpr std::uint8_t kDataPacket = 1;

// ---------- 小端读取 (与平台字节序无关) ----------
inline std::uint16_t rdU16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

inline std::uint32_t rdU32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

inline std::uint64_t rdU64(const std::uint8_t* p) {
    return static_cast<std::uint64_t>(rdU32(p)) | (static_cast<std::uint64_t>(rdU32(p + 4)) << 32);
}

inline std::uint64_t readWord(const std::uint8_t* p, unsigned bytes) {
    switch (bytes) {
    case 1: return p[0];
    case 2: return rdU16(p);
    case 4: return rdU32(p);
    default: return rdU64(p);
    }
}

// ---------- 逻辑/物理地址换算 (与参考实现相同) ----------
inline std::uint64_t physToLogical(std::uint64_t phys) {
    const std::uint64_t page = phys / kPhysPageSize;
    const std::uint64_t rem = phys % kPhysPageSize;
    return page * kLogPageSize + std::min(rem, kLogPageSize);
}

// ---------- 分页文件读取器 (单页缓存) ----------
class PagedReader {
public:
    explicit PagedReader(const std::filesystem::path& path)
        : f_(path, std::ios::binary), page_(kPhysPageSize, 0) {
        if (!f_) {
            throw std::runtime_error("E57: cannot open file");
        }
        f_.seekg(0, std::ios::end);
        fileSize_ = static_cast<std::uint64_t>(f_.tellg());
        f_.seekg(0, std::ios::beg);
    }

    std::uint64_t fileSize() const { return fileSize_; }

    // 从逻辑地址读取 (自动跨页, 跳过每页尾部 4 字节校验区)
    void readLogical(std::uint64_t logicalOffset, void* dst, std::size_t n) {
        auto* out = static_cast<std::uint8_t*>(dst);
        while (n > 0) {
            const std::uint64_t page = logicalOffset / kLogPageSize;
            const std::uint64_t offInPage = logicalOffset % kLogPageSize;
            const std::size_t take = static_cast<std::size_t>(
                std::min<std::uint64_t>(n, kLogPageSize - offInPage));
            if (page != cachedPage_) {
                readPhysicalPage(page);
            }
            std::memcpy(out, page_.data() + offInPage, take);
            out += take;
            n -= take;
            logicalOffset += take;
        }
    }

    // 从物理地址读取原始字节 (仅用于 48 字节文件头)
    void readPhysical(std::uint64_t offset, void* dst, std::size_t n) {
        f_.seekg(static_cast<std::streamoff>(offset));
        f_.read(static_cast<char*>(dst), static_cast<std::streamsize>(n));
        if (!f_) {
            throw std::runtime_error("E57: unexpected end of file");
        }
        cachedPage_ = UINT64_MAX;  // 物理读取后页缓存失效
    }

private:
    void readPhysicalPage(std::uint64_t page) {
        const std::uint64_t pos = page * kPhysPageSize;
        if (pos >= fileSize_) {
            throw std::runtime_error("E57: read beyond end of file");
        }
        std::fill(page_.begin(), page_.end(), 0);
        const std::size_t want = static_cast<std::size_t>(
            std::min<std::uint64_t>(kPhysPageSize, fileSize_ - pos));
        f_.seekg(static_cast<std::streamoff>(pos));
        f_.read(reinterpret_cast<char*>(page_.data()), static_cast<std::streamsize>(want));
        if (!f_ && !f_.eof()) {
            throw std::runtime_error("E57: file read failed");
        }
        cachedPage_ = page;
    }

    std::ifstream f_;
    std::uint64_t fileSize_ = 0;
    std::uint64_t cachedPage_ = UINT64_MAX;
    std::vector<std::uint8_t> page_;
};

// ---------- 文件头 ----------
struct FileHeader {
    std::uint32_t majorVersion = 0;
    std::uint32_t minorVersion = 0;
    std::uint64_t filePhysicalLength = 0;
    std::uint64_t xmlPhysicalOffset = 0;
    std::uint64_t xmlLogicalLength = 0;
    std::uint64_t pageSize = 0;
};

FileHeader readFileHeader(PagedReader& r) {
    std::uint8_t h[48];
    r.readPhysical(0, h, sizeof(h));
    if (std::memcmp(h, "ASTM-E57", 8) != 0) {
        throw std::runtime_error("E57: bad file signature");
    }
    FileHeader fh;
    fh.majorVersion = rdU32(h + 8);
    fh.minorVersion = rdU32(h + 12);
    fh.filePhysicalLength = rdU64(h + 16);
    fh.xmlPhysicalOffset = rdU64(h + 24);
    fh.xmlLogicalLength = rdU64(h + 32);
    fh.pageSize = rdU64(h + 40);
    if (fh.majorVersion > 1) {
        throw std::runtime_error("E57: unsupported format version");
    }
    if (fh.pageSize != kPhysPageSize) {
        throw std::runtime_error("E57: unsupported page size");
    }
    return fh;
}

// ---------- XML 辅助 ----------
inline std::int64_t attrInt(const pugi::xml_node& n, const char* name, std::int64_t def) {
    const pugi::xml_attribute a = n.attribute(name);
    return a ? std::strtoll(a.value(), nullptr, 10) : def;
}

inline double attrDouble(const pugi::xml_node& n, const char* name, double def) {
    const pugi::xml_attribute a = n.attribute(name);
    return a ? std::strtod(a.value(), nullptr) : def;
}

inline double childDouble(const pugi::xml_node& parent, const char* name, double def) {
    const pugi::xml_node n = parent.child(name);
    return n ? std::strtod(n.text().as_string(), nullptr) : def;
}

// 叶子数值节点的取值 (Integer/Float 为文本; ScaledInteger 为 文本*scale+offset)
double leafValue(const pugi::xml_node& n) {
    const std::string type = n.attribute("type").as_string();
    const double v = std::strtod(n.text().as_string(), nullptr);
    if (type == "ScaledInteger") {
        return v * attrDouble(n, "scale", 1.0) + attrDouble(n, "offset", 0.0);
    }
    return v;
}

// ---------- prototype 字段描述 ----------
enum class FieldKind { Integer, ScaledInteger, Float, Other };

struct Field {
    FieldKind kind = FieldKind::Other;
    std::int64_t minimum = 0;
    std::int64_t maximum = 0;
    double scale = 1.0;
    double offset = 0.0;
    unsigned bits = 0;     // 存储位宽, 0 = 常量字段
    int floatBytes = 0;    // Float 字段字宽 (4/8)
    int stream = -1;       // 数据包内 bytestream 序号
};

// ceil(log2(maximum - minimum + 1)); minimum==maximum 时为 0 (常量)
unsigned bitsNeeded(std::int64_t minimum, std::int64_t maximum) {
    if (maximum <= minimum) {
        return 0;
    }
    std::uint64_t spread = static_cast<std::uint64_t>(maximum - minimum);
    unsigned bits = 0;
    while (spread > 0) {
        ++bits;
        spread >>= 1;
    }
    return bits;
}

// DFS 收集 prototype 的终端叶子 (bytestream 序号即叶子的 DFS 顺序)
void collectFields(const pugi::xml_node& node, std::vector<std::string>& order,
                   std::map<std::string, Field>& fields) {
    for (const pugi::xml_node& child : node.children()) {
        const std::string type = child.attribute("type").as_string();
        if (type == "Structure" || type == "Vector") {
            collectFields(child, order, fields);
            continue;
        }
        Field f;
        if (type == "Integer") {
            f.kind = FieldKind::Integer;
            f.minimum = attrInt(child, "minimum", INT64_MIN);
            f.maximum = attrInt(child, "maximum", INT64_MAX);
            f.bits = bitsNeeded(f.minimum, f.maximum);
        } else if (type == "ScaledInteger") {
            f.kind = FieldKind::ScaledInteger;
            f.minimum = attrInt(child, "minimum", INT64_MIN);
            f.maximum = attrInt(child, "maximum", INT64_MAX);
            f.scale = attrDouble(child, "scale", 1.0);
            f.offset = attrDouble(child, "offset", 0.0);
            f.bits = bitsNeeded(f.minimum, f.maximum);
        } else if (type == "Float") {
            f.kind = FieldKind::Float;
            f.floatBytes =
                std::string(child.attribute("precision").as_string()) == "single" ? 4 : 8;
        }
        order.push_back(child.name());
        fields[child.name()] = f;
    }
}

// 建立字段 → bytestream 映射。通常每个叶子占一个流 (常量流长度为 0);
// 少数写入器会省略常量流, 此时按非常量叶子的 DFS 顺序映射。
bool assignStreams(const std::vector<std::string>& order, std::map<std::string, Field>& fields,
                   unsigned streamCount) {
    if (streamCount == order.size()) {
        for (unsigned i = 0; i < streamCount; ++i) {
            fields[order[i]].stream = static_cast<int>(i);
        }
        return true;
    }
    std::size_t nonConst = 0;
    for (const auto& name : order) {
        const Field& f = fields[name];
        if (f.bits != 0 || f.floatBytes != 0) {
            ++nonConst;
        }
    }
    if (streamCount != nonConst) {
        return false;
    }
    int s = 0;
    for (const auto& name : order) {
        Field& f = fields[name];
        f.stream = (f.bits != 0 || f.floatBytes != 0) ? s++ : -1;
    }
    return true;
}

// ---------- 位流解码 ----------
// 整数位流: 值 = minimum + packed; packed 按 bits 位打包, 字按 1/2/4/8 字节小端分组,
// 从字的最低位开始依次取出 (与参考实现的寄存器读取方式一致)。
void decodeIntStream(const std::uint8_t* src, std::size_t recs, unsigned bits,
                     std::int64_t minimum, double scale, double offset, bool scaled,
                     float* out) {
    if (bits == 0) {
        const float c = scaled ? static_cast<float>(minimum * scale + offset)
                               : static_cast<float>(minimum);
        std::fill(out, out + recs, c);
        return;
    }
    const unsigned regBytes = bits <= 8 ? 1 : bits <= 16 ? 2 : bits <= 32 ? 4 : 8;
    const unsigned regBits = regBytes * 8u;
    const std::uint64_t mask = (bits == regBits) ? ~0ULL : ((1ULL << bits) - 1u);
    std::size_t wordPos = 0;
    unsigned bitOff = 0;
    for (std::size_t i = 0; i < recs; ++i) {
        const std::uint64_t lo = readWord(src + wordPos * regBytes, regBytes);
        std::uint64_t w;
        if (bitOff == 0) {
            w = lo;
        } else if (bitOff + bits <= regBits) {
            w = lo >> bitOff;
        } else {
            // 值跨字边界 (调用方保证该记录的全部位都在缓冲内)
            const std::uint64_t hi = readWord(src + (wordPos + 1) * regBytes, regBytes);
            w = (hi << (regBits - bitOff)) | (lo >> bitOff);
        }
        w &= mask;
        const std::int64_t value = minimum + static_cast<std::int64_t>(w);
        out[i] = scaled ? static_cast<float>(value * scale + offset)
                        : static_cast<float>(value);
        bitOff += bits;
        if (bitOff >= regBits) {
            bitOff -= regBits;
            ++wordPos;
        }
    }
}

// 浮点字段: 原始 4/8 字节字按小端存放, 直接重组
void decodeFloatStream(const std::uint8_t* src, std::size_t recs, int floatBytes, float* out) {
    if (floatBytes == 4) {
        for (std::size_t i = 0; i < recs; ++i) {
            float v;
            std::memcpy(&v, src + 4 * i, 4);
            out[i] = v;
        }
    } else {
        for (std::size_t i = 0; i < recs; ++i) {
            double v;
            std::memcpy(&v, src + 8 * i, 8);
            out[i] = static_cast<float>(v);
        }
    }
}

void decodeField(const Field& f, const std::uint8_t* data, std::size_t recs, bool rawInt,
                 float* out) {
    if (f.floatBytes != 0) {
        decodeFloatStream(data, recs, f.floatBytes, out);
    } else {
        const bool scaled = (f.kind == FieldKind::ScaledInteger) && !rawInt;
        decodeIntStream(data, recs, f.bits, f.minimum, f.scale, f.offset, scaled, out);
    }
}

// 单位四元数 → 旋转矩阵 (E57 位姿约定: p' = R * p + t)
glm::mat3 rotationFromQuaternion(double w, double x, double y, double z) {
    return glm::mat3(
        static_cast<float>(1.0 - 2.0 * (y * y + z * z)),
        static_cast<float>(2.0 * (x * y + w * z)),
        static_cast<float>(2.0 * (x * z - w * y)),
        static_cast<float>(2.0 * (x * y - w * z)),
        static_cast<float>(1.0 - 2.0 * (x * x + z * z)),
        static_cast<float>(2.0 * (y * z + w * x)),
        static_cast<float>(2.0 * (x * z + w * y)),
        static_cast<float>(2.0 * (y * z - w * x)),
        static_cast<float>(1.0 - 2.0 * (x * x + y * y)));
}

// 颜色归一分母: colorLimits → prototype 量程 → 16-bit 满量程
double colorDenom(const pugi::xml_node& scan, const char* limitName, const Field& protoField) {
    double lim = 0.0;
    if (const pugi::xml_node cl = scan.child("colorLimits")) {
        if (const pugi::xml_node n = cl.child(limitName)) {
            lim = leafValue(n);
        }
    }
    if (lim <= 0.0) {
        if (protoField.kind == FieldKind::ScaledInteger) {
            lim = protoField.maximum * protoField.scale + protoField.offset;
        } else if (protoField.kind == FieldKind::Integer) {
            lim = static_cast<double>(protoField.maximum);
        }
    }
    return lim > 0.0 ? lim : 65535.0;
}

// ---------- 单个 Data3D 扫描解码 ----------
void readScan(PagedReader& r, const pugi::xml_node& scan, Mesh* mesh, bool& hasColorGlobal) {
    const pugi::xml_node points = scan.child("points");
    if (!points) {
        return;
    }
    const std::uint64_t recordCount =
        std::strtoull(points.attribute("recordCount").value(), nullptr, 10);
    const std::uint64_t fileOffset =
        std::strtoull(points.attribute("fileOffset").value(), nullptr, 10);
    if (recordCount == 0 || fileOffset == 0) {
        return;
    }

    // prototype 字段表
    const pugi::xml_node proto = points.child("prototype");
    if (!proto) {
        return;
    }
    std::vector<std::string> order;
    std::map<std::string, Field> fields;
    collectFields(proto, order, fields);

    // 仅支持笛卡尔扫描 (与此前实现一致)
    if (!fields.count("cartesianX") || !fields.count("cartesianY") ||
        !fields.count("cartesianZ")) {
        return;
    }
    const bool hasColor = fields.count("colorRed") && fields.count("colorGreen") &&
                          fields.count("colorBlue");

    Field* fx = &fields["cartesianX"];
    Field* fy = &fields["cartesianY"];
    Field* fz = &fields["cartesianZ"];
    Field* fr = hasColor ? &fields["colorRed"] : nullptr;
    Field* fg = hasColor ? &fields["colorGreen"] : nullptr;
    Field* fb = hasColor ? &fields["colorBlue"] : nullptr;

    // 位姿: 扫描局部坐标 → 文件坐标
    glm::mat3 rot(1.f);
    glm::vec3 trans(0.f);
    if (const pugi::xml_node pose = scan.child("pose")) {
        double w = 1.0, x = 0.0, y = 0.0, z = 0.0;
        if (const pugi::xml_node rotn = pose.child("rotation")) {
            w = childDouble(rotn, "w", 1.0);
            x = childDouble(rotn, "x", 0.0);
            y = childDouble(rotn, "y", 0.0);
            z = childDouble(rotn, "z", 0.0);
        }
        if (const pugi::xml_node tr = pose.child("translation")) {
            trans = glm::vec3(static_cast<float>(childDouble(tr, "x", 0.0)),
                              static_cast<float>(childDouble(tr, "y", 0.0)),
                              static_cast<float>(childDouble(tr, "z", 0.0)));
        }
        rot = rotationFromQuaternion(w, x, y, z);
    }

    const float rDenom =
        hasColor ? static_cast<float>(colorDenom(scan, "colorRedMaximum", *fr)) : 1.f;
    const float gDenom =
        hasColor ? static_cast<float>(colorDenom(scan, "colorGreenMaximum", *fg)) : 1.f;
    const float bDenom =
        hasColor ? static_cast<float>(colorDenom(scan, "colorBlueMaximum", *fb)) : 1.f;

    // 压缩向量段头 (32 字节): sectionId + reserved[7] + logicalLength + dataOff + indexOff
    const std::uint64_t sectionLogicalStart = physToLogical(fileOffset);
    std::uint8_t sh[32];
    r.readLogical(sectionLogicalStart, sh, sizeof(sh));
    if (sh[0] != 1) {  // COMPRESSED_VECTOR_SECTION
        throw std::runtime_error("E57: bad binary section");
    }
    const std::uint64_t sectionLogicalLength = rdU64(sh + 8);
    const std::uint64_t dataPhysicalOffset = rdU64(sh + 16);
    const std::uint64_t sectionEnd = sectionLogicalStart + sectionLogicalLength;

    // 顺序扫描数据包 (与参考实现相同, 无需索引包)
    std::uint64_t pktLog = physToLogical(dataPhysicalOffset);
    std::uint64_t recordsDone = 0;
    bool streamsAssigned = false;
    unsigned expectedStreamCount = 0;

    std::vector<float> xs, ys, zs, crs, cgs, cbs;

    while (pktLog < sectionEnd && recordsDone < recordCount) {
        std::uint8_t hdr[6];
        r.readLogical(pktLog, hdr, 6);
        const std::uint8_t packetType = hdr[0];
        const std::uint64_t pktLen = static_cast<std::uint64_t>(rdU16(hdr + 2)) + 1u;
        if (packetType != kDataPacket) {  // 跳过索引包/空包
            pktLog += pktLen;
            continue;
        }

        // 尾部多留 8 字节余量, 供跨字位读取越界保护
        std::vector<std::uint8_t> pkt(pktLen + 8, 0);
        r.readLogical(pktLog, pkt.data(), pktLen);

        const unsigned streamCount = rdU16(pkt.data() + 4);
        if (pktLen < 6u + 2u * streamCount) {
            throw std::runtime_error("E57: malformed data packet");
        }
        if (!streamsAssigned) {
            if (!assignStreams(order, fields, streamCount)) {
                throw std::runtime_error("E57: unsupported codec layout");
            }
            streamsAssigned = true;
            expectedStreamCount = streamCount;
        } else if (streamCount != expectedStreamCount) {
            throw std::runtime_error("E57: inconsistent bytestream count");
        }
        for (const Field* f : {fx, fy, fz, fr, fg, fb}) {
            if (f && f->stream < 0 && (f->bits != 0 || f->floatBytes != 0)) {
                throw std::runtime_error("E57: unsupported codec layout");
            }
        }

        // 各流长度与前缀偏移, 并校验流数据不越出包边界
        std::vector<std::uint32_t> lens(streamCount);
        std::uint64_t totalBytes = 6u + 2u * streamCount;
        for (unsigned i = 0; i < streamCount; ++i) {
            lens[i] = rdU16(pkt.data() + 6 + 2u * i);
            totalBytes += lens[i];
        }
        if (totalBytes > pktLen) {
            throw std::runtime_error("E57: malformed data packet");
        }
        const std::uint64_t off = 6u + 2u * streamCount;

        // 本包记录数: 取所需流中可完整解码的最小值 (常量流不限制)
        auto streamCap = [&](const Field& f) -> std::size_t {
            if (f.stream < 0) {
                return SIZE_MAX;
            }
            const std::uint32_t len = lens[f.stream];
            if (f.floatBytes != 0) {
                return len / static_cast<std::uint32_t>(f.floatBytes);
            }
            if (f.bits != 0) {
                return static_cast<std::size_t>(
                    (static_cast<std::uint64_t>(len) * 8u) / f.bits);
            }
            return SIZE_MAX;
        };
        std::size_t recs = static_cast<std::size_t>(recordCount - recordsDone);
        for (const Field* f : {fx, fy, fz, fr, fg, fb}) {
            if (f) {
                recs = std::min(recs, streamCap(*f));
            }
        }
        if (recs == 0) {
            pktLog += pktLen;
            continue;
        }

        const std::uint8_t* streamsBase = pkt.data() + off;
        auto streamPtr = [&](int s) -> const std::uint8_t* {
            std::uint64_t o = 0;
            for (int i = 0; i < s; ++i) {
                o += lens[i];
            }
            return streamsBase + o;
        };

        xs.resize(recs);
        ys.resize(recs);
        zs.resize(recs);
        decodeField(*fx, streamPtr(fx->stream), recs, false, xs.data());
        decodeField(*fy, streamPtr(fy->stream), recs, false, ys.data());
        decodeField(*fz, streamPtr(fz->stream), recs, false, zs.data());
        if (hasColor) {
            crs.resize(recs);
            cgs.resize(recs);
            cbs.resize(recs);
            // 颜色按原始整数值输出, 稍后按分母归一
            decodeField(*fr, streamPtr(fr->stream), recs, true, crs.data());
            decodeField(*fg, streamPtr(fg->stream), recs, true, cgs.data());
            decodeField(*fb, streamPtr(fb->stream), recs, true, cbs.data());
        }

        mesh->vertices_.reserve(mesh->vertices_.size() + recs);
        if (hasColor) {
            mesh->pointColors.reserve(mesh->pointColors.size() + recs);
        }
        for (std::size_t k = 0; k < recs; ++k) {
            VertexPN v{};
            v.position = rot * glm::vec3(xs[k], ys[k], zs[k]) + trans;
            mesh->vertices_.push_back(v);
            if (hasColor) {
                mesh->pointColors.emplace_back(
                    std::clamp(crs[k] / rDenom, 0.f, 1.f),
                    std::clamp(cgs[k] / gDenom, 0.f, 1.f),
                    std::clamp(cbs[k] / bDenom, 0.f, 1.f));
            }
        }

        recordsDone += recs;
        pktLog += pktLen;
    }

    if (recordsDone == 0) {
        return;  // 该扫描无可读点, 跳过
    }
    hasColorGlobal = hasColorGlobal || hasColor;
    if (mesh->name.empty()) {
        if (const pugi::xml_node nm = scan.child("name")) {
            if (const char* t = nm.text().as_string(); t && *t) {
                mesh->name = t;
            }
        }
    }
}

} // namespace

std::unique_ptr<Mesh> loadE57(const std::filesystem::path& filepath) {
    PagedReader r(filepath);
    const FileHeader fh = readFileHeader(r);

    // XML 元数据段 (逻辑空间连续读出后交给 pugixml)
    std::string xml;
    xml.resize(static_cast<std::size_t>(fh.xmlLogicalLength));
    r.readLogical(physToLogical(fh.xmlPhysicalOffset), xml.data(), xml.size());

    pugi::xml_document doc;
    if (!doc.load_buffer(xml.data(), xml.size())) {
        throw std::runtime_error("E57: cannot parse XML metadata");
    }

    const pugi::xml_node root = doc.document_element();  // e57Root
    const pugi::xml_node data3D = root.child("data3D");
    if (!data3D) {
        throw std::runtime_error("E57: file contains no 3D scans");
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->pointCloud = true;

    bool hasColorGlobal = false;
    for (const pugi::xml_node& scan : data3D.children()) {
        if (std::string(scan.attribute("type").as_string()) != "Data3D") {
            continue;
        }
        readScan(r, scan, mesh.get(), hasColorGlobal);
    }

    if (mesh->vertices_.empty()) {
        throw std::runtime_error("E57: no readable points in file");
    }

    // 混合情形: 部分扫描带颜色、部分不带 → 为无颜色段补白色保持属性对齐
    if (hasColorGlobal && mesh->pointColors.size() < mesh->vertices_.size()) {
        mesh->pointColors.resize(mesh->vertices_.size(), glm::vec3(1.f));
    }

    return mesh;
}

} // namespace prism
