cmake_minimum_required(VERSION 3.5.0)
project(prismviewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NATIVE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
# 路径: harmony/entry/src/main/cpp -> 上溯 5 级 = browser
set(PRISM_SRC ${NATIVE_ROOT}/../../../../../src)
set(PRISM_TP  ${PRISM_SRC}/../third_party)

option(PRISM_ENABLE_CORE "Link PrismViewer model/renderer core" ON)

# ---------------------------------------------------------------------------
# 交叉编译的第三方静态库 (由 tools/build_deps.py 产出)
# third_party/<lib>/{include, lib/<abi>/lib<lib>.a}
# 据此决定启用哪些"需要第三方库"的模型格式
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(_prism_abi "arm64-v8a")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(_prism_abi "x86_64")
else()
    set(_prism_abi "")
endif()

set(_tp_includes "")
set(_tp_libs "")
set(_tp_defs "")

foreach(_lib pugixml laszip assimp)
    if(EXISTS "${PRISM_TP}/${_lib}/include")
        list(APPEND _tp_includes "${PRISM_TP}/${_lib}/include")
    endif()
endforeach()

if(_prism_abi)
    if(EXISTS "${PRISM_TP}/pugixml/lib/${_prism_abi}/libpugixml.a")
        list(APPEND _tp_libs "${PRISM_TP}/pugixml/lib/${_prism_abi}/libpugixml.a")
        list(APPEND _tp_defs  PRISM_HAVE_PUGIXML=1)
    endif()
    if(EXISTS "${PRISM_TP}/laszip/lib/${_prism_abi}/liblaszip.a")
        list(APPEND _tp_libs "${PRISM_TP}/laszip/lib/${_prism_abi}/liblaszip.a")
        list(APPEND _tp_defs  PRISM_HAVE_LASZIP=1)
    endif()
    if(EXISTS "${PRISM_TP}/assimp/lib/${_prism_abi}/libassimp.a")
        list(APPEND _tp_libs "${PRISM_TP}/assimp/lib/${_prism_abi}/libassimp.a")
        list(APPEND _tp_defs  PRISM_HAVE_ASSIMP=1)
        # assimp 自带的 zlib 是独立静态库, 必须一并链接
        if(EXISTS "${PRISM_TP}/assimp/lib/${_prism_abi}/libzlib.a")
            list(APPEND _tp_libs "${PRISM_TP}/assimp/lib/${_prism_abi}/libzlib.a")
        endif()
    endif()
endif()
message(STATUS "prism: abi=${_prism_abi} defs=[${_tp_defs}]")

# ---------------------------------------------------------------------------
# 内核源码
# ---------------------------------------------------------------------------
set(PRISM_CORE_SOURCES
    # 模型层 (平台无关)
    ${PRISM_SRC}/model/Mesh.cpp
    ${PRISM_SRC}/model/ModelLoader.cpp
    ${PRISM_SRC}/model/OBJLoader.cpp
    ${PRISM_SRC}/model/STLLoader.cpp
    ${PRISM_SRC}/model/PLYLoader.cpp
    ${PRISM_SRC}/model/OFFLoader.cpp
    ${PRISM_SRC}/model/PMXLoader.cpp
    ${PRISM_SRC}/model/Procedural.cpp
    # 渲染层 (GL 经 GLResources.h 适配到 GLES3)
    ${PRISM_SRC}/renderer/Shader.cpp
    ${PRISM_SRC}/renderer/MeshRenderer.cpp
    ${PRISM_SRC}/renderer/FrameBuffer.cpp
    ${PRISM_SRC}/renderer/Grid.cpp
    # 工具层
    ${PRISM_SRC}/utils/FileUtils.cpp
    ${PRISM_SRC}/utils/I18n.cpp
    ${PRISM_SRC}/utils/StbWrite.cpp
    ${PRISM_SRC}/utils/StbImage.cpp
    # 待接入: ui/Picker.cpp
)

# 需要第三方库的 loader
if("PRISM_HAVE_PUGIXML=1" IN_LIST _tp_defs)
    list(APPEND PRISM_CORE_SOURCES ${PRISM_SRC}/model/E57Loader.cpp)
    message(STATUS "启用格式: E57 (pugixml)")
endif()
if("PRISM_HAVE_LASZIP=1" IN_LIST _tp_defs)
    list(APPEND PRISM_CORE_SOURCES ${PRISM_SRC}/model/LASLoader.cpp)
    message(STATUS "启用格式: LAS/LAZ (laszip)")
endif()
if("PRISM_HAVE_ASSIMP=1" IN_LIST _tp_defs)
    list(APPEND PRISM_CORE_SOURCES
        ${PRISM_SRC}/model/AssimpCommon.cpp
        ${PRISM_SRC}/model/FBXLoader.cpp
        ${PRISM_SRC}/model/GLTFLoader.cpp
        ${PRISM_SRC}/model/ColladaLoader.cpp
        ${PRISM_SRC}/model/ThreeMFLoader.cpp
    )
    message(STATUS "启用格式: FBX/glTF/DAE/3MF (assimp)")
endif()

# ---------------------------------------------------------------------------
# 嵌入式着色器头 (鸿蒙沙箱内无稳定资源目录 -> 编译进 .so)
# ---------------------------------------------------------------------------
set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_gen_dir}")
set(_embed_h "${_gen_dir}/shaders_embedded.h")

