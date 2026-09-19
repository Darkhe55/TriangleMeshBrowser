# PrismViewer for HarmonyOS PC / 三角网格浏览器（鸿蒙 PC 版）

本目录是 PrismViewer 的 **HarmonyOS PC 原生移植工程**（ArkTS 壳 + Native C++ 渲染内核）。

---

## 1. 架构

```
ArkTS (UIAbility + XComponent)
        │  libraryname: 'entry'
        ▼
libentry.so  ── napi_init.cpp        NAPI 模块注册 / XComponent 生命周期回调
             ├─ platform/EglCore    EGL 上下文 + GLES 3.0 表面 (来自 XComponent NativeWindow)
             ├─ platform/InputState 线程安全输入队列 (触摸/鼠标/按键/滚轮)
             └─ platform/PlatformViewer  渲染宿主 (每帧渲染)
                    └─ 复用 browser/src 的模型/渲染/UI 内核
```

- 渲染在**独立线程**，`eglMakeCurrent` → 渲染 → `eglSwapBuffers`。
- `napi_init.cpp` 注册 4 类回调：surface 创建/变化/销毁、触摸、鼠标、按键。

---

## 2. 目录结构

```
harmony/
├── AppScope/                     应用级配置 (bundleName / 版本 / 图标)
│   ├── app.json5
│   └── resources/base/{element,media}
├── entry/                        主模块 (HAP)
│   ├── build-profile.json5       含 externalNativeOptions → cpp/CMakeLists.txt
│   ├── hvigorfile.ts
│   ├── oh-package.json5
│   └── src/main/
│       ├── module.json5          deviceTypes: 2in1 / tablet / phone
│       ├── ets/
│       │   ├── entryability/EntryAbility.ets
│       │   └── pages/Index.ets         XComponent(type: SURFACE)
│       ├── cpp/
│       │   ├── CMakeLists.txt          PRISM_ENABLE_CORE 开关
│       │   ├── napi_init.cpp
│       │   └── platform/{EglCore,InputState,PlatformViewer}.{h,cpp}
│       └── resources/
├── build-profile.json5           产品/签名配置
├── hvigor/hvigor-config.json5    hvigor 版本与执行参数
├── oh-package.json5
├── local.properties              SDK 路径 (本地, 不入库)
└── tools/
    ├── build_ascii.py            ASCII 工作区构建脚本 (见第 3 节)
    └── gen_icons.py              生成应用图标
```

---

## 3. 构建

### 3.1 前置

- HarmonyOS SDK（DevEco Studio 或 command-line-tools）
- Python 3（运行构建脚本）

### 3.2 ⚠️ 路径必须为纯 ASCII

**hvigor 会拒绝含非 ASCII 字符的工程路径**（如中文目录 `棱镜模型`），报
`Invalid project path`。因此**不能**直接在中文路径下运行 hvigor。

`tools/build_ascii.py` 会自动处理：

```bash
cd browser/harmony
python tools/build_ascii.py
```

脚本动作：
1. 把 `browser/harmony` 与 `browser/src` 同步到纯 ASCII 工作区
   （默认 `~/ohos-build/prism`，可用环境变量 `PRISM_OHOS_BUILD_WS` 覆盖）
2. 在 ASCII 工作区执行 `hvigorw assembleHap`
3. 把 `.hap` 拷回 `browser/harmony/build-output/`

工具链路径默认 `~/Documents/ohos-tools/command-line-tools`，
可用环境变量 `DEVECO_HOME` 覆盖。

### 3.3 构建产物

```
browser/harmony/build-output/entry-default-unsigned.hap
```

> 目前为**未签名**包 —— 因为签名需要你的发布证书，见第 4 节。

---

## 4. 签名与上架（华为应用市场）

### 4.1 为什么必须用你自己的证书

- SDK 自带的 `OpenHarmony.p12` 等材料属于 **OpenHarmony** 证书链：
  - 其 debug profile 含 **UDID 设备白名单**，且证书 **2024-01 已过期**
  - **无法安装到华为 HarmonyOS 设备**（含鸿蒙 PC），也**无法上架**
- 上架华为应用市场必须使用 **AppGallery Connect（AGC）签发的发布证书**。

