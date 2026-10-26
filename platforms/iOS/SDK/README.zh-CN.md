# HiFace iOS SDK 0.7.0

[English](README.md) | 简体中文

最低 iOS 15，使用 Swift 6。

**模型适用范围：当前模型不支持12周岁以下儿童人脸识别，不能用于金融相关场景。**

## 添加依赖

在 Xcode 添加 `https://github.com/FaceAISDK/HiFace-iOS`，固定版本 `0.7.0`，选择 `HiFace` product。
本地交付包可直接添加为本地 Swift Package。保留 `HiFace` 和 `HiFaceNative` 两个二进制目标。

在 App Info.plist 添加 `NSCameraUsageDescription`，并配置自己的应用签名。

## 初始化

```swift
import HiFace
let sdk = HiFaceSDK()
```

使用默认模型配置。离开功能页时取消请求或关闭搜索会话，业务容器销毁时调用 `sdk.close()`。

公开接口、帧格式和生命周期要求见 [API 指南](API.zh-CN.md)。

SDK正在Beta 迭代公测中，请及时升级SDK版本。[更新说明](https://github.com/FaceSDKPro/)。
