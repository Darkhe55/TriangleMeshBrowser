// libentry.so 的 ArkTS 类型声明
//
// Native 侧 (napi_init.cpp) 通过 napi_define_properties 导出下列方法,
// 供 pages/Index.ets 以轮询方式与渲染线程协作。

/** ArkTS 轮询: 是否有"打开文件"请求待处理 (取出即清除) */
export const pollOpenFileRequest: () => boolean;

/** ArkTS 文件选择器返回后, 提交已复制进沙箱的文件路径 (保留原扩展名) */
export const submitModelPath: (path: string) => void;

/** ArkTS 告知应用沙箱目录 (截图 / 导出写入此处) */
export const setSandboxDir: (dir: string) => void;

/** ArkTS 轮询: 取出新截图在沙箱内的路径 (无则返回空串) */
export const pollScreenshotPath: () => string;

/** ArkTS 轮询: 取出最近一次错误信息 (无则返回空串) */
export const pollLastError: () => string;