### 4.2 申请证书（在 AGC 完成，一次性）

1. 注册华为开发者账号并完成**实名认证**
2. 登录 [AppGallery Connect](https://developer.huawei.com/consumer/cn/service/josp/agc/index.html)
3. **用户与访问 → 密钥管理** 创建密钥，得到 `.p12`（私钥库）
4. **证书管理** 申请证书，得到 `.cer`（证书）
5. **Profile 管理** 创建发布 Profile（需先创建应用），得到 `.p7b`
6. 记录：`bundleName`、keyAlias、两个密码

> `AppScope/app.json5` 的 `bundleName` 必须与 AGC 中创建的应用一致
> （当前为 `com.prism.trianglemesh`，请改为你注册的包名）。

### 4.3 配置签名

编辑 `harmony/build-profile.json5`，填入 `signingConfigs`：

```json5
{
  "app": {
    "signingConfigs": [
      {
        "name": "default",
        "type": "HarmonyOS",
        "material": {
          "storeFile": "C:/keys/你的.p12",
          "storePassword": "<加密后的 store 密码>",
          "keyAlias": "<keyAlias>",
          "keyPassword": "<加密后的 key 密码>",
          "signAlg": "SHA256withECDSA",
          "certpath": "C:/keys/你的.cer",
          "profile": "C:/keys/你的.p7b"
        }
      }
    ],
    "products": [
      {
        "name": "default",
        "signingConfig": "default",
        "compatibleSdkVersion": "5.0.0(12)",
        "targetSdkVersion": "5.0.0(12)",
        "runtimeOS": "HarmonyOS"
      }
    ]
  }
}
```

> `storePassword` / `keyPassword` 需为 **DevEco Studio 加密后的密文**
> （在 DevEco 中配置签名时会自动生成；命令行明文密码通常不被接受）。

### 4.4 出包

```bash
python tools/build_ascii.py
```

签名成功后产物为 `entry-default-signed.hap`，即可上传到 AGC。

---

## 5. 当前进度 / TODO

### 已完成

- [x] 工程骨架 (AppScope / entry / hvigor / build-profile)
- [x] ArkTS `UIAbility` + `XComponent(SURFACE)` 接入
- [x] Native 层: NAPI 注册、XComponent 生命周期、**EGL + GLES 3.0** 上下文、独立渲染线程
- [x] 输入适配: 触摸 / 鼠标 / 按键 / 滚轮（线程安全队列）
- [x] **模型/渲染内核接入**: `Mesh` / `Procedural` / `MeshRenderer` / `Shader`
- [x] **GLSL 3.30 → GLSL ES 3.00 运行时转换**（着色器嵌入 `.so`，无需资源文件）
- [x] GL 头适配（`GLResources.h` 按平台切 GLEW / GLES3）
- [x] 构建产出 `.hap`（约 4.0 MB，含内核）

### 待完成

- [ ] **ImGui 控制面板**：需先把 imgui 源码交叉编译到 OHOS（`imgui_impl_opengl3` 走 GLES3）
- [ ] **完整格式支持**：交叉编译 Assimp / laszip / pugixml 后，去掉
      `ModelLoader.cpp` 中的 `#ifndef PRISM_OHOS` 守卫即可启用
      FBX / glTF / DAE / 3MF / LAS / LAZ / E57
- [ ] 文件选择器（替代 Win32 `GetOpenFileName`）
- [ ] IME 适配（替代 Win32 IMM）
- [ ] 触摸手势（双指缩放/旋转）与 `OrbitCamera` 完整对接
- [ ] 截图导出到应用沙箱
- [ ] 发布签名（见第 4 节）

---

## 6. 故障排查

| 现象 | 原因 / 解决 |
|------|------------|
| `Invalid project path` | 工程路径含中文 → 用 `tools/build_ascii.py` |
| `hvigorw not found` | 设置 `DEVECO_HOME` 指向 command-line-tools |
| `No signingConfig found` | 未配置签名 → 见第 4 节 |
| `EGLNativeWindowType` 类型错误 | OHOS 上该类型为整型，需 `reinterpret_cast` |
| 安装失败 | 证书与设备/应用市场不匹配 → 用 AGC 签发证书 |
