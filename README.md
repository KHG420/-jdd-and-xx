# 桌边的你们

Windows 和 macOS 都可以使用的原生双人桌宠。根据人物参考图制作，保留蜡笔画风、透明桌面窗口和双人互动，离线运行。

## 仓库与版本说明

仓库地址：[KHG420/-jdd-and-xx](https://github.com/KHG420/-jdd-and-xx)。当前原生双人桌宠版本为 **1.1.0**，macOS 使用 Swift / AppKit，Windows 使用 C++ / Win32。

仓库原有的 `desktop_pet.py`、`pet_longhair_transparent_*.png` 和 `图库/` 属于早期 Python 桌宠，已从当前目录删除；当前目录由原生双人桌宠替代，构建和运行入口见下文，使用 `Sources/`、`Windows/` 和 `Assets/`。旧版仅在 Git 历史中保留，可在历史提交 `caf0202` 中查看。

```sh
git clone git@github.com:KHG420/-jdd-and-xx.git
cd ./-jdd-and-xx
```

源码仓库不包含 `build/` 内的应用、ZIP、测试输出和本地工具。下表是本地构建并打包后的产物路径，不是已上传的 GitHub Release 下载链接；首次克隆后请先按“从源码构建”完成构建，再使用启动脚本。

## 下载与启动

| 系统 | 发布文件 | 启动方式 |
| --- | --- | --- |
| Windows 10 / 11，x64 | `build/releases/桌边的你们-Windows-x64.zip` | 解压后双击 `windows/DesktopPets.exe` |
| macOS 13+，Apple Silicon / Intel | `build/releases/桌边的你们-macOS-universal.zip` | 解压后双击 `桌边的你们.app` |

Windows 的 `.exe` 内嵌全部素材，可以单独复制使用，不需要管理员权限，也不用安装 Node、Python、.NET 或 Visual C++ Redistributable。Mac 的 `.app` 包含 arm64 和 x86_64 两个架构。

在本项目目录中，也可使用 `启动桌宠.command`（Mac）或 `启动桌宠-Windows.bat`（Windows）。这两个入口会打开对应的本地构建产物。

应用启动后，两个人出现在桌面右下角。**Windows 在系统托盘显示双人图标，macOS 在顶部菜单栏显示双人图标**。Windows 图标可能位于托盘的折叠菜单中。

## 怎么相处

- **点击人物**：打招呼；短时间连续点击会闹一点小脾气，随后自行恢复。
- **按住拖动**：进入被提起的姿势；放开后缓冲落地，位置会保存。落点限制在显示器可用区域。
- **右键人物**：选择看书 / 敲电脑、喝茶 / 吃饭、打盹、比心、叉腰，以及双人互动。
- **靠近两个人**：相距约 300 个逻辑点、脚边高度接近时，偶尔招呼对方、挪近，再一起看书、比心、拍拍头或拥抱。共同场景约 11 秒；可以点击结束，也可以一起拖动。
- **菜单栏 / 托盘图标**：切换单人或双人，调整 80% / 100% / 125% 大小，暂停、安静陪伴、隐藏、重置位置或退出。
- **安静陪伴**：隐藏短句，减少日常变化，不主动开始双人互动；手动动作仍可用。
- **暂停动作**：冻结行为和动作；点击人物会唤醒。隐藏后从菜单栏 / 托盘叫回来。

女孩会读书、喝茶；男孩会敲电脑、吃饭。夜间更容易犯困。每人 9 个姿态、4 个双人场景，结合眨眼、呼吸、摆动和状态过渡实现手绘精灵动画；不包含自由行走、语音或大模型聊天。

透明区域根据素材 alpha 判断点击穿透。角色不抢键盘焦点。两端支持显示器缩放、位置约束和系统减少动画设置；屏幕休眠时停止计时器。macOS 可跨 Spaces 显示，Windows 遵循系统虚拟桌面的窗口归属规则。

## 本地偏好

仅保存位置、大小、单人/双人选择与安静模式。

- macOS：`local.aq.desktop-companions` 偏好域，沿用原版本设置。
- Windows：`%APPDATA%\DeskCompanions\settings.ini`。

程序不联网，不采集屏幕、键盘或工作数据，不自动添加开机启动。

## 从源码构建

### macOS 应用

需要 Xcode Command Line Tools。以下命令构建通用 `.app`，并运行本机架构的 Mac 检查：

```sh
/bin/zsh scripts/build.sh
/bin/zsh scripts/test.sh
```

也可使用 `./启动桌宠.command --rebuild` 重建后启动。更新已运行应用前，先从菜单栏退出旧版。

### 在 macOS / Linux 交叉构建 Windows 应用

需要 MinGW-w64 开发工具。macOS 开发机可通过 `brew install mingw-w64` 安装；这是构建工具，不会成为用户的运行依赖。

```sh
/bin/zsh scripts/build-windows.sh
```

脚本生成 `build/windows/DesktopPets.exe`。嵌入资源与静态链接都在构建时完成。

### 在 Windows 原生构建和测试

安装 Visual Studio 2022 的“使用 C++ 的桌面开发”工作负载，在 **x64 Developer PowerShell** 中运行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1
powershell -ExecutionPolicy Bypass -File scripts/test-windows.ps1
```

测试会输出 `build/windows-verification/windows-self-test.txt` 和通过 Windows GDI+ 渲染的 `windows-preview.png`。也可以直接运行已构建程序的 `--self-test <输出目录>` 模式，无需编译器；测试模式不读写用户偏好，不添加托盘图标，结束后退出。

### 打包

两个应用构建完成后，在 macOS 运行：

```sh
/bin/zsh scripts/package.sh
```

生成两个 ZIP 和 SHA-256 校验文件到 `build/releases/`。

## 代码与验证

- `Sources/`：macOS 原生实现，AppKit 窗口与 Core Animation 贴图。
- `Windows/Behavior.hpp`：与 Swift 对齐的 Windows 行为规则及色键函数。
- `Windows/DesktopPets.cpp`：Win32 窗口、托盘、DPI、GDI+ 渲染和集成自测。
- `Windows/resources.rc`：将同一套 `Assets/` 图集嵌入 exe。
- `Tests/`：Mac 集成测试、Windows 行为与几何测试。
- `docs/跨平台验证记录.md`：本次双平台构建与验证的实际范围。
- `docs/素材说明.md`：现有素材的生成记录，两端不重复生成人物。

Mac 使用本地 ad-hoc 签名；Windows 未配置商业代码签名。发布物不声称经过 Apple 公证或 Windows SmartScreen 信誉认证。
