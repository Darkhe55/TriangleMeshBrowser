// src/utils/I18n.cpp
#include "I18n.h"
#include <cstring>

namespace prism::i18n {

namespace {

Language s_lang = Language::Zh;  // 默认中文

// 条目: { key, 中文, 英文 }
// 注意: 含 %u/%s 的条目,两种语言必须保持相同的格式占位符
struct Entry {
    const char* key;
    const char* zh;
    const char* en;
};

constexpr Entry kEntries[] = {
    // ---- 窗口 / 状态栏 ----
    {"app.title",              "棱镜模型查看器",            "Prism Model Viewer"},
    {"status.appName",         "  棱镜模型查看器",          "  Prism Model Viewer"},
    {"status.noModel",         "未加载",                    "No model"},

    // ---- 菜单栏 ----
    {"menu.settings",          "设置",                      "Settings"},
    {"menu.file",              "文件",                      "File"},
    {"menu.geometry",          "几何",                      "Geometry"},
    {"menu.console",           "控制台",                    "Console"},
    {"menu.help",              "帮助",                      "Help"},

    // ---- 设置弹出菜单 ----
    {"settings.showAxis",      "显示坐标轴",                "Show axes"},
    {"settings.showGrid",      "显示 XOY 网格",             "Show XOY grid"},
    {"settings.showStatusBar", "显示状态栏",                "Show status bar"},
    {"settings.mouseRotation", "鼠标旋转",                  "Mouse rotation"},
    {"settings.invertX",       "反转 X 轴",                 "Invert X axis"},
    {"settings.invertY",       "反转 Y 轴",                 "Invert Y axis"},
    {"settings.wasd",          "WASD 飞行",                 "WASD fly"},
    {"settings.enableWasd",    "启用 WASD 移动",            "Enable WASD movement"},
    {"settings.moveSpeed",     "速度 (单位/秒)",            "Speed (units/sec)"},
    {"settings.wasdTip",       "按住 Shift = 3x 加速",      "Hold Shift = 3x speed"},
    {"settings.language",      "语言",                      "Language"},

    // ---- 文件菜单 ----
    {"file.open",              "打开模型...",               "Open Model..."},
    {"file.exportScreenshot",  "导出截图 (PNG)...",         "Export Screenshot (PNG)..."},
    {"file.exportModel",       "导出模型...",               "Export Model..."},
    {"file.closeModel",        "关闭模型",                  "Close Model"},

    // ---- 几何菜单 ----
    {"geom.0",                 "立方体",                    "Cube"},
    {"geom.1",                 "球体",                      "Sphere"},
    {"geom.2",                 "圆柱",                      "Cylinder"},
    {"geom.3",                 "圆环",                      "Torus"},
    {"geom.4",                 "圆锥",                      "Cone"},

    // ---- 控制台菜单(子板块开关) ----
    {"section.modelInfo",      "模型信息",                  "Model Info"},
    {"section.colors",         "颜色",                      "Colors"},
    {"section.lighting",       "光照",                      "Lighting"},
    {"section.fog",            "雾效",                      "Fog"},
    {"section.rightLighting",  "右视口光照",                "Right Viewport Lighting"},
    {"section.picking",        "拾取",                      "Picking"},
    {"section.pmx",            "材质",                      "Materials"},
    {"section.view",           "视图",                      "View"},

    // ---- 帮助菜单 ----
    {"help.leftDrag",          "左键拖拽:旋转",             "Left-drag: rotate"},
    {"help.panDrag",           "中键/右键拖拽:平移",        "Middle/Right-drag: pan"},
    {"help.scroll",            "滚轮:缩放",                 "Scroll: zoom"},
    {"help.shortcut",          "F: 复位相机 / Shift+F: 框选", "F: reset camera / Shift+F: frame"},
    {"help.pick",              "点击模型: 高亮三角面",      "Click model: highlight triangle"},
    {"help.drop",              "拖文件到窗口: 直接打开",    "Drop file on window: open it"},
    {"help.wasd",              "WASD / 方向键: 平移视点",   "WASD / arrows: pan viewpoint"},
    {"help.qe",                "Q / E: 上下",               "Q / E: down / up"},
    {"help.space",             "Space: 上升 (E 等价)",      "Space: up (same as E)"},
    {"help.shiftWasd",         "Shift + WASD: 3x 加速",     "Shift + WASD: 3x speed"},

    // ---- 控制台:模型信息 ----
    {"console.title",          "控制台",                    "Console"},
    {"info.noModel",           "(无模型)",                  "(No model)"},
    {"info.file",              "文件: %s",                  "File: %s"},
    {"info.vertices",          "顶点数: %u",                "Vertices: %u"},
    {"info.triangles",         "面  数: %u",                "Triangles: %u"},
    {"info.bbox",              "包围盒:",                   "Bounding box:"},

    // ---- 控制台:颜色 ----
    {"colors.baseColor",       "面片颜色",                  "Face color"},
    {"colors.fogColor",        "雾色",                      "Fog color"},

    // ---- 控制台:光照 ----
    {"lighting.lightColor",    "光源色",                    "Light color"},
    {"lighting.fillColor",     "补光色",                    "Fill color"},
    {"lighting.ambient",       "环境光",                    "Ambient"},
    {"lighting.fill",          "补光",                      "Fill"},
    {"lighting.specular",      "高光",                      "Specular"},
    {"lighting.lightDir",      "光源方向",                  "Light direction"},

    // ---- 控制台:雾效 ----
    {"fog.enable",             "启用雾效",                  "Enable fog"},
    {"fog.near",               "雾近",                      "Fog near"},
    {"fog.far",                "雾远",                      "Fog far"},

    // ---- 控制台:右视口光照 ----
    {"right.baseColor",        "右面片色",                  "Right face color"},
    {"right.ambient",          "右环境光",                  "Right ambient"},
    {"right.lightDir",         "右光源方向",                "Right light direction"},
    {"right.fogNear",          "右雾近",                    "Right fog near"},
    {"right.fogFar",           "右雾远",                    "Right fog far"},

    // ---- 控制台:拾取 ----
    {"picking.face",           "高亮面: #%u",               "Highlighted face: #%u"},
    {"picking.clear",          "取消高亮",                  "Clear highlight"},
    {"picking.hint",           "(点击模型高亮三角面)",      "(Click model to highlight a triangle)"},

    // ---- 控制台:材质 (PMX / FBX / glTF / GLB 通用) ----
    {"pmx.layers",             "材质层",                    "Material layers"},
    {"pmx.tex",                "Tex 贴图",                  "Texture map"},
    {"pmx.toon",               "Toon 材质",                 "Toon shading"},
    {"pmx.sphere",             "Sph 球面贴图",              "Sph sphere map"},
    {"pmx.adjust",             "基础调整",                  "Basic adjustments"},
    {"pmx.alpha",              "透明度",                    "Opacity"},
    {"pmx.colorOverride",      "颜色覆盖",                  "Color override"},
    {"pmx.overrideColor",      "覆盖色",                    "Override color"},
    {"pmx.edge",               "边缘 (Edge)",               "Edge"},
    {"pmx.edgeWidth",          "边缘宽度",                  "Edge width"},

    // ---- 控制台:视图 ----
    {"view.reset",             "复位相机 (F)",              "Reset camera (F)"},
    {"view.fit",               "框选模型 (Shift+F)",        "Frame model (Shift+F)"},
    {"view.splitMode",         "视口模式",                  "Viewport mode"},
    {"view.split.off",         "单视口",                    "Single viewport"},
    {"view.split.models",      "对比:不同模型",             "Compare: different models"},
    {"view.split.shading",     "对比:不同光照",             "Compare: different lighting"},
    {"view.renderMode",        "渲染模式",                  "Render mode"},
    {"view.mode.solid",        "实体",                      "Solid"},
    {"view.mode.wire",         "线框",                      "Wireframe"},
    {"view.mode.solidwire",    "实体+线框",                 "Solid + wireframe"},

    // ---- 视口标签 ----
    {"viewport.main",          "主视口",                    "Main viewport"},
    {"viewport.compareModels", "对比: 不同模型",            "Compare: different models"},
    {"viewport.compareShading","对比: 不同光照",            "Compare: different lighting"},

    // ---- 提示 (toast) ----
    {"toast.generate",         "生成: ",                    "Generated: "},
    {"toast.loadFailed",       "加载失败: ",                "Load failed: "},
    {"toast.exportNoModel",    "导出失败: 当前未加载模型",  "Export failed: no model loaded"},
    {"toast.exported",         "已导出: ",                  "Exported: "},
    {"toast.exportFailed",     "导出失败: ",                "Export failed: "},
    {"toast.saved",            "已保存: ",                  "Saved: "},
    {"toast.shotFailed",       "截图失败: ",                "Screenshot failed: "},
};

} // namespace

void setLanguage(Language lang) noexcept { s_lang = lang; }
Language language() noexcept { return s_lang; }

const char* tr(const char* key) {
    for (const auto& e : kEntries) {
        if (std::strcmp(e.key, key) == 0)
            return (s_lang == Language::Zh) ? e.zh : e.en;
    }
    return key;
}

} // namespace prism::i18n
