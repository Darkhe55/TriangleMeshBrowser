// src/model/ThreeMFLoader.cpp
#include "ThreeMFLoader.h"
#include "AssimpCommon.h"
#include "../utils/FileUtils.h"

namespace prism {

std::unique_ptr<Mesh> load3MF(const std::filesystem::path& filepath) {
    // 内存读取(支持中文路径);3MF 为 zip 容器,Assimp 用 "3mf" 提示解析
    // 3MF 规范 +Z 朝上(毫米),保持原样
    const std::vector<std::uint8_t> buf = readFileBinary(filepath);
    return importWithAssimp(buf, "3mf", filepath.parent_path(),
                            getFileName(filepath), UpAxisPolicy::Keep);
}

} // namespace prism
