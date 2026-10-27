# HiFace Windows Demo

支持 Windows 10/11 x64。先运行 `self_test.bat`，看到 `RESULT: PASS` 后运行 `run_demo.bat`。
保留完整目录，无需开发工具或下载模型。

## 使用

启动显示功能首页，前八项依次为检测、采集/基准、动态 1:1、活体、1:N/M:N、双图片比对、图片搜索、相机设置，与 Android 首页顺序一致。
人脸录入、人脸库作为额外独立入口，共十个功能页；侧栏与首页指向同一页面，Esc 在无活动任务时返回首页。
离开任何相机功能页都会关闭相机、取消流程并丢弃未确认录入；新页面重新选择操作和打开相机。
相机流程先选择并打开摄像头；1:1 需先采集基准，搜索需先录入人员。
参数修改后点击“应用修改”，进行中的流程可取消。
各功能页分别保留人数、阈值、动作、搜索模式/间隔/超时和人员字段草稿，不会从另一页带入参数。
相机设置应用后保存分辨率、旋转与镜像到 `camera-settings.txt`，下次启动读取；设置页只预览，不执行人脸分析。

- 人脸库“导入 JSON”可导入 `data/features_10000.json` 的 1 万条 AI 合成人脸特征；相同 ID 会覆盖。
- `data/images_1000.json` 是 1000 张 AI 合成人脸图片的公开 URL 清单，供下载参考；图片使用本地多选导入，不要将此文件作为特征 JSON 导入。
- 图片搜索：选照片查看每张人脸的 ID、分数和对齐图；默认全库 Top-1 / 0.80，不执行活体。
  `Ctrl+O` 选图、`F5` 重搜、`Esc` 取消，左右键选择人脸。

数据位于 `%LOCALAPPDATA%\HiFaceDemo`：`hiface-search.db` 为人脸库，`verification.hf1` 为 1:1 基准，`logs/` 为日志与自检报告。
可将环境变量 `HIFACE_DATA_DIR` 设为绝对目录以隔离测试数据；未设置时沿用系统用户目录。
相机打不开时检查 Windows 桌面相机权限、设备占用和 Media Foundation；缺 DLL 时重新完整解压。
SDK正在Beta 迭代公测中，请及时升级SDK版本。主流程与界面验收见包内 `Windows真机测试清单.txt`（源码位于 `../package/`）。

## 独立录入流程

从首页或搜索页进入“人脸录入”，先打开相机并开始采集，或选择图片再点击“从图片采集待确认人脸”。
采集成功只保留临时预览，此时人脸库数量不会增加；填写人员 ID、分组和标签，核对预览后点击“确认预览并保存人员”。
空 ID 和重复 ID 会拒绝保存；重复时可修改 ID 后重试。重新采集、取消、选择另一张图片或离开本页会丢弃待保存结果。
搜索页只负责 1:N/M:N 识别和暂停/恢复；录入与人脸库使用独立页面，符合 Android 的搜索子页面结构。

## Android 对照与验证边界

| Android 细节 | Windows 当前行为 |
|---|---|
| 首页功能卡片、独立 Activity | 前八项相同顺序；每项独立页面，录入与人脸库也可从首页直达 |
| 录入后预览并确认保存 | 相机/图片均先形成临时草稿，确认后才写库；取消或离页不写库 |
| Activity 页面状态隔离 | 参数草稿按页保存；离页释放相机、预览、临时人脸和旧图片结果 |
| 相机设置跨重启保留 | 应用后原子保存分辨率、旋转和镜像；损坏文件回退到默认值 |
| 相机资源与分析生命周期 | 每次离页停止采集与流程；相机设置及待确认录入阶段不做推理 |
| 基准与人脸库 | 当前保留一份活动 HF1 基准，可用库内不同 ID 切换；尚无 Android 多基准列表界面 |
| 网络图片导入 | 使用本地多选图片与公开 URL 清单，尚无 Android 的网络下载进度界面 |
| 页面亮度 | 不模拟 Android 的窗口 90% 亮度策略；Windows 亮度由系统/显示器管理 |

布局回归覆盖 100/125/150/200% 几何尺寸；实际 Windows 高 DPI、输入法、相机权限/占用/拔插、
长时间运行、真人识别和照片/视频活体攻击效果仍须真机验收。Wine 与交叉编译检查不能替代这些项目。
SDK、公开 ABI 与稳定模型发布策略保持固定，本次只修改 Demo 和接入说明。

## 修改与编译

使用完整 SDK/Demo 包时，在平台根目录运行 `build_demo.ps1`。
单独的 Demo 包需另备 SDK、Visual Studio 2022 C++ 桌面工具和 CMake 3.22.1+，在本 Demo 目录执行：

```powershell
cmake -S source -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:/HiFace/sdk
cmake --build build --config Release
./build/bin/Release/hiface_demo.exe
```

`source/` 包含界面、相机和业务流程；`source/integration/` 是调用 SDK 公共 C API 的 C++ 示例。

## 构建后的程序包

重新编译 EXE 或替换 DLL 后，使用 `tools/sign_product.py` 更新匹配的 `.hfpackage`；发布密钥保留在构建端。示例：

```powershell
python tools/sign_product.py --platform windows --root . --entry bin/hiface_demo.exe --sdk bin/hiface.dll --private-key C:/secure/publisher.pem --product-id com.hiface.demo --version 0.8.0 --replace
```

CLI 使用自己的 `bin/hiface_cli.exe.hfpackage`，并采用相同产品和发布密钥。SDK 的固定版本、头文件、导入库与 DLL 须匹配。HF1 与人脸数据库未加密，导出反馈前移除个人数据和本机路径。
