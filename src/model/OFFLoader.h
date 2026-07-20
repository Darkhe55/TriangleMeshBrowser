// src/model/OFFLoader.h
#pragma once

#include "Mesh.h"
#include <memory>
#include <string>

namespace prism {

// 解析 OFF / OFFn 格式
std::unique_ptr<Mesh> loadOFF(const std::string& path);

} // namespace prism
