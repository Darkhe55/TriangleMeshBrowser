# TriangleMeshBrowser (PrismViewer) / 三角网格浏览器

A lightweight, standalone 3D triangle-mesh browser built with C++17 + OpenGL 3.3 + GLFW + Dear ImGui.

基于 C++17 + OpenGL 3.3 + GLFW + Dear ImGui 构建的轻量级、独立的 3D 三角网格浏览器。

> **Single-file executable** — no runtime dependencies. All third-party libraries
> (OpenGL loader, GLFW, ImGui, GLM, stb) are statically linked into the binary;
> the only requirements are the standard Windows system DLLs
> (`opengl32`, `gdi32`, `user32`, `shell32`).
>
> **单文件可执行程序** — 无需任何运行时。所有第三方库（OpenGL loader、GLFW、ImGui、GLM、stb）均静态链接到二进制文件中，仅依赖 Windows 系统 DLL（`opengl32`、`gdi32`、`user32`、`shell32`）。

---

## Features / 功能

- **Multi-format loading**: OBJ, STL (ASCII + Binary), PLY (ASCII + Binary Little Endian), OFF / NOFF / COFF / CNOFF, PMX 2.0 / 2.1 (geometry only)
- **多格式加载**：OBJ、STL（ASCII + 二进制）、PLY（ASCII + 二进制小端）、OFF / NOFF / COFF / CNOFF、PMX 2.0 / 2.1（仅几何数据）
- **Built-in geometry generators**: Cube, sphere, cylinder, torus, cone — generated at runtime, no external assets required
- **内置几何体生成器**：立方体、球体、圆柱、圆环、圆锥 — 运行时生成，无需外部素材
- **Orbit camera**: Left-drag to rotate, middle/right-drag to pan, scroll to zoom, `F` to reset, `Shift+F` to frame selection
- **轨道相机**：左键拖拽旋转、中键/右键拖拽平移、滚轮缩放、`F` 复位、`Shift+F` 框选
- **Phong + Blinn-Phong lighting**: Key light + back fill + distance fog (toggleable) with configurable direction, intensity, and colour
- **Phong + Blinn-Phong 光照**：主光 + 背面补光 + 距离雾（可开关），方向/强度/颜色均可调
- **3 render modes**: Solid, wireframe, solid+wireframe overlay
- **3 种渲染模式**：实体、线框、实体+线框叠加
- **Face picking**: Click a model to highlight individual triangles
- **面片拾取**：点击模型可高亮单个三角面
- **Drag & drop**: Drop a model file onto the window to open it
- **拖放打开**：将模型文件拖到窗口即可打开
- **Screenshot export**: FBO off-screen render → PNG (stb_image_write)
- **截图导出**：FBO 离屏渲染 → PNG（stb_image_write）
- **Dual-viewport compare mode**:
  - *Compare: different models* — left and right viewports each load a different model
  - *Compare: different lighting* — same model, different light/material parameters
- **双视口对比模式**：
  - *对比不同模型* — 左右视口各加载不同模型
  - *对比不同光照* — 同一模型，不同光照/材质参数
- **ImGui control panel**: Menu bar, side panel, status bar, CJK font support
- **ImGui 控制面板**：菜单栏、侧栏、状态栏，支持中文字体

---

## Screenshots / 截图

*(Add screenshots here — press `Ctrl+S` in the app to save a PNG, or use the menu `File → Save Screenshot`.)*

*（在此添加截图 — 在应用中按 `Ctrl+S` 保存 PNG，或使用菜单 `File → Save Screenshot`。）*

---

## Quick Start (Pre-built Release) / 快速开始（预编译版本）

1. Go to the [Releases](../../releases) page.
2. Download `TriangleMeshBrowser-v0.1.0.zip` for the latest build.
3. Extract the zip and run `PrismViewer.exe`.

--
1. 前往 [Releases](../../releases) 页面。
2. 下载最新版本的 `TriangleMeshBrowser-v0.1.0.zip`。
3. 解压并运行 `PrismViewer.exe`。

No installer, no runtime — just run the `.exe`.

无需安装，无需运行时 — 直接运行 `.exe` 即可。

---

## Building from Source / 从源码编译

### Prerequisites / 前置依赖

Install these tools **once** on your build machine:

在编译机上**一次性**安装以下工具：

| Tool / 工具 | Version / 版本 | Where to get it / 获取方式 |
|-------------|---------------|--------------------------|
| **Visual Studio** | 2019 / 2022 / 18 | https://visualstudio.microsoft.com/ — select "Desktop development with C++" workload / 选择"使用 C++ 的桌面开发"工作负载 |
| **CMake** | ≥ 3.20 | https://cmake.org/download/ |
| **vcpkg** | latest / 最新 | https://github.com/microsoft/vcpkg — `git clone https://github.com/microsoft/vcpkg.git && cd vcpkg && .\bootstrap-vcpkg.bat` |

