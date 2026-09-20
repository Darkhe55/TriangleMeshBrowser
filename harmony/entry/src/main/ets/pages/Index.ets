// browser/harmony/entry/src/main/ets/pages/Index.ets
// ArkTS 壳: 托管 XComponent 渲染面, 并在 UI 线程承担 Native 做不了的系统交互。
//
// 与 Native (libentry.so) 的分工:
//   Native  = 渲染 + 模型解析 + 截图写沙箱
//   ArkTS   = 系统文件选择器 (DocumentViewPicker) + 截图保存对话框
// 两者以 200ms 轮询协作, 避免跨线程回调的复杂度:
//   1. pollOpenFileRequest() == true -> 弹选择器 -> 复制进沙箱 -> submitModelPath()
//   2. pollScreenshotPath() 非空      -> 弹保存对话框 -> 把沙箱 PNG 写到用户选的位置
import { hilog } from '@kit.PerformanceAnalysisKit';
import { fileIo as fs, picker } from '@kit.CoreFileKit';
import { common } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import nativeEntry from 'libentry.so';

const DOMAIN: number = 0x0000;
const TAG: string = 'PrismViewer';
const POLL_MS: number = 200;

// 与桌面版一致的 13 种可打开格式 (用于文件选择器的后缀过滤)
const MODEL_SUFFIXES: string[] = [
  '.obj', '.stl', '.ply', '.off', '.pmx',
  '.fbx', '.gltf', '.glb', '.dae', '.3mf',
  '.las', '.laz', '.e57',
];

@Entry
@Component
struct Index {
  private ctx = getContext(this) as common.UIAbilityContext;
  private pollTimer: number = -1;
  private busy: boolean = false;          // 防止重复弹出选择器
  private modelsDir: string = '';
  private shotsDir: string = '';

  aboutToAppear(): void {
    this.prepareSandbox();
  }

  aboutToDisappear(): void {
    this.stopBridge();
  }

  /** 准备沙箱目录, 并告知 Native 截图写入位置 */
  private prepareSandbox(): void {
    try {
      this.modelsDir = `${this.ctx.cacheDir}/models`;
      this.shotsDir = `${this.ctx.cacheDir}/shots`;
      if (!fs.accessSync(this.modelsDir)) {
        fs.mkdirSync(this.modelsDir);
      }
      if (!fs.accessSync(this.shotsDir)) {
        fs.mkdirSync(this.shotsDir);
      }
      nativeEntry.setSandboxDir(this.shotsDir);
      hilog.info(DOMAIN, TAG, 'sandbox ready: %{public}s', this.modelsDir);
    } catch (err) {
      const e = err as BusinessError;
      hilog.error(DOMAIN, TAG, 'sandbox init failed: %{public}d %{public}s', e.code, e.message);
    }
  }

  private startBridge(): void {
    if (this.pollTimer !== -1) {
      return;
    }
    this.pollTimer = setInterval(() => {
      this.pollNative();
    }, POLL_MS);
    hilog.info(DOMAIN, TAG, 'bridge started');
  }

  private stopBridge(): void {
    if (this.pollTimer !== -1) {
      clearInterval(this.pollTimer);
      this.pollTimer = -1;
    }
  }

  /** 轮询 Native 侧的三类事件: 打开文件 / 截图完成 / 错误 */
  private pollNative(): void {
    try {
      // NAPI 返回值需显式标注类型: ArkTS 严格模式禁止隐式 any
      const wantOpen: boolean = nativeEntry.pollOpenFileRequest();
      if (!this.busy && wantOpen) {
        this.openFilePicker();
      }
    } catch (err) {
      const e = err as BusinessError;
      hilog.error(DOMAIN, TAG, 'pollOpenFileRequest failed: %{public}s', e.message);
    }

    try {
      const shot: string = nativeEntry.pollScreenshotPath();
      if (shot.length > 0) {
        this.saveShotWithPicker(shot);
      }
    } catch (err) {
      const e = err as BusinessError;
      hilog.error(DOMAIN, TAG, 'pollScreenshotPath failed: %{public}s', e.message);
    }

    try {
      const errMsg: string = nativeEntry.pollLastError();
      if (errMsg.length > 0) {
        hilog.warn(DOMAIN, TAG, 'native error: %{public}s', errMsg);
      }
    } catch (err) {
      const e = err as BusinessError;
      hilog.error(DOMAIN, TAG, 'pollLastError failed: %{public}s', e.message);
    }
  }

