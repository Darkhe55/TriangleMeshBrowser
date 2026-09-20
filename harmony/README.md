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
browser/harmony/build-output/entry-default-signed.hap     # 已签名, 可上架
browser/harmony/build-output/entry-default-unsigned.hap   # 未签名中间产物
```

> 构建脚本会在 hvigor 出包后自动调用 SDK 自带的 `hap-sign-tool.jar` 完成签名
> （材料配置见第 4 节）。只想出未签名包时加 `--no-sign`。

---

## 4. 签名与上架（华为应用市场）

### 4.1 为什么必须用你自己的证书

- SDK 自带的 `OpenHarmony.p12` 等材料属于 **OpenHarmony** 证书链：
  - 其 debug profile 含 **UDID 设备白名单**，且证书 **2024-01 已过期**
  - **无法安装到华为 HarmonyOS 设备**（含鸿蒙 PC），也**无法上架**
- 上架华为应用市场必须使用 **AppGallery Connect（AGC）签发的发布证书**。

### 4.2 准备签名材料（一次性）

**不需要安装 DevEco Studio** —— 用 SDK 自带 JDK 的 `keytool` + AGC 网页即可完成。

**第 1 步：本地生成密钥库与证书请求**

`.p12` 是**在本地生成**的（不是从 AGC 下载）：

```bash
keytool -genkeypair -alias prismviewer_key \
  -keyalg EC -groupname secp256r1 \
  -keystore prismviewer.p12 -storetype PKCS12 \
  -storepass "你的密码" -keypass "你的密码" -validity 3650 \
  -dname "CN=PrismViewer, OU=Dev, O=Darkhe55, L=City, ST=State, C=CN"

keytool -certreq -alias prismviewer_key \
  -keystore prismviewer.p12 -storepass "你的密码" \
  -file prismviewer.csr
```

> - 密码 **≥6 位**；PKCS12 下 `storepass` 与 `keypass` **必须相同**
> - 务必记牢三样：`keyAlias`、`storepass`、`keypass` —— `.p12` 丢失不可恢复
> - 想换密码且**不重新申请证书**：`keytool -storepasswd -keystore prismviewer.p12 -storepass 旧 -new 新`

**第 2 步：在 AGC 注册应用**

- 应用类型选「**应用**」（非游戏）；填**应用包名** —— 创建后**不可修改**，
  且必须与 `AppScope/app.json5` 的 `bundleName` 一致
- **开放能力：本应用一个都不用勾**。它只使用基础能力（ArkUI / XComponent /
  NDK 图形 / 文件读取），不涉及账号、推送、定位、支付等需单独开通的服务，
  直接跳过（该配置会写入 Profile，若之后改动需**重新下载 `.p7b`**）

**第 3 步：AGC →「证书、APP ID 和 Profile」→ 证书 → 新增证书**

- 类型选 **发布证书**，上传第 1 步生成的 `.csr` → 下载 **`.cer`**

**第 4 步：同页面 → Profile → 添加**

| 字段 | 填写 |
|------|------|
| 应用名称 | 选择第 2 步创建的应用 |
| Profile 名称 | 自定义，如 `PrismViewer-Release`（≤100 字符） |
| 类型 | **发布**（上架用；真机调试需另建调试证书 + 调试 Profile） |
| 选择证书 | 选第 3 步的发布证书（**类型与证书必须匹配**） |
| 选择设备 | 发布类型**无此项**（设备列表为空 = 全设备） |
| 申请权限 | **留空**（本工程未声明任何受限/ACL 权限） |

→ 下载 **`.p7b`**

**第 5 步：备齐 4 项**：三个文件路径 + `keyAlias` + 密码

### 4.3 配置签名

签名材料**不写进** `build-profile.json5`。原因：hvigor 只接受 DevEco Studio 用本机
密钥加密后的密文密码（长度必然 ≥32），**明文密码一律被拒绝**并报：

```
00303116 Configuration Error: The length of the storePassword or keyPassword
field in the signature configuration is less than 32.
```

因此本项目把签名移到构建脚本里：`tools/build_ascii.py` 在 hvigor 出包后调用
SDK 自带的 `hap-sign-tool.jar` 完成签名（同样产出 `entry-default-signed.hap`，
无需 DevEco Studio）。

把材料填进 `harmony/signing.local.json`（复制 `signing.local.json.example` 得到；
该文件已被 `.gitignore` 忽略，**密码不会入库**）：

```json
{
  "keystoreFile": "C:/keys/prismviewer.p12",
  "keystorePwd": "你的密码",
  "keyAlias": "prismviewer_key",
  "keyPwd": "你的密码",
  "appCertFile": "C:/keys/你的.cer",
  "profileFile": "C:/keys/你的.p7b",
  "signAlg": "SHA256withECDSA",
  "compatibleVersion": "12"
}
```

> `compatibleVersion` 取工程 `compatibleSdkVersion` 的 API 号（当前 `5.0.0(12)` → `12`）。

### 4.4 出包与校验

```bash
python tools/build_ascii.py             # 构建 + 自动签名
python tools/build_ascii.py --no-sign   # 只构建, 不签名
```

产物中的 `entry-default-signed.hap` 即可上架。手动校验签名：

```bash
java -jar "$DEVECO_HOME/sdk/default/openharmony/toolchains/lib/hap-sign-tool.jar" \
  verify-app -inFile build-output/entry-default-signed.hap \
  -outCertChain out.cer -outProfile out.p7b
