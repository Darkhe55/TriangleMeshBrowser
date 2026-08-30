// src/model/UsdLinkStubs.cpp
// tinyusdz 链接桩 (link stubs)
//
// 背景: 本项目的 tinyusdz overlay port 在 vcpkg-overlays/ports/tinyusdz 中
// 为瘦身关闭了 USDC 写(out) 与 usdVol(VDB 体积) 模块:
//   -DTINYUSDZ_WITH_MODULE_USDC_WRITER=OFF
//   -DTINYUSDZ_WITH_USDVOL=OFF
// 但 tinyusdz 库内部若干 .obj (tinyusdz.cc.obj / render-data.cc.obj) 仍
// 无条件引用这些被关闭模块的入口, 导致最终链接时缺符号。
//
// 本项目只用 tinyusdz 做 USD 几何读取, 永不调用 USDC 写出或 VDB 体积读取,
// 故在此提供签名称兼容的桩实现以满足链接器。桩永不返回成功。
#include "USDLoader.h"  // ensure this TU is compiled into the same binary

#include <cstdint>
#include <string>
#include <vector>

// 仅满足链接; signature 必须与 tinyusdz 官方声明完全一致 (C++ 名称修饰依赖)。
namespace tinyusdz {

class Stage;
class Layer;

namespace usdc {

bool SaveAsUSDCToMemory(const Stage&, std::vector<std::uint8_t>*,
                        std::string*, std::string*,
                        std::int64_t, std::int64_t, bool) {
    return false;  // 本项目不写 USDC; 桩永失败
}

bool SaveAsUSDCToMemory(const Layer&, std::vector<std::uint8_t>*,
                        std::string*, std::string*,
                        std::int64_t, std::int64_t, bool) {
    return false;  // 本项目不写 USDC; 桩永失败
}

}  // namespace usdc

namespace usdVol {

struct VDBGrid;

bool ReadVDBFromMemory(const std::uint8_t*, std::size_t,
                       const std::string&, std::vector<VDBGrid>*,
                       std::string*, std::string*, std::size_t) {
    return false;  // 本项目不读 VDB 体积; 桩永失败
}

}  // namespace usdVol

}  // namespace tinyusdz
