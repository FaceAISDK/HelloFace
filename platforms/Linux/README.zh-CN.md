# HiFace Linux 桌面 Demo

[English](README.md) | 简体中文

Linux x86_64、ARM64，CPU 推理，公共 C ABI v1。提供桌面摄像头程序、图片/人脸库命令行及完整 Demo 源码；依赖独立 Linux SDK。
运行基线为 Ubuntu 22.04（glibc 2.35），兼容目标为 Ubuntu 24.04；桌面使用 X11 或 Wayland 中的 XWayland。

**当前模型不支持12周岁以下儿童人脸识别，不能用于金融相关场景。** SDK正在Beta 迭代公测中，请及时升级SDK版本。

## HelloFace 中直接运行

从 HelloFace 仓库进入 `platforms/Linux`，执行 `./self-test.sh`、`./run-demo.sh`。启动脚本根据 `uname -m` 自动选择 `runtimes/x86_64` 或 `runtimes/arm64`。
SDK 二进制、公共头文件和模型在 `runtimes/{arch}/sdk`；`src` 是集成 Demo 源码。SDK 接入见对应 SDK 的 `README.zh-CN.md`。
编译 Demo 时 CMake 同样自动选择宿主架构，也可传 `-DHIFACE_SDK_DIR`。开发和 CLI 命令中的 `bin`、`sdk` 路径应加 `runtimes/{arch}/` 前缀。

## 单架构压缩包直接运行

在 Linux 执行 `uname -m`：`x86_64` 选择 x86_64 包，`aarch64` 选择 arm64 包。
复制完整 `.tar.gz` 到 Linux 本地目录，在 Linux 内解压。包可使用 `runtimes/{arch}/` 或直接 `bin/`、`sdk/` 布局；请保留完整目录和启动脚本的相对位置。

```bash
# 解压下载的完整压缩包后，进入其根目录：
runtime=runtimes/x86_64  # ARM64 改为 runtimes/arm64
[ -d "$runtime" ] || runtime=.
sha256sum -c FILES.sha256
./self-test.sh
./run-demo.sh
# 指定摄像头：
./run-demo.sh /dev/video2

# 无需 X11 的摄像头链路检查：
"$runtime/bin/hiface_desktop" --camera-test /dev/video2

# 预览和人脸分析前将摄像头像素逆时针旋转 90°：
HIFACE_CAMERA_ROTATE=left ./run-demo.sh /dev/video2

# Xorg 保持原生横屏，HiFace 内部以逆时针 90° 绘制竖屏全屏界面：
HIFACE_DISPLAY_ROTATE=left ./run-demo.sh /dev/video2

# 自助机/服务模式：SDK 初始化后进入 Face search 并启动连续搜索：
HIFACE_START_SEARCH=1 ./run-demo.sh /dev/video2
```

无需安装编译器、Qt 或 OpenCV。Ubuntu 缺少桌面运行库时：

```bash
sudo apt update
sudo apt install -y libx11-6 xwayland v4l-utils
v4l2-ctl --list-devices
```

从 Linux 图形桌面终端启动；纯 SSH/无桌面环境可运行自检和图片/数据库命令。
摄像头需支持 V4L2 单平面或多平面 MMAP，以及 YUYV、MJPEG、NV12 或 NV21（单缓冲区平面打包）。
默认请求 640×360、25 FPS，实际参数由驱动协商；Rockchip ISP 的 NV12 多平面采集接口已支持。
设置 `HIFACE_CAMERA_ROTATE=left`（也可写 `ccw` 或 `90ccw`），可在显示和人脸分析前将横屏帧转为 360×640；反方向使用 `right`（也可写 `cw` 或 `90cw`）。
`HIFACE_DISPLAY_ROTATE=left` 仅在 HiFace 窗口内部旋转完整界面并反向映射鼠标/触摸坐标；请保持 Xorg 输出为 `normal`，避免 DRM 全屏旋转引起 `flip timeout`。

Rockchip 设备可用独立的全分辨率 ISP 通道显示高清预览，同时保持识别流为 640×360：

```sh
HIFACE_PREVIEW_DEVICE=/dev/video32 \
HIFACE_PREVIEW_WIDTH=1280 HIFACE_PREVIEW_HEIGHT=720 \
HIFACE_PREVIEW_OUTPUT_WIDTH=576 HIFACE_PREVIEW_OUTPUT_HEIGHT=1024 \
./run-demo.sh /dev/video31
```

可选预览通道通过设备上的 `librga.so.2` 一次完成 NV12/NV21 缩放、颜色转换和摄像头旋转。RGA 直接输出 X11 原生 BGRX 排列，本地 X Server 支持时使用 MIT-SHM 上传，避免整帧 CPU 通道交换和套接字复制；两项优化均保留通用回退路径。第二路 ISP 或 RGA 不可用时，Demo 会自动回退到识别流，不影响启动。

