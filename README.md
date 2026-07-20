# TriangleMeshBrowser (PrismViewer)

A lightweight, standalone 3D triangle-mesh browser built with C++17 + OpenGL 3.3 + GLFW + Dear ImGui.

> **Single-file executable** — no runtime dependencies. All third-party libraries
> (OpenGL loader, GLFW, ImGui, GLM, stb) are statically linked into the binary;
> the only requirements are the standard Windows system DLLs
> (`opengl32`, `gdi32`, `user32`, `shell32`).

---

## Features

- **Multi-format loading**: OBJ, STL (ASCII + Binary), PLY (ASCII + Binary Little Endian), OFF / NOFF / COFF / CNOFF
- **Built-in geometry generators**: Cube, sphere, cylinder, torus, cone — generated at runtime, no external assets required
- **Orbit camera**: Left-drag to rotate, middle/right-drag to pan, scroll to zoom, `F` to reset, `Shift+F` to frame selection
- **Phong + Blinn-Phong lighting**: Key light + back fill + distance fog (toggleable) with configurable direction, intensity, and colour
- **3 render modes**: Solid, wireframe, solid+wireframe overlay
- **Face picking**: Click a model to highlight individual triangles
- **Drag & drop**: Drop a model file onto the window to open it
- **Screenshot export**: FBO off-screen render → PNG (stb_image_write)
- **Dual-viewport compare mode**:
  - *Compare: different models* — left and right viewports each load a different model
  - *Compare: different lighting* — same model, different light/material parameters
- **ImGui control panel**: Menu bar, side panel, status bar, CJK font support

---

## Screenshots

*(Add screenshots here — press `Ctrl+S` in the app to save a PNG, or use the menu `File → Save Screenshot`.)*

---

## Quick Start (Pre-built Release)

1. Go to the [Releases](../../releases) page.
2. Download `TriangleMeshBrowser-v0.1.0.zip` for the latest build.
3. Extract the zip and run `PrismViewer.exe`.

No installer, no runtime — just run the `.exe`.

---

## Building from Source

### Prerequisites

Install these tools **once** on your build machine:

| Tool | Version | Where to get it |
|------|---------|-----------------|
| **Visual Studio** | 2019 / 2022 / 18 | https://visualstudio.microsoft.com/ — select "Desktop development with C++" workload |
| **CMake** | ≥ 3.20 | https://cmake.org/download/ |
| **vcpkg** | latest | https://github.com/microsoft/vcpkg — `git clone https://github.com/microsoft/vcpkg.git && cd vcpkg && .\bootstrap-vcpkg.bat` |

> The end user's machine needs **none** of these — every dependency is statically linked.

### All Dependencies (installed automatically by vcpkg)

The following libraries are declared in `vcpkg.json` and fetched + built by vcpkg during the first CMake configure:

