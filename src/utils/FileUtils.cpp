// src/utils/FileUtils.cpp
#include "FileUtils.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace prism {

std::string readFileText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<std::uint8_t> readFileBinary(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open file: " + path);
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<size_t>(size));
    if (size > 0) f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot write file: " + path);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string getExtension(const std::string& path) {
    fs::path p(path);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

std::string getFileName(const std::string& path) {
    return fs::path(path).filename().string();
}

std::string joinPath(const std::string& a, const std::string& b) {
    fs::path p = fs::path(a) / fs::path(b);
    return p.lexically_normal().string();
}

bool fileExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

std::string getExecutableDir() {
#ifdef _WIN32
    char buf[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        // 兜底:当前工作目录
        return fs::current_path().string();
    }
    fs::path p(buf);
    return p.parent_path().string();
#else
    return fs::current_path().string();
#endif
}

std::string resourcePath(const std::string& relative) {
    return joinPath(getExecutableDir(), relative);
}

} // namespace prism
