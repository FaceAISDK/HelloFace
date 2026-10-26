# HiFace iOS SDK 0.7.0

English | [简体中文](README.zh-CN.md)

Add the binary Swift package from https://github.com/FaceAISDK/HiFace-iOS at version `0.7.0`. For a local delivery, add this directory as a local package. Select the `HiFace` product and retain both HiFace and HiFaceNative targets.

**Model scope: The current model does not support face recognition for people under 12 and must not be used in financial scenarios.**

Use `HiFaceSDK()` with its default model bundle. Search configuration starts with `FaceSearchSessionConfiguration.default`. See [API](API.md) for supported calls and behavior.

Requires iOS 15+ and Swift 6. Validate camera flows on iPhone. The SDK is in iterative public Beta testing. Please keep the SDK version up to date.
