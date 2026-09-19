// src/model/ModelLoader.cpp
#include "ModelLoader.h"
#include "OBJLoader.h"
#include "STLLoader.h"
#include "PLYLoader.h"
#include "OFFLoader.h"
#include "PMXLoader.h"
#include "../utils/FileUtils.h"
#include <stdexcept>
#include <string>

// 需要第三方库的格式按可用性启用:
//   桌面平台      : 全部启用 (依赖由 vcpkg 提供)
//   鸿蒙 PRISM_OHOS: 仅启用已完成交叉编译的库对应格式
#if !defined(PRISM_OHOS)
  #define PRISM_FMT_ASSIMP 1
  #define PRISM_FMT_LASZIP 1
  #define PRISM_FMT_E57    1
#else
  #if defined(PRISM_HAVE_ASSIMP)
    #define PRISM_FMT_ASSIMP 1
  #endif
  #if defined(PRISM_HAVE_LASZIP)
    #define PRISM_FMT_LASZIP 1
  #endif
  #if defined(PRISM_HAVE_PUGIXML)
    #define PRISM_FMT_E57 1
  #endif
#endif

#ifdef PRISM_FMT_ASSIMP
#include "FBXLoader.h"
#include "GLTFLoader.h"
#include "ColladaLoader.h"
#include "ThreeMFLoader.h"
#endif
#ifdef PRISM_FMT_LASZIP
#include "LASLoader.h"
#endif
#ifdef PRISM_FMT_E57
#include "E57Loader.h"
#endif

namespace prism {

const std::vector<std::string>& ModelLoader::supportedExtensions() {
    static const std::vector<std::string> exts = {
        ".obj", ".stl", ".ply", ".off", ".pmx",
#ifdef PRISM_FMT_ASSIMP
        ".fbx", ".gltf", ".glb", ".dae", ".3mf",
#endif
#ifdef PRISM_FMT_LASZIP
        ".las", ".laz",
#endif
#ifdef PRISM_FMT_E57
        ".e57",
#endif
    };
    return exts;
}

std::unique_ptr<Mesh> ModelLoader::load(const std::filesystem::path& filepath) {
    const std::string ext = getExtension(filepath);
    if (ext == ".obj") return loadOBJ(filepath);
    if (ext == ".stl") return loadSTL(filepath);
    if (ext == ".ply") return loadPLY(filepath);
    if (ext == ".off") return loadOFF(filepath);
    if (ext == ".pmx") return loadPMX(filepath);
#ifdef PRISM_FMT_ASSIMP
    if (ext == ".fbx") return loadFBX(filepath);
    if (ext == ".gltf" || ext == ".glb") return loadGLTF(filepath);
    if (ext == ".dae") return loadDAE(filepath);
    if (ext == ".3mf") return load3MF(filepath);
#endif
#ifdef PRISM_FMT_LASZIP
    if (ext == ".las" || ext == ".laz") return loadLAS(filepath);
#endif
#ifdef PRISM_FMT_E57
    if (ext == ".e57") return loadE57(filepath);
#endif

    std::string list;
    for (const std::string& e : supportedExtensions()) list += " " + e;
    throw std::runtime_error("Unsupported model format: " + ext + " (supported:" + list + ")");
}

} // namespace prism