# 期望输出: verify-app success
```

### 4.5 上传上架

AGC → 你的应用 → **版本管理 → 上传软件包** → 选择 `entry-default-signed.hap`
→ 填写版本说明 → **提交审核**。

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
- [x] **ImGui 控制面板**（源码编译 + GLES3 后端 + 中文字体）
- [x] **完整格式支持**：交叉编译并链接 **Assimp**（FBX / glTF / GLB / DAE / 3MF）、
      **laszip**（LAS / LAZ）、**pugixml**（E57）—— 与桌面版一致的全部 13 种格式
- [x] 构建产出 `.hap`（**约 13.6 MB** 签名后，含内核 + ImGui + 全部格式）
- [x] **发布签名出包**：`signing.local.json` 配置 + 构建后自动调用 `hap-sign-tool.jar`
      签名，产出可上架的 `entry-default-signed.hap`（`verify-app` 校验通过，见第 4 节）

### 待完成

- [ ] 文件选择器（替代 Win32 `GetOpenFileName`）
- [ ] IME 适配（替代 Win32 IMM）
- [ ] 触摸手势（双指缩放/旋转）与 `OrbitCamera` 完整对接
- [ ] 截图导出到应用沙箱
- [ ] 上传 AGC 提交审核（材料已备齐，见第 4.5 节）

---

## 6. 故障排查

| 现象 | 原因 / 解决 |
|------|------------|
| `Invalid project path` | 工程路径含中文 → 用 `tools/build_ascii.py` |
| `hvigorw not found` | 设置 `DEVECO_HOME` 指向 command-line-tools |
| `No signingConfig found` | **正常警告**（签名已移到脚本里做），只要末尾出现「已签名」即可 |
| `00303116 ... less than 32` | 试图在 `build-profile.json5` 填明文密码 → hvigor 只认密文，改填 `signing.local.json` |
| `verify-profile`/`verify-app` 报 `Param is not trusted` | 参数名错误（用 `-outFile`，不是 `-outProfile`） |
| `keystore password was incorrect` | `signing.local.json` 里的密码与 `.p12` 不符 |
| `EGLNativeWindowType` 类型错误 | OHOS 上该类型为整型，需 `reinterpret_cast` |
| 安装失败 | 证书与设备/应用市场不匹配 → 用 AGC 签发证书 |
| `Bundle name does not match AGC configuration` | `AppScope/app.json5` 的 `bundleName` 与 AGC Profile 不一致 |
