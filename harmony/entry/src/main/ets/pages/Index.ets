import { hilog } from '@kit.PerformanceAnalysisKit';

const DOMAIN: number = 0x0000;
const TAG: string = 'PrismViewer';

@Entry
@Component
struct Index {
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
        })
        .onDestroy(() => {
          hilog.info(DOMAIN, TAG, '%{public}s', 'XComponent onDestroy');
        })
        .width('100%')
        .height('100%')
    }
    .width('100%')
    .height('100%')
  }
}
