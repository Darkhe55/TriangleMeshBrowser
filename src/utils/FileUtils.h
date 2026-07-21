// src/utils/FileUtils.h
// 文件读取 / 路径 / 扩展名工具
// 路径统一用 std::filesystem::path,在 Windows 上从 std::wstring 构造,
// 避免 ANSI→UTF-16 转换在中文路径上失败("no mapping for the Unicode character exists")
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace prism {

namespace fs = std::filesystem;

// Windows: 把 ANSI(系统代码页, 中文系统 = CP936)字节转成 UTF-16 宽字符串,
//          用于从 GLFW drop / argv / GetOpenFileNameA 拿到的 char* 路径转换
// 非 Windows: 平台原生 UTF-8,直接拷贝
std::wstring ansiToWide(const std::string& ansiPath);

// Windows 宽字符路径 → UTF-8 字符串,用于错误信息显示
std::string  wideToUtf8(const std::wstring& widePath);

// 读取文本文件,失败抛 runtime_error
std::string readFileText(const fs::path& path);

// 读取二进制文件,失败抛 runtime_error
std::vector<std::uint8_t> readFileBinary(const fs::path& path);

// 取扩展名(小写,含点;如 ".ply")
std::string getExtension(const fs::path& path);

// 取文件名(去掉目录)
std::string getFileName(const fs::path& path);

// 写整个文件
void writeFile(const fs::path& path, const std::string& content);

// join 路径(都按 fs::path 处理)
fs::path joinPath(const fs::path& a, const fs::path& b);

// 是否存在
bool fileExists(const fs::path& path);

// 程序所在目录
fs::path getExecutableDir();

// 在程序目录的相对路径
fs::path resourcePath(const std::string& relative);

} // namespace prism
