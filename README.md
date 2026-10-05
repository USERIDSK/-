markdown
# 音视频批量转换工具 (Media Converter)

一个基于 FFmpeg 的轻量级音视频批量转换工具，支持格式互转、音频提取、视频画质调整、音频倍速调整等功能。
提供 **C++ (Win32 API)** 和 **C# (WinForms)** 两个版本源码，无需安装庞大的 IDE 即可编译。

## ✨ 功能特性
- 支持批量添加文件或文件夹（自动扫描子目录）。
- 支持按格式过滤文件并一键全选。
- 支持音视频格式互转（MP4、MKV、AVI、MP3、WAV、AAC、FLAC）。
- 支持提取视频中的音频（例如 MP4 转 MP3）。
- 支持调整视频画质（CRF）、分辨率、帧率。
- 支持调整音频码率、采样率、声道。
- 支持视频滤镜（亮度、对比度、饱和度、锐化）。
- 支持音频倍速（变速不变调）和音量调整。
- 自动处理重名文件，不会覆盖已有文件。

## 📦 依赖环境
本程序**不包含** FFmpeg 核心组件，请在运行前自行准备：

1. 前往 [FFmpeg 官网](https://ffmpeg.org/download.html) 或 [BtbN 的 GitHub 构建页](https://github.com/BtbN/FFmpeg-Builds/releases) 下载 `ffmpeg.exe`。
2. **方式一（推荐）**：将本程序放在ffmpeg.exe所在的同一个文件夹下。
3. **方式二**：将 `ffmpeg.exe` 所在目录添加到系统环境变量 `PATH` 中。

## 🚀 如何使用
1. 下载右侧 **Releases** 中最新的 `MediaConverter.exe`。
2. 将 `ffmpeg.exe` 放到同一个文件夹。
3. 双击运行 `MediaConverter.exe`。
4. 点击“添加文件”或“添加文件夹”，选择你要转换的文件。
5. 选择输出目录和输出格式，点击“开始转换”。

## 🔧 如何自行编译（开发者）
- **C# 版本**：
  使用 Windows 自带 `csc.exe` 编译：
  ```cmd
  csc /target:winexe /out:MediaConverter.exe cs.cs /reference:System.Windows.Forms.dll,System.Drawing.dll
C++ 版本：
使用 MinGW (g++) 编译：

cmd
g++ main.cpp -o MediaConverter.exe -static -mwindows -lcomctl32 -lole32
