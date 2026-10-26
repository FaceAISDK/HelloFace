import { DemoL10n } from './DemoL10n';
import { DemoTheme } from './DemoTheme';

/**
 * Shared back bar for light content screens, matching Android NavigationButton.OnLight layout.
 *
 * 浅色内容页的统一返回栏；布局与 Android NavigationButton.OnLight 对齐。
 */
@Component
export struct DemoPageHeader {
  @Prop title: string = '';
  @Prop titleSize: number = 20;
  onBack: () => void = () => {};
  // Optional trailing action preserves the same back-button layout and tap area on all content screens.
  // 可选右侧操作，保留所有内容页相同的返回按钮与点击区域。
  @Prop actionTitle: string = '';
  onAction: () => void = () => {};

  build() {
    Row({ space: 8 }) {
      Button({ type: ButtonType.Circle }) {
        Image($r('app.media.ic_navigation_back')).width(24).height(24).fillColor(DemoTheme.contentText)
      }.width(48).height(48).backgroundColor(Color.Transparent)
        .accessibilityText(DemoL10n.text("返回")).onClick(() => this.onBack())
      Text(this.title).fontSize(this.titleSize).fontWeight(FontWeight.Bold)
        .fontColor(DemoTheme.contentText).layoutWeight(1).maxLines(1)
      if (this.actionTitle.length > 0) {
        Button(this.actionTitle).type(ButtonType.Normal).height(44).fontSize(14)
          .padding({ left: 12, right: 12 }).borderRadius(12)
          .backgroundColor('#E5F2EE').fontColor(DemoTheme.contentAccent)
          .onClick(() => this.onAction())
      }
    }.width('100%').constraintSize({ minHeight: 60 })
      .padding({ left: 10, right: 12 })
      .backgroundColor(DemoTheme.contentBackground)
  }
}
