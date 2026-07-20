// src/main.cpp
// 棱镜模型查看器入口
#include "app/Viewer.h"
#include "utils/FileUtils.h"
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    try {
        prism::Viewer viewer(1920, 1080, "棱镜模型查看器");

        if (argc > 1) {
            std::string arg = argv[1];
            // 去掉引号
            if (arg.size() >= 2 && arg.front() == '"' && arg.back() == '"')
                arg = arg.substr(1, arg.size() - 2);
            if (!viewer.loadModel(arg)) {
                std::cerr << "无法加载: " << arg << "\n";
            }
        }

        viewer.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}
