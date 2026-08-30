// src/model/ModelLoader.cpp
#include "ModelLoader.h"
#include "OBJLoader.h"
#include "STLLoader.h"
#include "PLYLoader.h"
#include "OFFLoader.h"
#include "PMXLoader.h"
#include "FBXLoader.h"
#include "GLTFLoader.h"
#include "ColladaLoader.h"
#include "ThreeMFLoader.h"
#include "LASLoader.h"
#include "E57Loader.h"
#include "USDLoader.h"
#include "../utils/FileUtils.h"
#include <stdexcept>

namespace prism {

const std::vector<std::string>& ModelLoader::supportedExtensions() {
    static const std::vector<std::string> exts = {
        ".obj", ".stl", ".ply", ".off", ".pmx",
        ".fbx", ".gltf", ".glb", ".dae", ".3mf",
        ".las", ".laz", ".e57",
        ".usd", ".usda", ".usdc", ".usdz"};
    return exts;
}

std::unique_ptr<Mesh> ModelLoader::load(const std::filesystem::path& filepath) {
    std::string ext = getExtension(filepath);
    if (ext == ".obj") return loadOBJ(filepath);
    if (ext == ".stl") return loadSTL(filepath);
    if (ext == ".ply") return loadPLY(filepath);
    if (ext == ".off") return loadOFF(filepath);
    if (ext == ".pmx") return loadPMX(filepath);
    if (ext == ".fbx") return loadFBX(filepath);
    if (ext == ".gltf" || ext == ".glb") return loadGLTF(filepath);
    if (ext == ".dae") return loadDAE(filepath);
    if (ext == ".3mf") return load3MF(filepath);
    if (ext == ".las" || ext == ".laz") return loadLAS(filepath);
    if (ext == ".e57") return loadE57(filepath);
    if (ext == ".usd" || ext == ".usda" || ext == ".usdc" || ext == ".usdz") return loadUSD(filepath);
    throw std::runtime_error("Unsupported model format: " + ext +
                             " (supported: .obj .stl .ply .off .pmx .fbx .gltf .glb .dae .3mf .las .laz .e57 .usd .usda .usdc .usdz)");
}

} // namespace prism