| Library | Version | Purpose | License |
|---------|---------|---------|---------|
| **GLFW** | 3.4 | Window creation + OpenGL context + input | [zlib](https://github.com/glfw/glfw/blob/master/LICENSE.md) |
| **GLEW** | 2.3.1 | OpenGL extension wrangling | [Modified BSD / MIT](https://github.com/nigels-com/glew/blob/master/LICENSE.txt) |
| **GLM** | 1.0.3 | Header-only maths library (vectors, matrices) | [MIT (Happy Bunny)](https://github.com/g-truc/glm/blob/master/copying.txt) |
| **Dear ImGui** | 1.92.8 | Immediate-mode GUI (docking branch) | [MIT](https://github.com/ocornut/imgui/blob/master/LICENSE.txt) |
| **stb** | latest | Public-domain single-file libraries (`stb_image_write`) | [Public Domain / MIT](https://github.com/nothings/stb/blob/master/LICENSE) |

You do **not** need to manually download or install any of these — vcpkg handles everything.

### One-command Build

```powershell
# In the project root:
.\build.ps1
```

The first build takes **5–8 minutes** (vcpkg downloads + compiles the 5 libraries, then links the final binary).
Subsequent incremental builds finish in seconds.

### Build Options

```powershell
.\build.ps1 -DebugBuild    # Debug configuration
.\build.ps1 -Clean         # Wipe build/ and rebuild from scratch
.\build.ps1 -Run           # Build, then launch immediately
.\build.ps1 -Reconfigure   # Force CMake re-configure
```

### Manual CMake Build

If you prefer to run CMake directly:

```powershell
# Set up vcpkg triplet for static linking
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

---

## Usage

### Launch

```powershell
# Empty scene
.\build\Release\PrismViewer.exe

# Open a model directly
.\build\Release\PrismViewer.exe assets\models\icosahedron.obj
.\build\Release\PrismViewer.exe "C:\path\to\my_model.stl"
```

### Supported Formats

| Extension | Format | Notes |
|-----------|--------|-------|
| `.obj` | Wavefront OBJ | ASCII |
| `.stl` | STereoLithography | ASCII + Binary |
| `.ply` | Polygon File Format | ASCII + Binary Little Endian |
| `.off` `.noff` `.coff` `.cnoff` | Object File Format | All colour variants |

Extensions are case-insensitive.

### Controls

| Input | Action |
|-------|--------|
| **Left-drag** | Rotate camera |
| **Right-drag / Middle-drag** | Pan camera |
| **Scroll wheel** | Zoom |
| **F** | Reset camera to default view |
| **Shift+F** | Frame camera on current model |
| **Click model** | Pick & highlight a single triangle face |
| **Drag file onto window** | Open the dropped model |
| **Ctrl+O** | File-open dialog |
| **Esc** | Exit |

---

## Project Structure

```
TriangleMeshBrowser/
├── CMakeLists.txt              # CMake build definition
├── vcpkg.json                  # Dependency manifest (vcpkg)
├── build.ps1                   # One-click build script
├── setup.ps1                   # Environment sanity check
├── README.md
├── .gitignore
├── src/
│   ├── main.cpp                # Entry point
│   ├── app/
│   │   ├── Viewer.{h,cpp}      # Application shell
│   │   └── OrbitCamera.{h,cpp} # Orbital camera controller
│   ├── renderer/
│   │   ├── GLResources.h       # RAII OpenGL object wrappers
│   │   ├── Shader.{h,cpp}      # GLSL shader compiler
│   │   ├── MeshRenderer.{h,cpp}# Mesh rendering pipeline
│   │   ├── FrameBuffer.{h,cpp} # Off-screen FBO (screenshots)
│   │   ├── Grid.{h,cpp}        # Reference grid
│   │   └── shaders/            # GLSL 3.30 shader sources
│   ├── model/
│   │   ├── Mesh.{h,cpp}        # Unified mesh representation
│   │   ├── ModelLoader.{h,cpp} # Format-dispatch loader
│   │   ├── OBJLoader.{h,cpp}   # Wavefront OBJ parser
│   │   ├── STLLoader.{h,cpp}   # STL parser (ASCII + binary)
│   │   ├── PLYLoader.{h,cpp}   # PLY parser
│   │   ├── OFFLoader.{h,cpp}   # OFF/NOFF/COFF/CNOFF parser
│   │   └── Procedural.{h,cpp}  # Runtime geometry generators
│   ├── ui/
│   │   ├── Panel.{h,cpp}       # ImGui control panel
│   │   └── Picker.{h,cpp}      # Ray-triangle intersection
│   └── utils/
│       ├── FileUtils.{h,cpp}   # File-system helpers
│       ├── StbWrite.{h,cpp}    # PNG writer (stb_image_write)
│       └── ImeGuard.{h,cpp}    # IME context management
└── assets/
    └── models/                 # Hand-authored sample models
        ├── cube.obj
        ├── tetrahedron.obj
        ├── octahedron.obj
        └── icosahedron.obj
```

---

## Design Principles

- **All-RAII OpenGL resources**: `ShaderPtr`, `VaoPtr`, `BufferPtr` use `unique_ptr` with custom deleters; destructors call `glDelete*` automatically. **Zero naked-pointer leaks.**
- **Modern C++17**: `std::filesystem`, `if constexpr`, `std::optional`, `std::unique_ptr`, structured bindings.
- **No unsafe memory**: No `malloc`; `reinterpret_cast` is used only for STL/PLY binary byte-stream parsing, confined to the I/O layer.
- **Fully static linking**: vcpkg `x64-windows-static` triplet; the binary depends only on Windows system DLLs.
- **Original code**: Every source file is original work; only open-source libraries are linked (GLFW, ImGui, GLM, GLEW, stb).
- **No external assets**: Sample models are hand-written `.obj` files; procedural geometry is generated at runtime.

---

## Open-Source License Attribution

### This Project

All source code in `src/`, the shaders in `src/renderer/shaders/`, the sample models in `assets/models/`, and the build scripts (`build.ps1`, `setup.ps1`) are licensed under the **MIT License** — see [LICENSE](LICENSE) for the full text.

### Third-Party Libraries

This project links against the following open-source libraries. **None of their source code is included in this repository** — they are fetched at build time by vcpkg. See their respective repositories for full license texts.

| Library | Version | License | Upstream |
|---------|---------|---------|----------|
| **GLFW** | 3.4 | [zlib License](https://opensource.org/licenses/Zlib) | https://github.com/glfw/glfw |
| **GLEW** | 2.3.1 | [Modified BSD](https://opensource.org/licenses/BSD-3-Clause) / MIT | https://github.com/nigels-com/glew |
| **GLM** | 1.0.3 | [MIT (Happy Bunny)](https://opensource.org/licenses/MIT) | https://github.com/g-truc/glm |
| **Dear ImGui** | 1.92.8 | [MIT](https://opensource.org/licenses/MIT) | https://github.com/ocornut/imgui |
| **stb** | latest | [Public Domain](https://unlicense.org/) / [MIT](https://opensource.org/licenses/MIT) | https://github.com/nothings/stb |

#### Quick License Summaries

- **zlib**: Freely use, modify, distribute. Keep the copyright notice; do not misrepresent the origin.
- **Modified BSD**: Freely use, modify, distribute. Keep the copyright notice and disclaimer.
- **MIT**: Freely use, modify, distribute. Keep the copyright notice.
- **Public Domain (stb)**: No restrictions whatsoever; attribution is appreciated but not required.

All of these licenses are permissive and compatible with both open-source and proprietary use.

---

## Known Limitations

- PLY only supports `binary_little_endian`; `binary_big_endian` is uncommon in practice and not implemented.
- Large models (>1M triangles) may have a 1–2 second freeze on first load (main-thread parsing); background-thread loading is a planned improvement.
- No visual feedback (e.g., highlight border) during drag-and-drop; only GLFW's native drop event is used.
- Face picking precision: very small triangles (<5 pixels on screen) may require multiple clicks.

---

## Contributing

Issues and pull requests are welcome. Before opening a PR, please:

1. Run `.\setup.ps1` to verify your toolchain.
2. Build and test with `.\build.ps1 -Clean -Run`.
3. Ensure no new compiler warnings are introduced (`/W4` is enforced).

---

## Acknowledgements

Design inspired by various 3D mesh viewers in the open-source community.
