// src/model/STLLoader.h
#pragma once

#include "Mesh.h"
#include <memory>
#include <string>

namespace prism {

// 自动判 ASCII / Binary STL
std::unique_ptr<Mesh> loadSTL(const std::string& path);

} // namespace prism
