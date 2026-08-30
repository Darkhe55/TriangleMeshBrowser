// src/model/GLTFLoader.cpp
#include "GLTFLoader.h"
#include "AssimpCommon.h"
#include "../utils/FileUtils.h"

namespace prism {

std::unique_ptr<Mesh> loadGLTF(const std::filesystem::path& filepath) {
    // 内存读取(支持中文路径);外部贴图引用在 .gltf 同级目录
    const std::vector<std::uint8_t> buf = readFileBinary(filepath);
    const std::string hint = (getExtension(filepath) == ".glb") ? "glb" : "gltf2";
    return importWithAssimp(buf, hint, filepath.parent_path(),
                            getFileName(filepath), UpAxisPolicy::Convert);
}

} // namespace prism