  /** 弹出系统文件选择器, 把选中文件复制进沙箱后交给 Native */
  private async openFilePicker(): Promise<void> {
    this.busy = true;
    try {
      const documentPicker = new picker.DocumentViewPicker(this.ctx);
      const options = new picker.DocumentSelectOptions();
      options.maxSelectNumber = 1;
      options.fileSuffixFilters = MODEL_SUFFIXES;

      const uris = await documentPicker.select(options);
      if (uris === undefined || uris.length === 0) {
        hilog.info(DOMAIN, TAG, 'file picker cancelled');
        return;
      }

      const srcUri = uris[0];
      const rawName = srcUri.split('/').pop();
      const baseName = (rawName === undefined || rawName.length === 0) ? 'model' : rawName;
      // 加时间戳前缀避免重名覆盖
      const dstPath = `${this.modelsDir}/${Date.now()}_${baseName}`;

      const srcFile = fs.openSync(srcUri, fs.OpenMode.READ_ONLY);
      const dstFile = fs.openSync(dstPath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
      try {
        fs.copyFileSync(srcFile.fd, dstFile.fd);
      } finally {
        fs.closeSync(srcFile);
        fs.closeSync(dstFile);
      }

      hilog.info(DOMAIN, TAG, 'picked -> %{public}s', dstPath);
      nativeEntry.submitModelPath(dstPath);
    } catch (err) {
      const e = err as BusinessError;
      hilog.error(DOMAIN, TAG, 'file picker failed: %{public}d %{public}s', e.code, e.message);
    } finally {
      this.busy = false;
    }
  }

  /** 把 Native 写出的沙箱截图经系统保存对话框写到用户选择的位置 (无需额外权限) */
  private async saveShotWithPicker(sandboxPath: string): Promise<void> {
    try {
      const rawName = sandboxPath.split('/').pop();
      const fileName = (rawName === undefined || rawName.length === 0) ? 'prism_shot.png' : rawName;

      const savePicker = new picker.DocumentViewPicker(this.ctx);
      const options = new picker.DocumentSaveOptions();
      options.newFileNames = [fileName];
      options.fileSuffixChoices = ['.png'];

      const uris = await savePicker.save(options);
      if (uris === undefined || uris.length === 0) {
        hilog.info(DOMAIN, TAG, 'save cancelled, shot kept in sandbox: %{public}s', sandboxPath);
        return;
      }

      const srcFile = fs.openSync(sandboxPath, fs.OpenMode.READ_ONLY);
      const dstFile = fs.openSync(uris[0], fs.OpenMode.WRITE_ONLY | fs.OpenMode.TRUNC);
      try {
        fs.copyFileSync(srcFile.fd, dstFile.fd);
      } finally {
        fs.closeSync(srcFile);
        fs.closeSync(dstFile);
      }
      hilog.info(DOMAIN, TAG, 'screenshot saved: %{public}s', uris[0]);
    } catch (err) {
      const e = err as BusinessError;
      // 失败时截图仍在沙箱内, 记录日志即可
      hilog.error(DOMAIN, TAG, 'save screenshot failed: %{public}d %{public}s', e.code, e.message);
    }
  }

  build() {
    Column() {
      // Native XComponent: 由 native 层 (libentry.so) 接管 surface 与 OpenGL ES 渲染
      XComponent({
        id: 'prismViewer',
        type: XComponentType.SURFACE,
        libraryname: 'entry'
      })
        .onLoad(() => {
          hilog.info(DOMAIN, TAG, '%{public}s', 'XComponent onLoad');
          this.startBridge();
        })
        .onDestroy(() => {
          hilog.info(DOMAIN, TAG, '%{public}s', 'XComponent onDestroy');
          this.stopBridge();
        })
        .width('100%')
        .height('100%')
    }
    .width('100%')
    .height('100%')
  }
}