file(GLOB _shader_files
    "${PRISM_SRC}/renderer/shaders/*.vert"
    "${PRISM_SRC}/renderer/shaders/*.frag")
list(SORT _shader_files)

file(WRITE "${_embed_h}"
"// 自动生成 — 勿手改。由 harmony/entry/src/main/cpp/CMakeLists.txt 生成。
#pragma once
#include <string>

namespace prism {

struct EmbeddedShaderEntry { const char* name; const char* src; };

inline const EmbeddedShaderEntry kEmbeddedShaders[] = {
")
foreach(_f IN LISTS _shader_files)
    get_filename_component(_n "${_f}" NAME)
    file(READ "${_f}" _c)
    string(REPLACE "\\" "\\\\" _c "${_c}")
    string(REPLACE "\"" "\\\"" _c "${_c}")
    string(REPLACE "\n" "\\n" _c "${_c}")
    file(APPEND "${_embed_h}" "    { \"${_n}\", \"${_c}\" },\n")
endforeach()
file(APPEND "${_embed_h}"
"};

inline std::string embeddedShaderByName(const std::string& fileName) {
    for (const auto& e : kEmbeddedShaders) {
        if (fileName == e.name) return std::string(e.src);
    }
    return std::string();
}

} // namespace prism
")

# ---------------------------------------------------------------------------
# 源文件
# ---------------------------------------------------------------------------
file(GLOB PLATFORM_SOURCES ${NATIVE_ROOT}/platform/*.cpp)

set(ENTRY_SOURCES
    napi_init.cpp
    ${PLATFORM_SOURCES}
)
if(PRISM_ENABLE_CORE)
    list(APPEND ENTRY_SOURCES ${PRISM_CORE_SOURCES})
endif()

# imgui (源码编译进 .so; GLES3 后端)
set(_imgui_dir "${PRISM_TP}/imgui")
set(PRISM_HAVE_IMGUI OFF)
if(EXISTS "${_imgui_dir}/imgui.cpp")
    set(PRISM_HAVE_IMGUI ON)
    list(APPEND ENTRY_SOURCES
        ${_imgui_dir}/imgui.cpp
        ${_imgui_dir}/imgui_draw.cpp
        ${_imgui_dir}/imgui_tables.cpp
        ${_imgui_dir}/imgui_widgets.cpp
        ${_imgui_dir}/backends/imgui_impl_opengl3.cpp
    )
    if(PRISM_ENABLE_CORE)
        list(APPEND ENTRY_SOURCES ${PRISM_SRC}/ui/Panel.cpp)
    endif()
    message(STATUS "imgui: ${_imgui_dir}")
else()
    message(STATUS "imgui 未找到 (跳过控制面板): ${_imgui_dir}")
endif()

add_library(entry SHARED ${ENTRY_SOURCES})

target_include_directories(entry PRIVATE
    ${NATIVE_ROOT}
    ${NATIVE_ROOT}/platform
    ${PRISM_SRC}
    ${_gen_dir}
    ${_tp_includes}
)

# glm / stb 头 (构建脚本同步到 third_party/include)
if(DEFINED THIRD_PARTY_INCLUDE AND NOT THIRD_PARTY_INCLUDE STREQUAL "")
    target_include_directories(entry PRIVATE "${THIRD_PARTY_INCLUDE}")
endif()
if(EXISTS "${PRISM_TP}/include")
    target_include_directories(entry PRIVATE "${PRISM_TP}/include")
endif()

if(PRISM_HAVE_IMGUI)
    target_include_directories(entry PRIVATE
        ${_imgui_dir}
        ${_imgui_dir}/backends
    )
    target_compile_definitions(entry PRIVATE
        PRISM_HAVE_IMGUI=1
        IMGUI_IMPL_OPENGL_ES3
        IMGUI_DEFINE_MATH_OPERATORS
    )
endif()

target_compile_definitions(entry PRIVATE PRISM_OHOS=1 ${_tp_defs})

# OHOS toolchain 会注入 --gcc-toolchain, 但 clang 15 在本配置下走自带 libc++/sysroot,
# 于是每次编译都刷 "argument unused" 警告 —— 仅为噪音, 此处静音(不影响代码生成)。
target_compile_options(entry PRIVATE -Wno-unused-command-line-argument)

target_link_libraries(entry PUBLIC
    libace_napi.z.so
    libace_ndk.z.so
    libnative_window.so
    libnative_buffer.so
    libEGL.so
    libGLESv3.so
    libhilog_ndk.z.so
)

if(_tp_libs)
    target_link_libraries(entry PRIVATE ${_tp_libs})
endif()
