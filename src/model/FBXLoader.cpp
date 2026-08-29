// src/model/FBXLoader.cpp
#include "FBXLoader.h"
#include "AssimpCommon.h"
#include "../utils/FileUtils.h"

namespace prism {

std::unique_ptr<Mesh> loadFBX(const std::filesystem::path& filepath) {
    // 内存读取(支持中文路径);轴向转换由场景元数据 UpAxis 决定
    const std::vector<std::uint8_t> buf = readFileBinary(filepath);
    return importWithAssimp(buf, "fbx", filepath.parent_path(),
                            getFileName(filepath), /*respectSceneUpAxis*/ true);
}

} // namespace prism