T92P 的 J3 白光与 J19 红外灯分开控制，J4 三色灯用于状态提示。RGB 模式采用白光自动补光，红外保持关闭。板级启动脚本以 root 运行 `boards/t92p-lighting.sh prepare`，再为 Demo 设置 `HIFACE_LIGHT_PWM_PATH=/sys/class/pwm/pwmchip0/pwm0` 与 `HIFACE_LIGHT_SENSOR_DEVICE=/dev/v4l-subdev5`（需按 `cv2002_720` 传感器名称确认）。应用只获得白光 duty/enable 的权限，不以 root 运行；服务停止时运行同一脚本的 `off`。

自动策略沿用厂商 1 ms PWM 周期，以 CV2002 的曝光比例、0.3 dB/单位模拟增益及中央画面亮度估计暗光需求，并非独立照度计的 lux。持续暗光 2 秒后以每 0.5 秒 5% 渐亮至 25%；亮暗阈值分离，至少点亮 15 秒且持续明亮 6 秒才渐灭。相机停止、帧中断或传感器读数失败时关闭白光，避免补光反馈造成频繁闪烁。未配置环境变量的设备不启用板级灯控。
权限不足时检查 `/dev/video*` 的访问权限；加入 `video` 组后需完整注销再登录，不必用 root 启动 Demo。

## 桌面功能

启动显示功能卡片首页，前八项按 Android 顺序排列：检测、引导采集、动态验证、活体、连续搜索、图片比对、图片搜索和相机设置；另有录入、人脸库和高级设置，共 11 个独立功能页。进入相机功能页时连接所选设备，再点击 Start 启动本页流程；回首页或进入图片/人脸库页会关闭相机。启动时不自动打开相机。

| 入口 | 使用方法 |
|---|---|
| F1 Face search | 连续 1:N/M:N；其余相机功能由首页各自进入 |
| F2 Photo comparison | 两张本地图片比对与输入预览；图片检测、设置参考和图片录入分别在对应功能页的 Menu 中选图 |
| F3 Face library | 分页、精确 ID 查询、group/tag 筛选、元数据修改、删除、选择验证参考、导入/导出 |
| F5 Image search | 一图最多 10 脸，逐脸匹配 ID/分数、框与对齐缩略图，分页选择高亮 |
| F4 Settings | 检测人数、468 点/头姿、动作顺序、验证活体/阈值、搜索参数、摄像头尺寸/FPS/格式与镜像 |

相机快捷键选择独立页面：`D` 检测、`C` 抓拍、`L` 活体、`V` 验证、`E` 录入、`S` 搜索、`P` 暂停/恢复搜索、
`X` 取消流程；`Esc` 先取消活动流程，再按可返回首页；相机 Menu 也提供首页入口。`M` 镜像、`N` 切换/重连摄像头、`Q` 退出。输入框获得焦点后按键用于编辑，
`Ctrl+V` 粘贴、`Ctrl+A` 全选、`Ctrl+C` 复制、`Tab` / `Shift+Tab` 切换控件；`Esc` 先退出编辑。

先抓拍、保存图片参考，或在人脸库点击 Use as reference，再执行 Verify。当前参考会保存到本地，重启后恢复；
选择库内参考时原始 HF1 不重新量化。相机/单图录入先采集并预览，填写 ID/group/tag 后确认保存；重复 ID 保留草稿供修改，重拍、取消或离页不会入库，也不会替换验证参考。库中批量导入有覆盖确认。
抓拍、活体、验证和搜索共用一个相机流程槽。切页取消当前相机流程；暂停搜索释放范围缓存，恢复时重新预热。

Settings → Liveness / verify 用动作按钮顺序添加 blink / nod / shake / mouth，最多四步，允许重复；
Undo / Clear 删除步骤，动作留空运行静默活体。
Verify 默认启用静默活体，可在 Settings 关闭；Liveness 始终经过 Core 活体流程。
1:N 搜索默认开启同帧静默活体；M:N 自动关闭活体。搜索命中但活体拒绝时显示拒绝，不显示通过绿框。
所有结论、动作判定和阈值算法来自公共 Core。

Face library 的 Import demo 10k 使用随 Demo 附带的 10000 条合成随机特征，仅用于导入、分页和性能演示；不来自真人，也不能用于验证识别准确率。JSON 结构与 Android 兼容。
Import HF1 JSON 校验格式和特征空间，100 条一批提交；Stop import 保留已提交批次。
Import image folder 逐张提取本地目录的 JPEG/PNG/BMP/PPM，文件名去扩展名作为 ID，按 group 筛选框的值录入；
无脸、损坏图片会计为失败并跳过，写库失败终止并保留此前批次。导入会新增或覆盖同 ID。
Export HF1 JSON 使用当前已加载的 group/tag 筛选，保持原始 Q7 payload；已有同名输出文件会拒绝覆盖。
同一数据库导出时应暂停其他进程的写入。SDK 数据库最多 50000 条。

