# 棱镜模型查看器 隐私政策

**生效日期**：2026 年 9 月 20 日
**应用名称**：棱镜模型查看器（英文名：PrismViewer）
**应用包名**：`com.darkhe55.prismviewer`
**开发者**：Darkhe55
**联系方式**：`starray@mail.darkh.top`

---

## 一、总则

本隐私政策适用于「棱镜模型查看器」（以下简称"本应用"）。我们非常重视你的隐私，
本政策旨在清楚说明本应用如何处理与你相关的信息。

**核心结论：本应用不收集、不传输、不共享你的任何个人信息。**

---

## 二、我们收集哪些信息

**本应用不收集任何个人信息。**

具体而言，本应用：

- ❌ **不收集**设备标识符（IMEI、OAID、序列号、MAC 地址等）
- ❌ **不收集**位置信息
- ❌ **不收集**通讯录、短信、通话记录
- ❌ **不收集**相册、相机、麦克风内容
- ❌ **不收集**你的使用行为、点击记录或崩溃日志上报
- ❌ **不要求**注册账号，不采集手机号、邮箱等身份信息
- ❌ **不包含**任何广告、统计分析或用户画像功能

本应用是一款**完全离线的本地 3D 模型查看工具**，所有功能均在你的设备本地完成。

---

## 三、权限使用说明

本应用**未申请任何系统敏感权限**（HarmonyOS 的 `requestPermissions` 列表为空）。

你可能会用到以下系统交互，它们均由系统框架在你的**主动操作下**完成，
本应用不会在后台自行发起：

| 场景 | 说明 |
|------|------|
| **打开模型文件** | 当你点击"打开文件"时，由**系统的文件选择器**（DocumentViewPicker）展示文件列表。**只有你主动选中的那个文件**会被读取，选择过程完全由系统界面控制，本应用无法浏览你设备上的其他文件。 |
| **保存截图** | 当你导出截图时，由**系统的保存对话框**让你指定保存位置。本应用只把你指定的内容写入你指定的位置。 |

> 本应用**没有**申请网络权限，从技术上也**无法**将任何数据发送到外部服务器。

---

## 四、数据的存储与处理

1. **模型文件**：你选择打开的模型文件会被**临时复制**到本应用的私有沙箱缓存目录，
   仅用于解析与渲染。该副本位于应用私有空间，其他应用无法访问，并会随系统清理或
   卸载应用时被删除。
2. **截图**：截图在内存中生成后写入应用私有缓存目录，再经由系统保存对话框写入
   **你指定的位置**。若你在对话框中取消，截图仅保留在应用私有缓存中，不会外泄。
3. **不上传**：上述任何数据**都不会离开你的设备**。
4. **卸载即删**：卸载本应用会一并清除其私有沙箱内的全部临时数据。

---

## 五、第三方 SDK

本应用**未集成任何第三方 SDK**，包括但不限于广告、统计、推送、社交登录类 SDK。

本应用使用的开源组件（如 OpenGL ES、glm、ImGui、Assimp、laszip、pugixml 等）
均为**本地运行的代码库**，不具备数据采集或网络通信能力。相关开源许可信息见本
项目仓库中的 `README.md` 与各组件自带许可证。

---

## 六、儿童隐私

本应用不面向儿童单独提供服务，也**不会**收集任何年龄、身份相关信息。
由于其不采集任何个人信息，儿童使用本应用同样不存在个人信息泄露风险。
若你是未成年人的监护人，对本应用有任何疑问，欢迎通过下方联系方式与我们沟通。

---

## 七、你的权利

由于本应用不收集任何个人信息，因此不存在需要你行使的"查询、更正、删除个人信息"
或"撤回授权"等操作。你可以随时：

- 通过系统设置**卸载本应用**，即可清除全部本地数据；
- 若你在保存对话框中误存了文件，可自行在文件管理器中删除。

---

## 八、政策更新

若本应用未来新增需要处理个人信息的功能（例如联网下载模型库、账号同步等），
我们会在**功能上线前**更新本政策，并在应用内或应用市场页面显著提示。

本政策的任何修改都会更新顶部的"生效日期"。

---

## 九、联系方式

如对本隐私政策有任何疑问、意见或投诉，请联系：

- **开发者**：Darkhe55
- **邮箱**：`starray@mail.darkh.top`
- **项目主页**：https://github.com/Darkhe55/TriangleMeshBrowser

我们会在收到反馈后的合理期限内予以回复。

---

# PrismViewer Privacy Policy (English Summary)

**Effective date**: September 20, 2026
**App name**: PrismViewer (棱镜模型查看器)
**Bundle name**: `com.darkhe55.prismviewer`
**Developer**: Darkhe55

**PrismViewer does not collect, transmit, or share any personal information.**

- No device identifiers, location, contacts, media, or usage analytics are collected.
- No accounts, no ads, no third-party analytics or advertising SDKs.
- The app declares **no sensitive permissions** and has **no network access**.
- Opening a model file uses the system file picker: only the file you explicitly
  select is read, and it is copied into the app's private sandbox purely for parsing.
- Screenshots are written to the app's private cache and, if you choose, saved to a
  location you pick via the system save dialog. Nothing leaves your device.
- Uninstalling the app removes all local data.

All rendering and model parsing happen entirely on your device.

Contact: `starray@mail.darkh.top` · https://github.com/Darkhe55/TriangleMeshBrowser
