// src/model/PLYLoader.h
#pragma once

#include "Mesh.h"
#include <memory>
#include <string>

namespace prism {

// 支持 ASCII / binary_little_endian PLY
std::unique_ptr<Mesh> loadPLY(const std::string& path);

} // namespace prism
