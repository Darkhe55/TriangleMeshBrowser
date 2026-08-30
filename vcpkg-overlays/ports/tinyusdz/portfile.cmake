# PrismViewer overlay port: tinyusdz (USD/USDZ 读取)
# 上游无 install 规则, 此处手工安装静态库与头文件。
# 瘦身: 关闭音频/纹理格式/MTLX/各类格式导入器/写出器等非必要模块,
#       只保留 USDA/USDC 读取 + Tydra RenderScene 转换 (含 USDZ 容器解析)。
set(VCPKG_BUILD_TYPE release)  # 仅 Release, 缩短构建时间

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO lighttransport/tinyusdz
    REF v1.0.0-rc3
    SHA512 47a939326f206abbe6bf8841d1d7377563a621015df8f129c6f7720506f758f5d762a35f5c6c860f5e82b574baa88ed02312f0bea1f6286514059900a13d8996
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DTINYUSDZ_BUILD_TESTS=OFF
        -DTINYUSDZ_BUILD_EXAMPLES=OFF
        -DTINYUSDZ_BUILD_BENCHMARKS=OFF
        -DTINYUSDZ_BUILD_TOOLS=OFF
        -DTINYUSDZ_BUILD_GUI_VIEWER=OFF
        -DTINYUSDZ_BUILD_QUICKLOOK=OFF
        -DTINYUSDZ_BUILD_TEXTURE_GPU_BENCH=OFF
        -DTINYUSDZ_WITH_C_API=OFF
        -DTINYUSDZ_WITH_PXR_COMPAT_API=OFF
        -DTINYUSDZ_WITH_AUDIO=OFF
        -DTINYUSDZ_WITH_ALAC_AUDIO=OFF
        -DTINYUSDZ_WITH_TIFF=OFF
        -DTINYUSDZ_WITH_EXR=OFF
        -DTINYUSDZ_WITH_TEXTOOLS=OFF
        -DTINYUSDZ_WITH_COLORIO=OFF
        -DTINYUSDZ_WITH_USDMTLX=OFF
        -DTINYUSDZ_WITH_USD_TO_GLTF=OFF
        -DTINYUSDZ_WITH_USDOBJ=OFF
        -DTINYUSDZ_WITH_USDVOX=OFF
        -DTINYUSDZ_WITH_USDVOL=OFF
        -DTINYUSDZ_WITH_MESHOPT=OFF
        -DTINYUSDZ_WITH_QJS=OFF
        -DTINYUSDZ_WITH_WAMR=OFF
        -DTINYUSDZ_WITH_MCP_SERVER=OFF
        -DTINYUSDZ_WITH_FPNGE=OFF
        -DTINYUSDZ_WITH_GEOGRAM=OFF
        -DTINYUSDZ_WITH_PYTHON=OFF
        -DTINYUSDZ_WITH_MODULE_USDA_WRITER=OFF
        -DTINYUSDZ_WITH_MODULE_USDC_WRITER=OFF
        -DTINYUSDZ_ENABLE_THREAD=OFF
        -DTINYUSDZ_PRODUCTION_BUILD=ON
)

# 上游没有 install 规则; 只构建静态库目标后手工安装
vcpkg_cmake_build(TARGET tinyusdz_static)

file(GLOB_RECURSE TINYUSDZ_LIBS
    "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/*tinyusdz_static.lib")
if(NOT TINYUSDZ_LIBS)
    message(FATAL_ERROR "tinyusdz_static.lib not found after build")
endif()
list(GET TINYUSDZ_LIBS 0 TINYUSDZ_LIB)
file(INSTALL "${TINYUSDZ_LIB}" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")

# 头文件树 (包含根目录即满足 "tinyusdz.hh" / "tydra/xxx.hh" 的包含形式)
file(INSTALL "${SOURCE_PATH}/src/"
    DESTINATION "${CURRENT_PACKAGES_DIR}/include"
    FILES_MATCHING
        PATTERN "*.hh"
        PATTERN "*.h"
        PATTERN "*.hpp"
        PATTERN "*.inc"
)

# 上游 tydra/shape-to-mesh.hh 用 "#include \"../../src/math-util.inc\"" 的
# 相对路径引用兄弟头文件。安装到 include/ 后该路径会跨到包根的 src/ 树,
# 若同时安装 src/ 树则 value-types.hh / math-util.inc 等会从两个不同路径
# 被包含, #pragma once 按路径去重失效 → 重定义错误 (C2374/C2011)。
# 解决: 改写为 "#include \"../...\"" 使其落在 include/ 树内 (同一物理文件),
#       并完全取消 src/ 树的安装。
file(READ "${CURRENT_PACKAGES_DIR}/include/tydra/shape-to-mesh.hh" _shape_to_mesh)
string(REPLACE "#include \"../../src/" "#include \"../" _shape_to_mesh "${_shape_to_mesh}")
file(WRITE "${CURRENT_PACKAGES_DIR}/include/tydra/shape-to-mesh.hh" "${_shape_to_mesh}")


vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