全屏人脸搜索页由 V4L2 采集线程持续发布最新帧，预览以 25 FPS 刷新；模型/数据库工作线程只取最新帧，以 8 FPS 为上限异步识别，实际速率随单帧推理耗时变化。
识别阻塞不会阻塞采集和预览；捕获队列丢弃旧帧，界面分别显示 Preview FPS、Recognition FPS 和最近一次推理耗时。
无人值守的自助机或服务部署可设置 `HIFACE_START_SEARCH=1`，使 Demo 在 SDK 初始化完成后进入 Face search 并立即启动连续搜索；默认仍停在首页且不打开摄像头。

## 图片人脸搜索

打开 **F5 Image search** → Browse 选择照片，选择后自动开始；也可输入路径后按 Enter 或点击 Search；搜索中按 Esc 可取消。
照片按比例显示，紧凑人脸编号与结果卡对应，ID/分数在卡片和选中详情中显示，点击原图中的人脸或结果卡高亮对应人脸；方向键切换，Home / End 跳到首尾，Page Up / Page Down 翻页，右侧滚轮逐脸切换。
选中人脸始终显示在当前页，缩放窗口后保留选择；预览下方展示所选标签及 group/tag，卡片区分匹配、小脸、未匹配和技术错误。
图片中的小脸、提取失败、未匹配分别保留框和提示；无人脸或库为空有单独说明，只有成功匹配才显示 ID 与分数。
结果卡显示 333×333 对齐人脸，小脸使用占位图，不伪造特征或分数。

与 Android 图片搜索保持一致：最多 10 张脸、全库 Top-1、共享默认阈值 0.80；不读取相机搜索的 group/tag、人数或活体开关。
静态图片不做活体检测；相似度表示 SDK 身份相似度，不代表身份概率。
JPEG 自动处理 EXIF 旋转/镜像，JPEG/PNG/BMP 超过 4096 的图片先等比缩小，再裁掉奇数末行/列；不放大小图。
照片文件限制 64 MiB，解码前限制 64 百万像素和 16384 单边；PPM 沿用 4096 单边限制。

正在搜索时仍可换图、再次 Search 或 Cancel；请求序号使旧结果失效，队列只保留最新照片。
当前 SDK 同步调用自然结束后才释放缓冲或退出，界面保持响应。
照片搜索不写人脸库；先去 Face library 导入特征，或使用相机/图片录入建立库。

## 数据与诊断

数据默认在 `${XDG_DATA_HOME:-$HOME/.local/share}/hiface-demo`，可用 `HIFACE_DATA_DIR` 覆盖。
包括 `hiface-search.db`、`reference.hf1`、`settings.conf`、主动 Capture 保存的 `captures/*.ppm`；预览不落盘。
SQLite 与本地 HF1 未加密，HF1 是编码，不是加密；宿主需自行实现业务数据保护和清理。
界面使用英文标签，文件路径、ID 和分组支持 UTF-8；中文显示与输入依赖桌面字体及输入法。

```bash
./diagnose.sh > linux-diagnostic.txt 2>&1
./run-demo.sh /dev/video0 2>&1 | tee desktop-run.log
```

构建信息与验证报告见随包元数据及说明；`FILES.sha256` 校验包内文件，压缩包同目录提供 SHA-256 清单。诊断脚本不导出人脸库或参考特征。

## 独立编译 Demo

以下命令从已解压 Demo 根目录执行。CMake 自动选择宿主架构的 SDK；模型已内置，运行时传 `--builtin`，不要指定外部模型目录。

Demo 与 SDK 的依赖方向：`Demo → HiFace::HiFace（公开 C ABI）→ Core`。
`src/desktop/workflow.cpp` 中的 `FaceWorkflow` 是 Demo 的流程编排类；X11 渲染、V4L2、图片解码、HF1 JSON 和本地设置均属于 Demo。
SDK 不依赖 Demo。对应 `runtimes/{arch}/sdk/README.zh-CN.md`（或直接布局的 `sdk/README.zh-CN.md`）提供公开 SDK 接入说明。

