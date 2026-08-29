cmake_minimum_required(VERSION 3.20)
project(PrismViewer
    VERSION 0.4.1
    DESCRIPTION "棱镜模型查看器"
    LANGUAGES CXX)

# ---------- 编译选项 ----------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------- 全局编译选项 ----------
add_compile_options(
    /W4           # warning level 4
    /permissive-  # strict standard
    /utf-8        # UTF-8 source/exec charset
)

# MSVC optimization flags per config (generator-expression list)
# Release: /O2 速度优化 /Oi 内建函数 /Ob3 激进内联 /GL+LTCG 全程序优化
#          /Gy 函数级链接 /Gw 全局数据合并 /Zc:inline 剔除未用内联 (配合链接器去死代码)
set(_msvc_release_opts /O2 /Oi /Ob3 /GL /Gy /Gw /Zc:inline)
set(_msvc_debug_opts   /Od /Zi)
add_compile_options(
    "$<$<CONFIG:Release>:${_msvc_release_opts}>"
    "$<$<CONFIG:Debug>:${_msvc_debug_opts}>"
)

# Release 链接优化: 死代码剔除 + 相同代码折叠 (对静态链接的大型库如 Assimp 收益显著)
set(_msvc_release_link_opts /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO)
add_link_options(
    "$<$<CONFIG:Release>:${_msvc_release_link_opts}>"
)

# CRT: match vcpkg static libs (/MT); avoid LNK4098 conflicts.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---------- 通过 vcpkg 找依赖 ----------
# 注意: vcpkg toolchain 由 build.ps1 通过 -DCMAKE_TOOLCHAIN_FILE 传入
find_package(glfw3  CONFIG REQUIRED)
find_package(GLEW   CONFIG REQUIRED)
find_package(glm    CONFIG REQUIRED)
find_package(imgui  CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)

# stb is header-only in vcpkg; no CMake config shipped.
# Use its include directory directly.
find_path(Stb_INCLUDE_DIR
    NAMES stb_image_write.h
    PATHS ${VCPKG_INSTALLED_DIR}/x64-windows-static/include
    NO_DEFAULT_PATH
)
if(NOT Stb_INCLUDE_DIR)
    message(FATAL_ERROR "stb headers not found in vcpkg install tree")
endif()
message(STATUS "stb headers: ${Stb_INCLUDE_DIR}")

find_package(OpenGL REQUIRED)

# ---------- 可执行文件 ----------
add_executable(PrismViewer
    src/main.cpp

    src/app/Viewer.h
    src/app/Viewer.cpp
    src/app/OrbitCamera.h
    src/app/OrbitCamera.cpp

    src/renderer/GLResources.h
    src/renderer/Shader.h
    src/renderer/Shader.cpp
    src/renderer/MeshRenderer.h
    src/renderer/MeshRenderer.cpp
    src/renderer/FrameBuffer.h
    src/renderer/FrameBuffer.cpp
    src/renderer/Grid.h
    src/renderer/Grid.cpp

    src/model/Mesh.h
    src/model/Mesh.cpp
    src/model/PmxData.h
    src/model/MeshWriter.h
    src/model/MeshWriter.cpp
    src/model/ModelLoader.h
    src/model/ModelLoader.cpp
    src/model/OBJLoader.h
    src/model/OBJLoader.cpp
    src/model/STLLoader.h
    src/model/STLLoader.cpp
    src/model/PLYLoader.h
    src/model/PLYLoader.cpp
    src/model/OFFLoader.h
    src/model/OFFLoader.cpp
    src/model/AssimpCommon.h
    src/model/AssimpCommon.cpp
    src/model/FBXLoader.h
    src/model/FBXLoader.cpp
    src/model/GLTFLoader.h
    src/model/GLTFLoader.cpp
    src/model/PMXLoader.h
    src/model/PMXLoader.cpp
    src/model/Procedural.h
    src/model/Procedural.cpp

    src/ui/Panel.h
    src/ui/Panel.cpp
    src/ui/Picker.h
    src/ui/Picker.cpp

    src/utils/FileUtils.h
    src/utils/FileUtils.cpp
    src/utils/StbWrite.h
    src/utils/StbWrite.cpp
    src/utils/StbImage.h
    src/utils/StbImage.cpp
    src/utils/I18n.h
    src/utils/I18n.cpp
    src/utils/ImeGuard.h
    src/utils/ImeGuard.cpp
)

target_include_directories(PrismViewer PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR}/shaders
    ${Stb_INCLUDE_DIR}
)

# 生成 shader 资源 (用 CMake 编译为字节数组)
set(SHADER_FILES
    src/renderer/shaders/mesh.vert
    src/renderer/shaders/mesh.frag
    src/renderer/shaders/line.vert
    src/renderer/shaders/line.frag
    src/renderer/shaders/picker.vert
    src/renderer/shaders/picker.frag
    src/renderer/shaders/flat.vert
    src/renderer/shaders/flat.frag
    src/renderer/shaders/grid.vert
    src/renderer/shaders/grid.frag
)

# 复制 shader 到 build 目录,运行时读取
add_custom_command(
    TARGET PrismViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/shaders
        $<TARGET_FILE_DIR:PrismViewer>/shaders
)

# 复制 assets 到 build 目录
add_custom_command(
    TARGET PrismViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:PrismViewer>/assets
)

target_link_libraries(PrismViewer PRIVATE
    glfw
    GLEW::GLEW
    glm::glm
    imgui::imgui
    assimp::assimp
    OpenGL::GL
    opengl32
    gdi32
    user32
    shell32   # DragQueryFileW
    imm32     # ImeGuard (ImmAssociateContext / ImmCreateContext)
)

# GLEW 静态链接需要这个宏
target_compile_definitions(PrismViewer PRIVATE
    GLEW_STATIC
    IMGUI_DEFINE_MATH_OPERATORS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

# ---------- 安装 ----------
install(TARGETS PrismViewer
    RUNTIME DESTINATION bin
)
install(DIRECTORY assets          DESTINATION bin)
install(DIRECTORY src/renderer/shaders DESTINATION bin/shaders)
