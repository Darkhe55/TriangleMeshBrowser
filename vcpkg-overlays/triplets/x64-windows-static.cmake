# vcpkg-overlays/triplets/x64-windows-static.cmake
# 覆盖默认 x64-windows-static triplet: 全静态 + 链接期死代码剔除优化。
# /Gy 函数级链接 + /Gw 全局数据合并 + /GL 全程序优化:
#   使静态库成员可被最终链接器的 /LTCG /OPT:REF 精细剔除未引用代码,
#   对 xerces-c 等大型依赖的体积削减至关重要。
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_C_FLAGS   "/Gy /Gw /GL")
set(VCPKG_CXX_FLAGS "/Gy /Gw /GL")
