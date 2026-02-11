# Touhou Community Reliant Automatic Patcher（thcrap）汉化版

[![Join the chat at http://discord.thpatch.net](https://discordapp.com/api/guilds/213769640852193282/widget.png)](http://discord.thpatch.net/)
[![Backers on Open Collective](https://opencollective.com/thpatch/backers/badge.svg)](#backers)
[![Build status](https://ci.appveyor.com/api/projects/status/6mcjhegcnplaojd0?svg=true)](https://ci.appveyor.com/project/brliron/thcrap-dev)
[![GitHub Release](https://img.shields.io/github/release/thpatch/thcrap.svg)](https://github.com/thpatch/thcrap/releases)

## 项目说明

本项目是 thcrap（Touhou Community Reliant Automatic Patcher）的汉化分支。

thcrap 本质上是一个可扩展、可定制的 Windows 内存补丁框架，最初主要用于东方 Project（Touhou Project）相关游戏的翻译补丁加载与更新。

## 汉化版说明

- 作者：`TianQi233`
- 哔哩哔哩：[`Instance_TianQi`](https://space.bilibili.com/457047949)
- GitHub：[`CNTianQi233`](https://github.com/CNTianQi233)

### 免责声明

本汉化版为非官方版本，与 thcrap 官方项目无隶属关系，仅供学习与交流使用。  
因使用本版本导致的任何问题或损失，由使用者自行承担。

### 维护说明

作者不会长期维护此版本，后续更新与支持不作保证。

## 本分支当前改动（摘要）

- 配置工具界面汉化（含 `thcrap_configure_v3`）。
- 在 `thcrap_configure_v3` 主界面增加“关于汉化版”入口。
- 关闭“自动更新到新版本”检查逻辑（避免自动升级覆盖汉化版行为）。

## 基础引擎主要特性

- 通过 DLL 注入将主引擎与插件注入目标进程。
- 支持对子进程的注入传播，便于与其他第三方补丁共存。
- 配置与补丁数据使用 JSON，便于开源与协作。
- 支持补丁堆栈（Patch Stacking），按优先级叠加多个补丁。
- 通过 Win32 API 包装提供透明 Unicode 文件名支持。
- 支持基于 SHA-256 和 EXE 大小识别不同程序版本。
- 支持二进制内存修改（Binhack）与断点钩子（Breakpoint）。
- 支持文件断点与按 JSON 描述的文件格式补丁处理。

## 模块组成

- `win32_utf8`：Win32 UTF-8 包装层。
- `thcrap`：主补丁引擎。
- `thcrap_loader`：命令行加载器。
- `thcrap_configure`：旧版配置向导。
- `thcrap_configure_v3`：新版配置向导（C# WPF）。
- `thcrap_tsa`：面向上海爱丽丝 STG 引擎的插件。
- `thcrap_tasofro`：面向黄昏边境相关游戏的插件。
- `thcrap_update`：补丁更新与自更新相关模块。
- `thcrap_bgmmod`：BGM 相关辅助库。

## 编译环境（按当前分支）

建议使用 Visual Studio 2022，并安装以下组件：

1. `使用 C++ 的桌面开发`
2. `MSVC v141 - VS 2017 C++ x64/x86 生成工具`
3. `对 VS 2017 (v141) 工具的 C++ Windows XP 支持（v141_xp）`
4. （用于 `thcrap_configure_v3`）.NET 桌面开发相关组件

## 获取源码

```bash
git clone --recursive https://github.com/thpatch/thcrap.git
```

如果你使用的是本汉化分支，请替换为你的仓库地址。

## 编译方法

在仓库根目录（`thcrap.sln` 同级）执行：

### Win32 Release

```powershell
msbuild thcrap.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m
```

### x64 Release

```powershell
msbuild thcrap.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

编译产物默认位于：

- `bin/`
- `bin/bin/`

## 打包发行版（当前仓库做法）

本仓库常见做法是按 `dist/thcrap_release_YYYY-MM-DD.zip` 的目录结构打包发布。  
建议每次发布前：

1. 先完成 `Win32 + x64` 的 `Release` 编译。
2. 确认 `bin/bin` 下关键文件已更新（如 `thcrap_update.dll`、`thcrap_update_64.dll`、`thcrap_configure_v3.exe`）。
3. 生成发行目录和 zip 放入 `dist/`。

## 自动更新行为说明（汉化版）

当前汉化分支已关闭“自动检查并升级到新版本”的行为，避免运行时自动切回其他版本。  
若你希望恢复官方逻辑，可自行回滚对应改动。

## 证书签名（可选）

如果你需要可验证签名的自动更新流程，需要自备证书并按官方流程签名。  
本分支默认可在无 `cert.pfx` 情况下编译，但不会进行数字签名。

## 许可证

上游 thcrap 及其附属模块默认采用 Public Domain（除非特定文件另有声明）。

## Backers

支持上游项目可访问：  
<https://opencollective.com/thpatch>

<a href="https://opencollective.com/thpatch#backers" target="_blank"><img src="https://opencollective.com/thpatch/backers.svg?width=890&button=true"></a>

## Contributors

<a href="https://github.com/thpatch/thcrap/graphs/contributors"><img src="https://opencollective.com/thpatch/contributors.svg?width=890&limit=5&button=false" /></a>
