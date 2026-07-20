// src/utils/FileUtils.h
// 文件读取 / 路径 / 扩展名工具
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace prism {

// 读取文本文件,失败抛 runtime_error
std::string readFileText(const std::string& path);

// 读取二进制文件,失败抛 runtime_error
std::vector<std::uint8_t> readFileBinary(const std::string& path);

// 取扩展名(小写,不含点)
std::string getExtension(const std::string& path);

// 取文件名(去掉目录)
std::string getFileName(const std::string& path);

// 写整个文件
void writeFile(const std::string& path, const std::string& content);

// join 路径
std::string joinPath(const std::string& a, const std::string& b);

// 是否存在
bool fileExists(const std::string& path);

// 程序所在目录
std::string getExecutableDir();

// 在程序目录的相对路径
std::string resourcePath(const std::string& relative);

} // namespace prism
