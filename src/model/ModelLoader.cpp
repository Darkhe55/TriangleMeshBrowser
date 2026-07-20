// src/model/ModelLoader.cpp
#include "ModelLoader.h"
#include "OBJLoader.h"
#include "STLLoader.h"
#include "PLYLoader.h"
#include "OFFLoader.h"
#include "../utils/FileUtils.h"
#include <stdexcept>

namespace prism {

const std::vector<std::string>& ModelLoader::supportedExtensions() {
    static const std::vector<std::string> exts = {".obj", ".stl", ".ply", ".off"};
    return exts;
}

std::unique_ptr<Mesh> ModelLoader::load(const std::string& filepath) {
    std::string ext = getExtension(filepath);
    if (ext == ".obj") return loadOBJ(filepath);
    if (ext == ".stl") return loadSTL(filepath);
    if (ext == ".ply") return loadPLY(filepath);
    if (ext == ".off") return loadOFF(filepath);
    throw std::runtime_error("Unsupported model format: " + ext +
                             " (supported: .obj .stl .ply .off)");
}

} // namespace prism
