// src/utils/FileUtils.cpp
#include "FileUtils.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace prism {

std::wstring ansiToWide(const std::string& ansiPath) {
#ifdef _WIN32
    if (ansiPath.empty()) return std::wstring{};
    // 先按系统代码页(中文 Windows = CP936/GBK)解码 — 这正是 GLFW GetOpenFileNameA
    // 以及 Windows 资源管理器拖拽时给到的字节流
    UINT codePage = CP_ACP;
    int wlen = MultiByteToWideChar(codePage, 0, ansiPath.c_str(),
                                   static_cast<int>(ansiPath.size()),
                                   nullptr, 0);
    if (wlen <= 0) {
        // 退路:当 UTF-8 再试(有些工具链会直接给 UTF-8 字节)
        codePage = CP_UTF8;
        wlen = MultiByteToWideChar(codePage, 0, ansiPath.c_str(),
                                   static_cast<int>(ansiPath.size()),
                                   nullptr, 0);
        if (wlen <= 0) {
            // 真的救不回来:逐字节硬塞,至少 std::ifstream 还能拿到一个 fallback
            return std::wstring{ansiPath.begin(), ansiPath.end()};
        }
    }
    std::wstring out(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(codePage, 0, ansiPath.c_str(),
                        static_cast<int>(ansiPath.size()),
                        &out[0], wlen);
    return out;
#else
    // 非 Windows:平台原生 UTF-8
    std::wstring out;
    out.reserve(ansiPath.size());
    for (unsigned char c : ansiPath) out.push_back(static_cast<wchar_t>(c));
    return out;
#endif
}

std::string wideToUtf8(const std::wstring& widePath) {
#ifdef _WIN32
    if (widePath.empty()) return std::string{};
    int len = WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(),
                                  static_cast<int>(widePath.size()),
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0) return std::string{};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(),
                        static_cast<int>(widePath.size()),
                        &out[0], len, nullptr, nullptr);
    return out;
#else
    return std::string(widePath.begin(), widePath.end());
#endif
}

std::string readFileText(const fs::path& path) {
    // 用 wstring 路径直接打开,绕开 ANSI→UTF-16 转换
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open file: " + wideToUtf8(path.wstring()));
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<std::uint8_t> readFileBinary(const fs::path& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open file: " + wideToUtf8(path.wstring()));
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<size_t>(size > 0 ? size : 0));
    if (size > 0) f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

void writeFile(const fs::path& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot write file: " + wideToUtf8(path.wstring()));
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string getExtension(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

std::string getFileName(const fs::path& path) {
    return path.filename().string();
}

fs::path joinPath(const fs::path& a, const fs::path& b) {
    fs::path p = a / b;
    return p.lexically_normal();
}

bool fileExists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

fs::path getExecutableDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return fs::current_path();
    }
    fs::path p(buf);
    return p.parent_path();
#else
    return fs::current_path();
#endif
}

fs::path resourcePath(const std::string& relative) {
    return joinPath(getExecutableDir(), fs::path(relative));
}

} // namespace prism