> The end user's machine needs **none** of these — every dependency is statically linked.
>
> 最终用户的电脑**无需**安装任何上述工具 — 所有依赖均已静态链接。

### All Dependencies (installed automatically by vcpkg) / 所有依赖（由 vcpkg 自动安装）

The following libraries are declared in `vcpkg.json` and fetched + built by vcpkg during the first CMake configure:

以下库在 `vcpkg.json` 中声明，由 vcpkg 在首次 CMake 配置时自动下载和编译：

| Library / 库 | Version / 版本 | Purpose / 用途 | License / 许可证 |
|--------------|---------------|---------------|------------------|
| **GLFW** | 3.4 | Window creation + OpenGL context + input / 窗口创建 + OpenGL 上下文 + 输入 | [zlib](https://github.com/glfw/glfw/blob/master/LICENSE.md) |
| **GLEW** | 2.3.1 | OpenGL extension wrangling / OpenGL 扩展加载 | [Modified BSD / MIT](https://github.com/nigels-com/glew/blob/master/LICENSE.txt) |
| **GLM** | 1.0.3 | Header-only maths library (vectors, matrices) / 纯头文件数学库（向量、矩阵） | [MIT (Happy Bunny)](https://github.com/g-truc/glm/blob/master/copying.txt) |
| **Dear ImGui** | 1.92.8 | Immediate-mode GUI (docking branch) / 即时模式 GUI（docking 分支） | [MIT](https://github.com/ocornut/imgui/blob/master/LICENSE.txt) |
| **stb** | latest / 最新 | Public-domain single-file libraries (`stb_image_write`) / 公共领域单文件库 | [Public Domain / MIT](https://github.com/nothings/stb/blob/master/LICENSE) |

You do **not** need to manually download or install any of these — vcpkg handles everything.

你**无需**手动下载或安装上述任何库 — vcpkg 会处理一切。

### One-command Build / 一键编译

```powershell
# In the project root / 在项目根目录下:
.\build.ps1
```

The first build takes **5–8 minutes** (vcpkg downloads + compiles the 5 libraries, then links the final binary).
Subsequent incremental builds finish in seconds.

首次编译约需 **5–8 分钟**（vcpkg 下载 + 编译 5 个库 + 链接最终二进制）。后续增量编译只需数秒。

### Build Options / 编译选项

```powershell
.\build.ps1 -DebugBuild    # Debug configuration / Debug 配置
.\build.ps1 -Clean         # Wipe build/ and rebuild from scratch / 清理 build/ 并重新编译
.\build.ps1 -Run           # Build, then launch immediately / 编译后立即启动
.\build.ps1 -Reconfigure   # Force CMake re-configure / 强制重新运行 CMake 配置
```

### Manual CMake Build / 手动 CMake 编译

If you prefer to run CMake directly / 如果你更习惯直接运行 CMake：

```powershell
# Set up vcpkg triplet for static linking / 设置 vcpkg triplet 进行静态链接
$triplet = "x64-windows-static"
$vcpkgRoot = "<path-to-vcpkg>"
$toolchain = "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake"

cmake -S . -B build `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
  -DVCPKG_TARGET_TRIPLET="$triplet"

cmake --build build --config Release --parallel
```

The output binary will be at `build\Release\PrismViewer.exe`.

编译产物位于 `build\Release\PrismViewer.exe`。

---

## Usage / 使用方法

### Launch / 启动

```powershell
# Empty scene / 空场景
.\build\Release\PrismViewer.exe

# Open a model directly / 直接打开模型
.\build\Release\PrismViewer.exe assets\models\icosahedron.obj
.\build\Release\PrismViewer.exe "C:\path\to\my_model.stl"
```

### Supported Formats / 支持的格式

| Extension / 扩展名 | Format / 格式 | Notes / 备注 |
|-------------------|--------------|-------------|
| `.obj` | Wavefront OBJ | ASCII |
| `.stl` | STereoLithography | ASCII + Binary / 二进制 |
| `.ply` | Polygon File Format | ASCII + Binary Little Endian / 二进制小端 |
| `.off` `.noff` `.coff` `.cnoff` | Object File Format | All colour variants / 全部颜色变体 |
| `.pmx` | MikuMikuDance PMX | 2.0 / 2.1, geometry only (no materials/bones) / 仅几何（不含材质/骨骼） |

Extensions are case-insensitive. / 扩展名不区分大小写。

### Controls / 操作

| Input / 输入 | Action / 效果 |
|-------------|--------------|
| **Left-drag / 左键拖拽** | Rotate camera / 旋转相机 |
| **Right-drag / Middle-drag / 右键/中键拖拽** | Pan camera / 平移相机 |
| **Scroll wheel / 滚轮** | Zoom / 缩放 |
| **F** | Reset camera to default view / 复位相机 |
| **Shift+F** | Frame camera on current model / 框选当前模型 |
| **Click model / 点击模型** | Pick & highlight a single triangle face / 拾取并高亮单个三角面 |
| **Drag file onto window / 拖文件到窗口** | Open the dropped model / 打开拖入的模型 |
| **Ctrl+O** | File-open dialog / 打开文件对话框 |
| **Esc** | Exit / 退出 |

---

## Project Structure / 项目结构

```
TriangleMeshBrowser/
├── CMakeLists.txt              # CMake build definition / CMake 构建定义
├── vcpkg.json                  # Dependency manifest (vcpkg) / 依赖清单
├── build.ps1                   # One-click build script / 一键编译脚本
├── setup.ps1                   # Environment sanity check / 环境检查脚本
├── README.md
├── LICENSE                     # MIT License / MIT 许可证
├── .gitignore
├── src/
│   ├── main.cpp                # Entry point / 入口
│   ├── app/
│   │   ├── Viewer.{h,cpp}      # Application shell / 应用程序主类
│   │   └── OrbitCamera.{h,cpp} # Orbital camera controller / 轨道相机控制器
│   ├── renderer/
│   │   ├── GLResources.h       # RAII OpenGL object wrappers / RAII OpenGL 对象包装
│   │   ├── Shader.{h,cpp}      # GLSL shader compiler / GLSL 着色器编译器
│   │   ├── MeshRenderer.{h,cpp}# Mesh rendering pipeline / 网格渲染管线
│   │   ├── FrameBuffer.{h,cpp} # Off-screen FBO (screenshots) / 离屏帧缓冲（截图）
│   │   ├── Grid.{h,cpp}        # Reference grid / 参考网格
│   │   └── shaders/            # GLSL 3.30 shader sources / GLSL 3.30 着色器源码
│   ├── model/
│   │   ├── Mesh.{h,cpp}        # Unified mesh representation / 统一网格表示
│   │   ├── ModelLoader.{h,cpp} # Format-dispatch loader / 格式分发加载器
│   │   ├── OBJLoader.{h,cpp}   # Wavefront OBJ parser / OBJ 解析器
│   │   ├── STLLoader.{h,cpp}   # STL parser (ASCII + binary) / STL 解析器
│   │   ├── PLYLoader.{h,cpp}   # PLY parser / PLY 解析器
│   │   ├── OFFLoader.{h,cpp}   # OFF/NOFF/COFF/CNOFF parser / OFF 解析器
│   │   └── Procedural.{h,cpp}  # Runtime geometry generators / 运行时几何体生成器
│   ├── ui/
│   │   ├── Panel.{h,cpp}       # ImGui control panel / ImGui 控制面板
│   │   └── Picker.{h,cpp}      # Ray-triangle intersection / 射线-三角形求交
│   └── utils/
│       ├── FileUtils.{h,cpp}   # File-system helpers / 文件系统辅助
│       ├── StbWrite.{h,cpp}    # PNG writer (stb_image_write) / PNG 写入
│       └── ImeGuard.{h,cpp}    # IME context management / 输入法上下文管理
└── assets/
    └── models/                 # Hand-authored sample models / 手写示例模型
        ├── cube.obj            # 立方体
        ├── tetrahedron.obj     # 正四面体
        ├── octahedron.obj      # 正八面体
        └── icosahedron.obj     # 正二十面体
```

---

## Design Principles / 设计原则

- **All-RAII OpenGL resources**: `ShaderPtr`, `VaoPtr`, `BufferPtr` use `unique_ptr` with custom deleters; destructors call `glDelete*` automatically. **Zero naked-pointer leaks.**
- **OpenGL 资源全 RAII**：`ShaderPtr`、`VaoPtr`、`BufferPtr` 使用 `unique_ptr` + 自定义删除器，析构时自动调用 `glDelete*`。**零裸指针泄漏。**
- **Modern C++17**: `std::filesystem`, `if constexpr`, `std::optional`, `std::unique_ptr`, structured bindings.
- **现代 C++17**：`std::filesystem`、`if constexpr`、`std::optional`、`std::unique_ptr`、结构化绑定。
- **No unsafe memory**: No `malloc`; `reinterpret_cast` is used only for STL/PLY binary byte-stream parsing, confined to the I/O layer.
- **无不安全内存操作**：不使用 `malloc`；`reinterpret_cast` 仅在 STL/PLY 二进制字节流读取时使用，集中在 IO 层。
- **Fully static linking**: vcpkg `x64-windows-static` triplet; the binary depends only on Windows system DLLs.
- **完全静态链接**：vcpkg `x64-windows-static` triplet；二进制仅依赖 Windows 系统 DLL。
- **Original code**: Every source file is original work; only open-source libraries are linked (GLFW, ImGui, GLM, GLEW, stb).
- **原创代码**：所有源文件均为原创；仅链接开源库（GLFW、ImGui、GLM、GLEW、stb）。
- **No external assets**: Sample models are hand-written `.obj` files; procedural geometry is generated at runtime.
- **无外部素材**：示例模型为手写 `.obj` 文件；几何体在运行时程序化生成。

---

## Open-Source License Attribution / 开源许可证说明

### This Project / 本项目

All source code in `src/`, the shaders in `src/renderer/shaders/`, the sample models in `assets/models/`, and the build scripts (`build.ps1`, `setup.ps1`) are licensed under the **MIT License** — see [LICENSE](LICENSE) for the full text.

`src/` 中的所有源代码、`src/renderer/shaders/` 中的着色器、`assets/models/` 中的示例模型以及构建脚本（`build.ps1`、`setup.ps1`）均采用 **MIT 许可证** — 详见 [LICENSE](LICENSE)。

### Third-Party Libraries / 第三方库

This project links against the following open-source libraries. **None of their source code is included in this repository** — they are fetched at build time by vcpkg. See their respective repositories for full license texts.

本项目链接以下开源库。**它们的源代码不包含在本仓库中** — 由 vcpkg 在构建时获取。完整的许可证文本请参见各自的仓库。

| Library / 库 | Version / 版本 | License / 许可证 | Upstream / 上游 |
|--------------|---------------|-----------------|-----------------|
| **GLFW** | 3.4 | [zlib License](https://opensource.org/licenses/Zlib) | https://github.com/glfw/glfw |
| **GLEW** | 2.3.1 | [Modified BSD](https://opensource.org/licenses/BSD-3-Clause) / MIT | https://github.com/nigels-com/glew |
| **GLM** | 1.0.3 | [MIT (Happy Bunny)](https://opensource.org/licenses/MIT) | https://github.com/g-truc/glm |
| **Dear ImGui** | 1.92.8 | [MIT](https://opensource.org/licenses/MIT) | https://github.com/ocornut/imgui |
| **stb** | latest / 最新 | [Public Domain](https://unlicense.org/) / [MIT](https://opensource.org/licenses/MIT) | https://github.com/nothings/stb |

#### Quick License Summaries / 许可证简要说明

- **zlib**: Freely use, modify, distribute. Keep the copyright notice; do not misrepresent the origin. / 自由使用、修改、分发。保留版权声明，不得歪曲来源。
- **Modified BSD**: Freely use, modify, distribute. Keep the copyright notice and disclaimer. / 自由使用、修改、分发。保留版权声明和免责声明。
- **MIT**: Freely use, modify, distribute. Keep the copyright notice. / 自由使用、修改、分发。保留版权声明。
- **Public Domain (stb)**: No restrictions whatsoever; attribution is appreciated but not required. / 无任何限制；署名感谢但不强制。

All of these licenses are permissive and compatible with both open-source and proprietary use.

以上所有许可证均为宽松许可证，兼容开源和商业使用。

---

## Known Limitations / 已知限制

- PLY only supports `binary_little_endian`; `binary_big_endian` is uncommon in practice and not implemented.
- PLY 仅支持 `binary_little_endian`；`binary_big_endian` 在实际中很少见，未实现。
- Large models (>1M triangles) may have a 1–2 second freeze on first load (main-thread parsing); background-thread loading is a planned improvement.
- 大模型（>100 万三角形）首次加载可能有 1–2 秒卡顿（主线程解析）；后台线程加载是计划中的改进。
- No visual feedback (e.g., highlight border) during drag-and-drop; only GLFW's native drop event is used.
- 拖放时无视觉反馈（如高亮边框）；仅使用 GLFW 原生 drop 事件。
- Face picking precision: very small triangles (<5 pixels on screen) may require multiple clicks.
- 面片拾取精度：极小三角面（屏幕上 <5 像素）可能需要多次点击。

---

## Contributing / 参与贡献

Issues and pull requests are welcome. Before opening a PR, please:

欢迎提交 Issue 和 Pull Request。在提交 PR 之前，请：

1. Run `.\setup.ps1` to verify your toolchain. / 运行 `.\setup.ps1` 验证工具链。
2. Build and test with `.\build.ps1 -Clean -Run`. / 使用 `.\build.ps1 -Clean -Run` 编译和测试。
3. Ensure no new compiler warnings are introduced (`/W4` is enforced). / 确保不引入新的编译警告（强制 `/W4`）。

---

## Acknowledgements / 致谢

Design inspired by various 3D mesh viewers in the open-source community.

设计灵感来自开源社区中的多个 3D 网格查看器。
