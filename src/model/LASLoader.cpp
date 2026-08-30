// src/model/LASLoader.cpp
#include "LASLoader.h"

// 定义 LASZIP_API_VERSION 使头文件内部用 <laszip/xxx.h> 前缀路径包含,
// 否则它会裸包含 <laszip_common.h> 而找不到文件。
#define LASZIP_API_VERSION
#include <laszip/laszip_api.h>

#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace prism {
namespace {

// 点记录格式是否携带 RGB (LAS 规范: 2/3/5/7/8)
bool formatHasRgb(int format) {
    return format == 2 || format == 3 || format == 5 || format == 7 || format == 8;
}

} // namespace

std::unique_ptr<Mesh> loadLAS(const std::filesystem::path& filepath) {
    // ifstream(fs::path) 走宽字符路径, 支持中文路径
    std::ifstream stream(filepath, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("LAS: cannot open file");
    }

    laszip_POINTER las = nullptr;
    if (laszip_create(&las) != 0) {
        throw std::runtime_error("LAS: laszip_create failed");
    }
    // RAII 清理: 关闭阅读器 + 销毁句柄
    struct Cleanup {
        laszip_POINTER* p;
        ~Cleanup() {
            if (p && *p) {
                laszip_close_reader(*p);
                laszip_destroy(*p);
            }
        }
    } cleanup{&las};

    laszip_BOOL isCompressed = 0;
    if (laszip_open_reader_stream(las, stream, &isCompressed) != 0) {
        char* err = nullptr;
        laszip_get_error(las, &err);
        throw std::runtime_error(std::string("LAS: open reader failed: ") + (err ? err : ""));
    }

    laszip_header_struct* header = nullptr;
    if (laszip_get_header_pointer(las, &header) != 0 || header == nullptr) {
        throw std::runtime_error("LAS: cannot read header");
    }

    // LAS 1.4 大文件时点数记录在扩展字段
    std::uint64_t total = header->number_of_point_records;
    if (total == 0 && header->extended_number_of_point_records != 0) {
        total = header->extended_number_of_point_records;
    }
    const bool hasRgb = formatHasRgb(header->point_data_format);

    auto mesh = std::make_unique<Mesh>();
    mesh->pointCloud = true;
    constexpr std::uint64_t kReserveCap = 50'000'000;  // 预分配上限, 防异常头部爆内存
    std::vector<std::uint16_t> rawRgb;  // 有颜色格式时缓存原始 16-bit RGB(3 分量/点)
    if (total > 0 && total <= kReserveCap) {
        mesh->vertices_.reserve(static_cast<std::size_t>(total));
        if (hasRgb) rawRgb.reserve(static_cast<std::size_t>(total) * 3);
    }

    laszip_point_struct* pt = nullptr;
    if (laszip_get_point_pointer(las, &pt) != 0 || pt == nullptr) {
        throw std::runtime_error("LAS: cannot get point buffer");
    }

    bool anyColor = false;   // 是否存在非零颜色分量
    std::uint64_t read = 0;
    while (total == 0 || read < total) {
        if (laszip_read_point(las) != 0) break;  // EOF 或错误
        VertexPN v{};
        v.position = glm::vec3(
            static_cast<float>(pt->X * header->x_scale_factor + header->x_offset),
            static_cast<float>(pt->Y * header->y_scale_factor + header->y_offset),
            static_cast<float>(pt->Z * header->z_scale_factor + header->z_offset));
        mesh->vertices_.push_back(v);
        if (hasRgb) {
            rawRgb.push_back(pt->rgb[0]);
            rawRgb.push_back(pt->rgb[1]);
            rawRgb.push_back(pt->rgb[2]);
            anyColor = anyColor || (pt->rgb[0] | pt->rgb[1] | pt->rgb[2]) != 0;
        }
        ++read;
    }
    if (mesh->vertices_.empty()) {
        throw std::runtime_error("LAS: file contains no points");
    }

    if (hasRgb && anyColor) {
        // 8-bit 数据常被原样存进 16-bit 字段; 按全点云最大值判断归一化系数,
        // 再转换为线性 0-1 颜色。
        std::uint16_t maxComp = 0;
        for (std::uint16_t c : rawRgb) maxComp = std::max(maxComp, c);
        const float denom = maxComp <= 255 ? 255.f : 65535.f;
        const std::size_t n = mesh->vertices_.size();
        mesh->pointColors.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            mesh->pointColors.emplace_back(rawRgb[i * 3] / denom,
                                           rawRgb[i * 3 + 1] / denom,
                                           rawRgb[i * 3 + 2] / denom);
        }
    }
    // 无颜色(不支持的格式或全黑)时保持 pointColors 为空, 渲染回退到基准色

    return mesh;
}

} // namespace prism
