// src/model/ColladaLoader.cpp
#include "ColladaLoader.h"
#include "AssimpCommon.h"
#include "../utils/FileUtils.h"

namespace prism {

std::unique_ptr<Mesh> loadDAE(const std::filesystem::path& filepath) {
    // 内存读取(支持中文路径);Collada 恒为 +Y 朝上,统一转换
    const std::vector<std::uint8_t> buf = readFileBinary(filepath);
    return importWithAssimp(buf, "collada", filepath.parent_path(),
                            getFileName(filepath), UpAxisPolicy::Convert);
}

} // namespace prism
