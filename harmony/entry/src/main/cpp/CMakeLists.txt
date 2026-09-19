cmake_minimum_required(VERSION 3.5.0)
project(prismviewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NATIVE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# ---- 复用桌面工程中平台无关的源码 (browser/src) ----
# 路径: browser/harmony/entry/src/main/cpp -> 上溯 5 级 = browser
set(PRISM_SRC ${NATIVE_ROOT}/../../../../../src)

# 说明: PRISM_ENABLE_CORE=ON 时接入完整模型/渲染/UI 内核 (逐批适配中, 默认关闭以保证
#       HarmonyOS 构建链可独立跑通)。适配完成后将设为默认 ON。
option(PRISM_ENABLE_CORE "Link the full PrismViewer core (model/renderer/ui)" OFF)

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
    # 渲染层 (GL 调用经 GlCompat 适配到 GLES3)
    ${PRISM_SRC}/renderer/Shader.cpp
    ${PRISM_SRC}/renderer/MeshRenderer.cpp
    ${PRISM_SRC}/renderer/FrameBuffer.cpp
    ${PRISM_SRC}/renderer/Grid.cpp
    # UI 层
    ${PRISM_SRC}/ui/Panel.cpp
    ${PRISM_SRC}/ui/Picker.cpp
    # 工具层 (I18n/Stb 平台无关)
    ${PRISM_SRC}/utils/I18n.cpp
    ${PRISM_SRC}/utils/StbWrite.cpp
    ${PRISM_SRC}/utils/StbImage.cpp
)

# ---- 平台适配层 (HarmonyOS XComponent + EGL/GLES3) ----
file(GLOB PLATFORM_SOURCES ${NATIVE_ROOT}/platform/*.cpp)

set(ENTRY_SOURCES
    napi_init.cpp
    ${PLATFORM_SOURCES}
)

if(PRISM_ENABLE_CORE)
    list(APPEND ENTRY_SOURCES ${PRISM_CORE_SOURCES})
endif()

add_library(entry SHARED ${ENTRY_SOURCES})

target_include_directories(entry PRIVATE
    ${NATIVE_ROOT}
    ${NATIVE_ROOT}/platform
    ${PRISM_SRC}
)

# 第三方头文件 (stb 等) — 由 CMake 参数 THIRD_PARTY_INCLUDE 传入
if(DEFINED THIRD_PARTY_INCLUDE AND NOT THIRD_PARTY_INCLUDE STREQUAL "")
    target_include_directories(entry PRIVATE ${THIRD_PARTY_INCLUDE})
endif()

target_compile_definitions(entry PRIVATE PRISM_OHOS=1)

target_link_libraries(entry PUBLIC
    libace_napi.z.so
    libace_ndk.z.so
    libnative_window.so
    libnative_buffer.so
    libEGL.so
    libGLESv3.so
    libhilog_ndk.z.so
)
