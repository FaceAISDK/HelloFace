# HelloFace

[English](README.md) | 简体中文

**0.7.0 Beta**

Android、iOS、Harmony、Windows 和 Linux SDK 与 Demo 统一更新至 0.7.0。

### 离线人脸检测、采集、活体、1:1 比对及 1:N/M:N 搜索。

![HelloFace](AboutHelloFace.png)

| 平台 | 环境 | 接入说明 |
|---|---|---|
| Android | Android 6.0+ · ARM64/ARMv7 · JDK 17 · SDK 36 | [Demo](platforms/Android/README.zh-CN.md) · [SDK](platforms/Android/docs/SDK.zh-CN.md) |
| iOS | iOS 15+ · Swift 6 · Xcode | [Demo](platforms/iOS/README.zh-CN.md) · [SDK](platforms/iOS/SDK/API.zh-CN.md) |
| Harmony | HarmonyOS API 20+ · ARM64 · DevEco Studio 26 | [Demo](platforms/Harmony/README.zh-CN.md) · [SDK](platforms/Harmony/SDK.zh-CN.md) |
| Linux | Ubuntu 22.04+ · x86_64/ARM64 · X11/XWayland | [Demo](platforms/Linux/README.zh-CN.md) · [SDK](platforms/Linux/runtimes/x86_64/sdk/README.zh-CN.md) |
| Windows | Windows 10/11 x64 · CPU | [Demo](platforms/Windows/README.zh-CN.md) · [SDK](platforms/Windows/sdk/README.zh-CN.md) |
| Web 人脸录入 | 浏览器 | [在线体验](https://hiface-mdp.pages.dev/face/) |

## 克隆 Demo 与 SDK

本仓库使用 Git LFS 保存运行所需的二进制。请先安装 Git LFS 再克隆，确保工作区得到实际的 SDK 和 Demo 二进制，而不是 LFS 指针文件：

```bash
git lfs install
git clone --branch 0.7.0 --depth 1 https://github.com/FaceSDKPro/HelloFace.git
cd HelloFace
```

如果安装 Git LFS 前已经克隆仓库，请补充下载二进制：

```bash
git lfs install
git lfs pull
```

然后按照 [Android](platforms/Android/README.zh-CN.md)、[iOS](platforms/iOS/README.zh-CN.md)、[Harmony](platforms/Harmony/README.zh-CN.md)、[Linux](platforms/Linux/README.zh-CN.md) 或 [Windows](platforms/Windows/README.zh-CN.md) 的 Demo 说明运行。GitHub Release 自动生成的源码归档也已配置为包含实际 Git LFS 对象。

**更多平台（计划）：** 将支持微信小程序、Chrome、uni-app、Flutter 插件、React Native（RN）插件等。

- SDK正在Beta 迭代公测中，请及时升级SDK版本。
- 当前模型不支持12周岁以下儿童人脸识别，不能用于金融相关场景。
- 人脸搜索识别要求摄像头成像清晰；室外环境下，摄像头宽动态范围须大于 100 dB。
- Windows/Linux 桌面与摄像头待真机摄像头验证；移动端运行需配置自己的应用签名。

[版本下载](https://github.com/FaceSDKPro/HelloFace/releases) · [Android APK 构建](https://github.com/FaceSDKPro/HelloFace/actions/workflows/android-demo.yml) · [问题反馈](https://github.com/FaceSDKPro/HelloFace/issues)

邮箱联系：[FaceAISDK.Service@gmail.com](mailto:FaceAISDK.Service@gmail.com)

微信联系：

<img src="WeChat.jpg" alt="微信二维码" width="200">