从 Demo 包自身编译，无需仓库或 Core 源码：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
./build/hiface_demo self-test --builtin
./build/hiface_desktop --builtin "$HOME/.local/share/hiface-demo/hiface-search.db" /dev/video0
```

可传 `-DHIFACE_SDK_DIR=/absolute/path/to/standalone-sdk` 使用单独 SDK 包；
`-DHIFACE_BUILD_LINUX_DESKTOP=OFF` 只编译 CLI；`-DBUILD_TESTING=ON` 编译 Demo 回归测试后运行 `ctest --test-dir build`。
源码支持 GNU C++17，CMake 3.22.1+；SDK 已自带算法运行库，不需要再构建 NCNN。

界面采用侧栏、预览卡片和独立工作流操作区；设置分为 Detection、Liveness / verify、Search、Camera。
人脸录入先用相机或图片采集，再在预览确认框填写非空 ID / group / tag；点击确认才写库。重复 ID 保留草稿；重拍、取消、换页不会写库，也不会替换 1:1 基准。
人脸库分为 People 与 Import / export，导入显示进度并允许停止；删除、覆盖导入、清除参考和放弃未保存设置需确认。
输入支持 UTF-8、鼠标定位光标、全选/复制/粘贴；过长粘贴会保留原输入。
Enter 可提交图片路径、相机设备、人脸库筛选和设置；F1..F5 在编辑框中也能切页，弹窗内保持键盘焦点。
点击当前页面保留草稿；设置错误会定位到对应分组和输入框，弹窗取消后恢复原按钮焦点。
文件选择器的目录错误与录入校验在弹窗内显示，便于直接修正。按钮支持悬停、禁用、处理中反馈；未匹配/空库使用提示色，技术错误单独显示。
窗口初始尺寸适配当前屏幕，最小 960×700，适用于 VMware 常见的 1024×768 桌面。

## CLI

从 Demo 根目录执行，按实际布局选择运行目录：

```bash
runtime=runtimes/x86_64  # ARM64 改为 runtimes/arm64
[ -d "$runtime" ] || runtime=.
"$runtime/bin/hiface_demo" --help
"$runtime/bin/hiface_demo" detect --builtin face.jpg
"$runtime/bin/hiface_demo" compare --builtin first.jpg second.png
"$runtime/bin/hiface_demo" capture --builtin face.jpg aligned.ppm
"$runtime/bin/hiface_demo" enroll --builtin hiface-search.db person-001 face.jpg
"$runtime/bin/hiface_demo" search --builtin hiface-search.db query.jpg
# 多人图片搜索，输出每张脸的框、状态、匹配 ID/分数的 JSON：
"$runtime/bin/hiface_demo" search-faces --builtin hiface-search.db group-photo.jpg
"$runtime/bin/hiface_demo" gallery-import hiface-search.db assets/features_10000.json
"$runtime/bin/hiface_demo" gallery-export hiface-search.db export.json
```

CLI enroll 为 upsert；桌面单条录入重复 ID 拒绝。CLI gallery-import 是覆盖导入，逐批提交；
失败打印此前已保存数。导出文件已存在时拒绝覆盖。

## 验证范围

摄像头以有界帧池复用图像内存，只复用已无 UI/SDK 引用的帧；积压时丢弃旧帧，不增长队列。
ISP 预览缓冲优先通过 V4L2 DMA-BUF 交给 RGA；不支持导出或转换失败时回退到映射内存。
可用 `HIFACE_PREVIEW_DMA=0` 强制兼容路径进行对照，不影响独立的 I420 识别输入。
V4L2 使用最长 10ms 的就绪等待，SDK 工作线程按事件唤醒；UI 在新帧、新状态或输入事件到来时重绘，
静态页面不再持续进行整屏合成。原有 X11/Xrender 竖屏后端保留，未切换 DRM/KMS。
`HIFACE_CPU_THREADS=2` 可为显示和采集保留 CPU 余量，`4` 优先单次推理延迟；不设置时沿用 SDK 自动策略。
线程数为进程级设置，修改后重启 Demo，不改变模型、特征、搜索阈值或活体检查。
`HIFACE_PERF_LOG=1` 每 5 秒记录预览/识别帧率、末次分析和平均绘制耗时，仅在排查时启用。

UI 自动测试复用实际绘制指令，检查 960×700、1024×768、1440×900 布局、控件碰撞、
禁用/忙碌状态、UTF-8 编辑、录入弹窗、设置草稿、删除确认与导入进度。
Linux CI 配置了 Xvfb 首页及全部 11 个功能页窗口渲染和 Ubuntu 22.04 → 24.04 独立消费；未执行的 CI 不算已通过。
**交叉编译与宿主机逻辑测试不代表 Linux 窗口、USB 摄像头、真人识别或活体攻击已经验收，需在目标设备实测。**

首页新增独立 Camera settings 页，只修改分辨率、FPS、格式和镜像；应用/重置不会改动动作与搜索参数。
Photo comparison 页只演示双图片比对；静态图片检测、图片基准和图片录入分别在对应功能页的 Menu 选择照片。
选图后关闭实时相机，避免覆盖静态结果。功能切换释放旧图片/预览/搜索结果；窄竖屏首页可滚轮或 Page Up/Down 滚动。
