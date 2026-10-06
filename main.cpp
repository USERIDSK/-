// ============================================================
// 音视频批量转换工具 (Win32 / g++ MinGW 兼容版)
// 支持"仅换容器"模式（自动探测源编码 + 容器兼容性判断）
// ============================================================
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <commctrl.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <set>
#include <functional>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <process.h>

// ================= 自定义消息 =================
#define WM_APP_PROGRESS (WM_APP + 1)
#define WM_APP_FINISHED (WM_APP + 2)

// ================= 全局控件 =================
static HWND hBtnSelectSrc, hBtnSelectFolder, hBtnRemoveSelected, hBtnClearList;
static HWND hComboFilterFormat, hBtnFilter;
static HWND hListFiles;
static HWND hEditDst, hBtnSelectDst, hBtnOpenDst;
static HWND hComboFormat, hComboPreset, hComboCRF, hComboAudioBitrate;
static HWND hComboRes, hComboFPS, hComboAudioRate, hComboChannels;
static HWND hEditVolume;
static HWND hEditBrightness, hEditContrast, hEditSaturation, hEditSharpen, hEditSpeed;
static HWND hEditStart, hEditDuration;
static HWND hBtnConvert, hStatus;

// 移动标签
struct MovableLabel { HWND hwnd; int x, y, w, h; };
static std::vector<MovableLabel> g_movableLabels;

static HWND MakeMovableLabel(HWND parent, const wchar_t* text,
                             int x, int y, int w, int h) {
    HWND hw = CreateWindowW(L"Static", text, WS_CHILD | WS_VISIBLE,
                            x, y, w, h, parent, NULL, NULL, NULL);
    MovableLabel ml = { hw, x, y, w, h };
    g_movableLabels.push_back(ml);
    return hw;
}

// ================= 全局状态 =================
static std::vector<std::wstring> g_fileList;
static std::atomic<bool> g_isConverting(false);
static std::wstring g_lastDstDir;

// ================= 数据结构 =================
struct ConversionOptions {
    std::wstring inputPath;
    std::wstring outputPath;
    std::wstring targetExt;
    bool sourceHasAudio = true;
    bool isAudioOutput  = false;
    bool isStreamCopy   = false;   // 仅换容器

    std::wstring startTime;
    std::wstring duration;

    std::wstring preset = L"medium";
    int          crf    = 23;
    std::wstring resolution;
    int          fps    = 0;
    double brightness = 0;
    double contrast   = 1;
    double saturation = 1;
    double sharpen    = 0;

    std::wstring audioBitrate;
    int    audioSampleRate = 0;
    int    audioChannels   = 0;
    double volume          = 1;

    double speed = 1;
};

struct RunResult {
    bool         success  = false;
    DWORD        exitCode = 0;
    std::wstring stderrText;
};

struct ConvertResult {
    bool         success = false;
    std::wstring error;
};

struct FinishedPayload {
    int success = 0;
    int failed  = 0;
    int skipped = 0;
    int total   = 0;
    std::wstring dstDir;
    std::wstring targetExt;
    std::vector<std::wstring> errors;
};

// 媒体信息（用于 stream copy 判断）
struct MediaInfo {
    bool hasVideo = false;
    bool hasAudio = false;
    std::wstring videoCodec;
    std::wstring audioCodec;
    bool valid = false;   // 探测是否成功
};

// ============================================================
// 工具函数
// ============================================================
static std::wstring Num(double v) {
    wchar_t buf[64];
    swprintf(buf, 64, L"%.4g", v);
    return std::wstring(buf);
}

static std::wstring QuoteArg(const std::wstring& s) {
    std::wstring out = L"\"";
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'"') out += L"\\\"";
        else              out += s[i];
    }
    out += L"\"";
    return out;
}

static std::wstring Join(const std::vector<std::wstring>& v, const std::wstring& sep) {
    std::wstring r;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) r += sep;
        r += v[i];
    }
    return r;
}

static std::wstring ToLower(const std::wstring& s) {
    std::wstring r = s;
    for (size_t i = 0; i < r.size(); ++i)
        r[i] = (wchar_t)towlower(r[i]);
    return r;
}

static int WcsICmp(const wchar_t* a, const wchar_t* b) {
    while (*a && *b) {
        wchar_t ca = (wchar_t)towlower(*a);
        wchar_t cb = (wchar_t)towlower(*b);
        if (ca != cb) return (int)ca - (int)cb;
        ++a; ++b;
    }
    return (int)(wchar_t)towlower(*a) - (int)(wchar_t)towlower(*b);
}

static std::vector<std::wstring> BuildAtempoChain(double speed) {
    std::vector<std::wstring> chain;
    double remaining = speed;
    while (remaining > 2.0) { chain.push_back(L"atempo=2.0"); remaining /= 2.0; }
    while (remaining < 0.5) { chain.push_back(L"atempo=0.5"); remaining /= 0.5; }
    chain.push_back(L"atempo=" + Num(remaining));
    return chain;
}

static double ParseDouble(const std::wstring& s, double fallback) {
    if (s.empty()) return fallback;
    wchar_t* end = NULL;
    double v = wcstod(s.c_str(), &end);
    if (end == s.c_str()) return fallback;
    return v;
}

static bool FileExists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static bool IsVideoExtension(const std::wstring& ext) {
    static const wchar_t* arr[] = { L".mp4", L".avi", L".mkv", L".mov", L".webm" };
    for (size_t i = 0; i < 5; ++i)
        if (ext == arr[i]) return true;
    return false;
}

static std::wstring GetFileExtension(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return L"";
    size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos && dot < slash) return L"";
    return ToLower(path.substr(dot));
}

static std::wstring GetBaseName(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    std::wstring name = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
    size_t dot = name.find_last_of(L'.');
    return (dot == std::wstring::npos) ? name : name.substr(0, dot);
}

// ============================================================
// 日志
// ============================================================
static void WriteLog(const std::wstring& text) {
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring p(exePath);
    size_t slash = p.find_last_of(L"\\/");
    std::wstring logPath = (slash == std::wstring::npos)
                         ? L"log.txt"
                         : p.substr(0, slash + 1) + L"log.txt";

    HANDLE h = CreateFileW(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;

    int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(),
                                  NULL, 0, NULL, NULL);
    if (len > 0) {
        std::string utf8(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(),
                            &utf8[0], len, NULL, NULL);
        DWORD written = 0;
        WriteFile(h, utf8.c_str(), (DWORD)utf8.size(), &written, NULL);
    }
    CloseHandle(h);
}

// ============================================================
// FFmpeg 定位
// ============================================================
static std::wstring GetFFmpegPath() {
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring ws(exePath);
    size_t slash = ws.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        std::wstring local = ws.substr(0, slash + 1) + L"ffmpeg.exe";
        if (FileExists(local)) return local;
    }
    wchar_t sysPath[MAX_PATH] = { 0 };
    if (SearchPathW(NULL, L"ffmpeg.exe", NULL, MAX_PATH, sysPath, NULL) > 0)
        return std::wstring(sysPath);
    return L"ffmpeg.exe";
}

// ============================================================
// 执行 ffmpeg
// ============================================================
static RunResult RunFfmpeg(const std::wstring& ffmpegPath, const std::wstring& args) {
    RunResult result;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead = NULL, hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        result.stderrText = L"[错误] CreatePipe 失败";
        return result;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput  = hWrite;
    si.hStdError   = hWrite;

    PROCESS_INFORMATION pi = { 0 };
    std::wstring cmdLine = QuoteArg(ffmpegPath) + L" " + args;

    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(
        NULL, &cmdBuf[0], NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

    CloseHandle(hWrite);

    if (!ok) {
        DWORD err = GetLastError();
        wchar_t buf[1024];
        swprintf(buf, 1024,
            L"[错误] CreateProcessW 失败  GetLastError=%lu\n"
            L"  ffmpeg 路径: %ls\n"
            L"  完整命令行: %ls",
            (unsigned long)err, ffmpegPath.c_str(), cmdLine.c_str());
        result.stderrText = buf;
        CloseHandle(hRead);
        return result;
    }

    std::string buffer;
    char temp[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hRead, temp, sizeof(temp), &bytesRead, NULL) && bytesRead > 0) {
        buffer.append(temp, bytesRead);
    }
    CloseHandle(hRead);

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &result.exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (!buffer.empty()) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), (int)buffer.size(), NULL, 0);
        if (wlen > 0) {
            result.stderrText.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, buffer.data(), (int)buffer.size(),
                                &result.stderrText[0], wlen);
        }
    } else {
        result.stderrText = L"[提示] 无 stderr 输出，exitCode="
                          + std::to_wstring(result.exitCode);
    }

    result.success = (result.exitCode == 0);
    return result;
}

// ============================================================
// 媒体探测 + 容器兼容性
// ============================================================
static MediaInfo ProbeMediaInfo(const std::wstring& ffmpegPath,
                                 const std::wstring& inputPath) {
    MediaInfo info;
    RunResult r = RunFfmpeg(ffmpegPath, L"-hide_banner -i " + QuoteArg(inputPath));
    const std::wstring& s = r.stderrText;

    // 解析 "Video: h264 (High) (avc1 / 0x...), ..."
    size_t vpos = s.find(L"Video: ");
    if (vpos != std::wstring::npos) {
        size_t start = vpos + 7;
        size_t end = s.find_first_of(L" ,(", start);
        if (end != std::wstring::npos) {
            info.videoCodec = ToLower(s.substr(start, end - start));
            info.hasVideo = !info.videoCodec.empty();
        }
    }

    // 解析 "Audio: aac (LC) (mp4a / 0x...), ..."
    size_t apos = s.find(L"Audio: ");
    if (apos != std::wstring::npos) {
        size_t start = apos + 7;
        size_t end = s.find_first_of(L" ,(", start);
        if (end != std::wstring::npos) {
            info.audioCodec = ToLower(s.substr(start, end - start));
            info.hasAudio = !info.audioCodec.empty();
        }
    }

    info.valid = info.hasVideo || info.hasAudio;
    return info;
}

// 目标容器支持的视频编码
static bool ContainerSupportsVideoCodec(const std::wstring& container,
                                         const std::wstring& codec) {
    if (container == L".mkv") {
        return codec == L"h264" || codec == L"hevc" || codec == L"h265" ||
               codec == L"mpeg4" || codec == L"vp8" || codec == L"vp9" ||
               codec == L"av1" || codec == L"mpeg2video" || codec == L"vc1";
    }
    if (container == L".mp4") {
        return codec == L"h264" || codec == L"hevc" || codec == L"h265" ||
               codec == L"mpeg4" || codec == L"av1" || codec == L"vp9";
    }
    if (container == L".avi") {
        return codec == L"h264" || codec == L"mpeg4" || codec == L"mjpeg";
    }
    if (container == L".mov") {
        return codec == L"h264" || codec == L"hevc" || codec == L"prores";
    }
    if (container == L".webm") {
        return codec == L"vp8" || codec == L"vp9" || codec == L"av1";
    }
    return false;
}

static bool ContainerSupportsAudioCodec(const std::wstring& container,
                                         const std::wstring& codec) {
    if (container == L".mkv") {
        return codec == L"aac" || codec == L"mp3" || codec == L"opus" ||
               codec == L"vorbis" || codec == L"flac" || codec == L"ac3" ||
               codec == L"dts" || codec == L"eac3" || codec == L"pcm_s16le";
    }
    if (container == L".mp4") {
        return codec == L"aac" || codec == L"mp3" || codec == L"ac3" ||
               codec == L"eac3" || codec == L"alac";
    }
    if (container == L".avi") {
        return codec == L"mp3" || codec == L"ac3" || codec == L"pcm_s16le";
    }
    if (container == L".mov") {
        return codec == L"aac" || codec == L"pcm_s16le" || codec == L"pcm_s24le";
    }
    if (container == L".webm") {
        return codec == L"vorbis" || codec == L"opus";
    }
    return false;
}

// 判断能不能把 info 里的流直接 copy 进目标容器
static bool CanStreamCopy(const MediaInfo& info, const std::wstring& dstExt) {
    if (!info.valid) return false;

    if (info.hasVideo) {
        if (!IsVideoExtension(dstExt)) return false;   // 视频 → 音频需要重编码
        if (!ContainerSupportsVideoCodec(dstExt, info.videoCodec)) return false;
        if (info.hasAudio && !ContainerSupportsAudioCodec(dstExt, info.audioCodec))
            return false;
        return true;
    }

    // 纯音频源：音频 → 音频容器一般都要重编码
    // （裸 aac → m4a 这种可以，但用户场景少见，简化处理）
    return false;
}

static bool HasAudioStream(const std::wstring& ffmpegPath, const std::wstring& inputPath) {
    MediaInfo info = ProbeMediaInfo(ffmpegPath, inputPath);
    return info.hasAudio;
}

// ============================================================
// 参数构建
// ============================================================
static std::wstring BuildFfmpegArgs(const ConversionOptions& o) {
    std::wstring args;
    args += L"-hide_banner -i " + QuoteArg(o.inputPath) + L" ";

    if (!o.startTime.empty()) args += L"-ss " + QuoteArg(o.startTime) + L" ";
    if (!o.duration.empty())  args += L"-t "  + QuoteArg(o.duration)  + L" ";

    // ---------- 仅换容器 ----------
    if (o.isStreamCopy) {
        // 复制全部视频+音频流，不重新编码
        args += L"-map 0:v? -map 0:a? -c copy ";
        if (o.targetExt == L".mp4") args += L"-movflags +faststart ";
        args += QuoteArg(o.outputPath) + L" -y";
        return args;
    }

    bool applySpeed = (o.speed >= 0.5 && o.speed <= 4.0 &&
                       fabs(o.speed - 1.0) > 0.001);

    // ---------- 纯音频输出 ----------
    if (o.isAudioOutput) {
        args += L"-vn ";
        if      (o.targetExt == L".mp3")  args += L"-c:a libmp3lame ";
        else if (o.targetExt == L".wav")  args += L"-c:a pcm_s16le ";
        else if (o.targetExt == L".aac")  args += L"-c:a aac ";
        else if (o.targetExt == L".flac") args += L"-c:a flac ";

        std::vector<std::wstring> af;
        if (fabs(o.volume - 1.0) > 0.001)
            af.push_back(L"volume=" + Num(o.volume));
        if (applySpeed) {
            std::vector<std::wstring> chain = BuildAtempoChain(o.speed);
            af.insert(af.end(), chain.begin(), chain.end());
        }
        if (!af.empty()) args += L"-af " + QuoteArg(Join(af, L",")) + L" ";

        if (!o.audioBitrate.empty()) args += L"-b:a " + o.audioBitrate + L" ";
        if (o.audioSampleRate > 0)   args += L"-ar " + std::to_wstring(o.audioSampleRate) + L" ";
        if (o.audioChannels > 0)     args += L"-ac " + std::to_wstring(o.audioChannels)   + L" ";

        args += QuoteArg(o.outputPath) + L" -y";
        return args;
    }

    // ---------- 视频输出（重新编码） ----------
    args += L"-c:v libx264 ";
    args += L"-preset " + o.preset + L" ";
    args += L"-crf " + std::to_wstring(o.crf) + L" ";
    args += L"-pix_fmt yuv420p ";
    if (!o.resolution.empty()) args += L"-s " + o.resolution + L" ";
    if (o.fps > 0)             args += L"-r " + std::to_wstring(o.fps) + L" ";

    std::vector<std::wstring> vf;
    if (fabs(o.brightness) > 0.001 || fabs(o.contrast - 1) > 0.001 ||
        fabs(o.saturation - 1) > 0.001)
    {
        std::vector<std::wstring> eq;
        if (fabs(o.brightness) > 0.001)     eq.push_back(L"brightness=" + Num(o.brightness));
        if (fabs(o.contrast - 1) > 0.001)   eq.push_back(L"contrast="   + Num(o.contrast));
        if (fabs(o.saturation - 1) > 0.001) eq.push_back(L"saturation=" + Num(o.saturation));
        vf.push_back(L"eq=" + Join(eq, L":"));
    }
    if (o.sharpen > 0.001)
        vf.push_back(L"unsharp=5:5:" + Num(o.sharpen));

    std::vector<std::wstring> af;
    if (o.sourceHasAudio && fabs(o.volume - 1.0) > 0.001)
        af.push_back(L"volume=" + Num(o.volume));

    if (applySpeed) {
        std::vector<std::wstring> parts;

        std::vector<std::wstring> vfCopy = vf;
        vfCopy.push_back(L"setpts=PTS/" + Num(o.speed));
        parts.push_back(L"[0:v]" + Join(vfCopy, L",") + L"[v]");

        if (o.sourceHasAudio) {
            std::vector<std::wstring> afCopy = af;
            std::vector<std::wstring> chain  = BuildAtempoChain(o.speed);
            afCopy.insert(afCopy.end(), chain.begin(), chain.end());
            parts.push_back(L"[0:a]" + Join(afCopy, L",") + L"[a]");
            args += L"-filter_complex " + QuoteArg(Join(parts, L";"))
                  + L" -map \"[v]\" -map \"[a]\" ";
        } else {
            args += L"-filter_complex " + QuoteArg(Join(parts, L";"))
                  + L" -map \"[v]\" ";
        }
    } else {
        if (!vf.empty()) args += L"-vf " + QuoteArg(Join(vf, L",")) + L" ";
        if (!af.empty()) args += L"-af " + QuoteArg(Join(af, L",")) + L" ";
    }

    if (o.sourceHasAudio) {
        if (o.targetExt == L".avi") args += L"-c:a libmp3lame ";
        else                        args += L"-c:a aac ";
        if (!o.audioBitrate.empty()) args += L"-b:a " + o.audioBitrate + L" ";
        if (o.audioSampleRate > 0)   args += L"-ar " + std::to_wstring(o.audioSampleRate) + L" ";
        if (o.audioChannels > 0)     args += L"-ac " + std::to_wstring(o.audioChannels)   + L" ";
    } else {
        args += L"-an ";
    }

    if (o.targetExt == L".mp4") args += L"-movflags +faststart ";
    args += QuoteArg(o.outputPath) + L" -y";
    return args;
}

// ============================================================
// 转换单个文件
// ============================================================
static ConvertResult ConvertOneFile(const std::wstring& ffmpegPath,
                                    ConversionOptions& opt) {
    ConvertResult result;

    int counter = 1;
    std::wstring base = opt.outputPath;
    size_t dot = base.find_last_of(L'.');
    std::wstring stem = (dot == std::wstring::npos) ? base : base.substr(0, dot);
    std::wstring ext  = (dot == std::wstring::npos) ? L""  : base.substr(dot);
    std::wstring candidate = opt.outputPath;
    while (FileExists(candidate)) {
        candidate = stem + L"_" + std::to_wstring(counter) + ext;
        counter++;
    }
    opt.outputPath = candidate;

    std::wstring args = BuildFfmpegArgs(opt);

    WriteLog(L"\n============================================================\n"
             L"输入: " + opt.inputPath + L"\n"
             L"输出: " + opt.outputPath + L"\n"
             L"模式: " + (opt.isStreamCopy ? L"仅换容器 (stream copy)" : L"重新编码") + L"\n"
             L"参数: " + args + L"\n");

    RunResult r = RunFfmpeg(ffmpegPath, args);

    WriteLog(L"退出码: " + std::to_wstring((int)r.exitCode) + L"\n"
             L"stderr:\n" + r.stderrText + L"\n");

    result.success = r.success;
    if (!r.success) {
        std::wstring err = r.stderrText;
        if (err.size() > 1500) err = err.substr(err.size() - 1500);
        result.error = err;
    }
    return result;
}

// ============================================================
// 列表框 / 下拉框
// ============================================================
static void RefreshListBox() {
    SendMessageW(hListFiles, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < g_fileList.size(); ++i)
        SendMessageW(hListFiles, LB_ADDSTRING, 0, (LPARAM)g_fileList[i].c_str());

    std::set<std::wstring> exts;
    for (size_t i = 0; i < g_fileList.size(); ++i) {
        std::wstring ext = GetFileExtension(g_fileList[i]);
        if (!ext.empty()) exts.insert(ext);
    }

    int curSel = (int)SendMessageW(hComboFilterFormat, CB_GETCURSEL, 0, 0);
    wchar_t curText[50] = { 0 };
    if (curSel >= 0)
        SendMessageW(hComboFilterFormat, CB_GETLBTEXT, curSel, (LPARAM)curText);

    SendMessageW(hComboFilterFormat, CB_RESETCONTENT, 0, 0);
    if (exts.empty()) {
        EnableWindow(hComboFilterFormat, FALSE);
    } else {
        EnableWindow(hComboFilterFormat, TRUE);
        int foundIdx = -1, idx = 0;
        for (std::set<std::wstring>::iterator it = exts.begin(); it != exts.end(); ++it) {
            SendMessageW(hComboFilterFormat, CB_ADDSTRING, 0, (LPARAM)it->c_str());
            if (WcsICmp(it->c_str(), curText) == 0) foundIdx = idx;
            idx++;
        }
        SendMessageW(hComboFilterFormat, CB_SETCURSEL, foundIdx >= 0 ? foundIdx : 0, 0);
    }
}

static void UpdateControlStates(HWND hwnd) {
    (void)hwnd;
    int fmtIdx = (int)SendMessageW(hComboFormat, CB_GETCURSEL, 0, 0);
    if (fmtIdx < 0) return;
    wchar_t buf[50] = { 0 };
    SendMessageW(hComboFormat, CB_GETLBTEXT, fmtIdx, (LPARAM)buf);
    std::wstring fmtStr(buf);
    bool isAudio = (fmtStr.find(L"音频") != std::wstring::npos);
    bool isCopy  = (fmtStr.find(L"仅换容器") != std::wstring::npos);

    // 仅换容器模式下：所有转码参数禁用
    bool videoEnabled = !isAudio && !isCopy;
    bool audioEnabled = !isCopy;

    EnableWindow(hComboPreset,     videoEnabled);
    EnableWindow(hComboCRF,        videoEnabled);
    EnableWindow(hComboRes,        videoEnabled);
    EnableWindow(hComboFPS,        videoEnabled);
    EnableWindow(hEditBrightness,  videoEnabled);
    EnableWindow(hEditContrast,    videoEnabled);
    EnableWindow(hEditSaturation,  videoEnabled);
    EnableWindow(hEditSharpen,     videoEnabled);

    EnableWindow(hComboAudioBitrate, audioEnabled);
    EnableWindow(hComboAudioRate,    audioEnabled);
    EnableWindow(hComboChannels,     audioEnabled);
    EnableWindow(hEditVolume,        audioEnabled);
    EnableWindow(hEditSpeed,         audioEnabled);

    // 时间裁剪在 stream copy 下仍可用
}

static void UpdateFormatCombo(HWND hwnd) {
    int selCount = (int)SendMessageW(hListFiles, LB_GETSELCOUNT, 0, 0);
    if (selCount == 0) {
        SendMessageW(hComboFormat, CB_RESETCONTENT, 0, 0);
        EnableWindow(hComboFormat, FALSE);
        return;
    }
    EnableWindow(hComboFormat, TRUE);

    int selIndex = -1;
    SendMessageW(hListFiles, LB_GETSELITEMS, 1, (LPARAM)&selIndex);
    if (selIndex < 0) return;

    wchar_t buf[260] = { 0 };
    SendMessageW(hListFiles, LB_GETTEXT, selIndex, (LPARAM)buf);
    std::wstring srcPath(buf);
    std::wstring ext = GetFileExtension(srcPath);
    bool isVideo = IsVideoExtension(ext);

    // 探测源编码（可能耗时 100-300ms，只在选中文件时触发）
    MediaInfo info = ProbeMediaInfo(GetFFmpegPath(), srcPath);

    SendMessageW(hComboFormat, CB_RESETCONTENT, 0, 0);

    if (isVideo) {
        // -------- 视频目标：MP4 / MKV / AVI --------
        struct VT { const wchar_t* label; const wchar_t* ext; };
        static const VT vts[] = {
            { L"MP4 (视频)", L".mp4" },
            { L"MKV (视频)", L".mkv" },
            { L"AVI (视频)", L".avi" },
        };
        for (size_t i = 0; i < 3; ++i) {
            if (ext == vts[i].ext) continue;   // 跳过源格式
            SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)vts[i].label);
            // 能 stream copy 才加“仅换容器”
            if (CanStreamCopy(info, vts[i].ext)) {
                std::wstring copyLabel = std::wstring(vts[i].label)
                                       + L" - 仅换容器";
                // 用固定前缀 "MP4 - 仅换容器" 这种格式，解析时按前缀匹配
                wchar_t copyBuf[64];
                const wchar_t* code = (vts[i].ext == L".mp4") ? L"MP4" :
                                      (vts[i].ext == L".mkv") ? L"MKV" : L"AVI";
                swprintf(copyBuf, 64, L"%ls (仅换容器)", code);
                SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)copyBuf);
            }
        }

        // -------- 音频目标（视频 → 音频一律重编码，不加仅换容器） --------
        if (ext != L".mp3")  SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"MP3 (音频)");
        if (ext != L".wav")  SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"WAV (音频)");
        if (ext != L".aac")  SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"AAC (音频)");
        if (ext != L".flac") SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"FLAC (音频)");
    } else {
        // -------- 音频源：只列音频目标 --------
        if (ext != L".mp3")  SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"MP3 (音频)");
        if (ext != L".wav")  SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"WAV (音频)");
        if (ext != L".aac")  SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"AAC (音频)");
        if (ext != L".flac") SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"FLAC (音频)");
    }

    if (SendMessageW(hComboFormat, CB_GETCOUNT, 0, 0) > 0)
        SendMessageW(hComboFormat, CB_SETCURSEL, 0, 0);

    UpdateControlStates(hwnd);
}

static void AddToQueue(const std::wstring& f) {
    for (size_t i = 0; i < g_fileList.size(); ++i)
        if (WcsICmp(g_fileList[i].c_str(), f.c_str()) == 0) return;
    g_fileList.push_back(f);
}

static void AutoSelectFirst(HWND hwnd) {
    if (g_fileList.empty()) return;
    SendMessageW(hListFiles, LB_SETSEL, FALSE, -1);
    SendMessageW(hListFiles, LB_SETSEL, TRUE, 0);
    SendMessageW(hListFiles, LB_SETCARETINDEX, 0, FALSE);
    UpdateFormatCombo(hwnd);
}

// ============================================================
// 递归扫描文件夹
// ============================================================
static void ScanFolder(const std::wstring& folder, std::vector<std::wstring>& out) {
    WIN32_FIND_DATAW fd;
    std::wstring searchPath = L"\\\\?\\" + folder + L"\\*.*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
                ScanFolder(folder + L"\\" + fd.cFileName, out);
        } else {
            std::wstring ext = GetFileExtension(fd.cFileName);
            if (IsVideoExtension(ext) ||
                ext == L".mp3" || ext == L".wav" || ext == L".aac" ||
                ext == L".flac" || ext == L".m4a")
            {
                out.push_back(folder + L"\\" + fd.cFileName);
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

// ============================================================
// 后台线程
// ============================================================
struct ThreadData {
    HWND hwnd;
    std::vector<std::wstring> files;
    std::wstring dstDir;
    std::wstring ffmpegPath;
    ConversionOptions tmpl;
    int total;
};

static unsigned __stdcall ConversionThreadProc(void* param) {
    ThreadData* data = (ThreadData*)param;
    HWND hwnd          = data->hwnd;
    std::wstring dst   = data->dstDir;
    std::wstring fmp   = data->ffmpegPath;
    ConversionOptions tmpl = data->tmpl;
    int total          = data->total;

    WriteLog(L"\n\n############ 开始批量转换 ############\n"
             L"目标目录: " + dst + L"\n"
             L"ffmpeg: " + fmp + L"\n"
             L"模式: " + (tmpl.isStreamCopy ? L"仅换容器" : L"重新编码") + L"\n"
             L"文件数: " + std::to_wstring(total) + L"\n");

    FinishedPayload* payload = new FinishedPayload();
    payload->total     = total;
    payload->dstDir    = dst;
    payload->targetExt = tmpl.targetExt;

    for (size_t i = 0; i < data->files.size(); ++i) {
        const std::wstring& srcPath = data->files[i];
        std::wstring srcExt = GetFileExtension(srcPath);
        bool isSrcVideo = IsVideoExtension(srcExt);

        if (!isSrcVideo && !tmpl.isAudioOutput) {
            payload->skipped++;
            WriteLog(L"[跳过] 音频源无法转视频: " + srcPath + L"\n");
            continue;
        }

        std::wstring* msg = new std::wstring(
            L"状态：正在转换 (" + std::to_wstring(i + 1) + L"/"
            + std::to_wstring(data->files.size()) + L")...");
        PostMessageW(hwnd, WM_APP_PROGRESS, 0, (LPARAM)msg);

        ConversionOptions opt = tmpl;
        opt.inputPath  = srcPath;
        opt.outputPath = dst + L"\\" + GetBaseName(srcPath) + opt.targetExt;

        // stream copy 模式不需要探测音轨，交给 ffmpeg 的 -map 0:v? -map 0:a?
        if (!opt.isStreamCopy) {
            opt.sourceHasAudio = !isSrcVideo || HasAudioStream(fmp, srcPath);
        }

        ConvertResult r = ConvertOneFile(fmp, opt);
        if (r.success) {
            payload->success++;
        } else {
            payload->failed++;
            std::wstring name = srcPath.substr(srcPath.find_last_of(L"\\/") + 1);
            std::wstring err = r.error;
            if (err.size() > 400) err = err.substr(err.size() - 400);
            payload->errors.push_back(name + L": " + err);
        }
    }

    PostMessageW(hwnd, WM_APP_FINISHED, 0, (LPARAM)payload);

    delete data;
    return 0;
}

// ============================================================
// 启动转换
// ============================================================
static void StartConversion(HWND hwnd) {
    if (g_isConverting.load()) {
        MessageBoxW(hwnd, L"正在转换中，请稍候...", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (g_fileList.empty()) {
        MessageBoxW(hwnd, L"待转换队列为空！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    wchar_t dstDir[260] = { 0 };
    GetWindowTextW(hEditDst, dstDir, 260);
    if (wcslen(dstDir) == 0) {
        MessageBoxW(hwnd, L"请选择输出目录！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring ffmpegPath = GetFFmpegPath();
    if (!FileExists(ffmpegPath)) {
        std::wstring msg = L"未找到 ffmpeg.exe！\n\n"
                          L"已尝试查找：\n"
                          L"  1) 程序同目录\n"
                          L"  2) 系统 PATH\n\n"
                          L"请把 ffmpeg.exe 放到程序目录下再试。\n"
                          L"当前查找路径：\n"
                          + ffmpegPath;
        MessageBoxW(hwnd, msg.c_str(), L"缺少 ffmpeg", MB_OK | MB_ICONERROR);
        return;
    }

    int fmtIdx = (int)SendMessageW(hComboFormat, CB_GETCURSEL, 0, 0);
    std::wstring fmtStr;
    if (fmtIdx < 0) {
        fmtStr = L"MP4 (视频)";
        SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"MP4 (视频)");
        SendMessageW(hComboFormat, CB_SETCURSEL, 0, 0);
    } else {
        wchar_t fmtBuf[64] = { 0 };
        SendMessageW(hComboFormat, CB_GETLBTEXT, fmtIdx, (LPARAM)fmtBuf);
        fmtStr = fmtBuf;
    }

    bool isStreamCopy = (fmtStr.find(L"仅换容器") != std::wstring::npos);

    std::wstring targetExt = L".mp4";
    if      (fmtStr.find(L"MP4")  != std::wstring::npos) targetExt = L".mp4";
    else if (fmtStr.find(L"MKV")  != std::wstring::npos) targetExt = L".mkv";
    else if (fmtStr.find(L"AVI")  != std::wstring::npos) targetExt = L".avi";
    else if (fmtStr.find(L"MP3")  != std::wstring::npos) targetExt = L".mp3";
    else if (fmtStr.find(L"WAV")  != std::wstring::npos) targetExt = L".wav";
    else if (fmtStr.find(L"AAC")  != std::wstring::npos) targetExt = L".aac";
    else if (fmtStr.find(L"FLAC") != std::wstring::npos) targetExt = L".flac";

    static const wchar_t* PRESETS[] = { L"medium", L"fast", L"slow" };
    static const int       CRFS[]    = { 18, 23, 28 };

    int presetIdx       = (int)SendMessageW(hComboPreset,       CB_GETCURSEL, 0, 0);
    int crfIdx          = (int)SendMessageW(hComboCRF,          CB_GETCURSEL, 0, 0);
    int audioBitrateIdx = (int)SendMessageW(hComboAudioBitrate, CB_GETCURSEL, 0, 0);
    int fpsIdx          = (int)SendMessageW(hComboFPS,          CB_GETCURSEL, 0, 0);
    int resIdx          = (int)SendMessageW(hComboRes,          CB_GETCURSEL, 0, 0);
    int audioRateIdx    = (int)SendMessageW(hComboAudioRate,    CB_GETCURSEL, 0, 0);
    int channelsIdx     = (int)SendMessageW(hComboChannels,     CB_GETCURSEL, 0, 0);

    wchar_t startTime[50] = { 0 }, duration[50] = { 0 }, speed[20] = { 0 };
    wchar_t bright[20] = { 0 }, contrast[20] = { 0 }, sat[20] = { 0 };
    wchar_t sharpen[20] = { 0 }, volume[20] = { 0 };
    GetWindowTextW(hEditStart,      startTime, 50);
    GetWindowTextW(hEditDuration,   duration,  50);
    GetWindowTextW(hEditSpeed,      speed,     20);
    GetWindowTextW(hEditBrightness, bright,    20);
    GetWindowTextW(hEditContrast,   contrast,  20);
    GetWindowTextW(hEditSaturation, sat,       20);
    GetWindowTextW(hEditSharpen,    sharpen,   20);
    GetWindowTextW(hEditVolume,     volume,    20);

    ConversionOptions tmpl;
    tmpl.targetExt     = targetExt;
    tmpl.isStreamCopy  = isStreamCopy;
    tmpl.isAudioOutput = (targetExt == L".mp3" || targetExt == L".wav" ||
                          targetExt == L".aac" || targetExt == L".flac");
    tmpl.startTime     = startTime;
    tmpl.duration      = duration;

    tmpl.preset = PRESETS[(presetIdx < 0 || presetIdx > 2) ? 0 : presetIdx];
    tmpl.crf    = CRFS[(crfIdx < 0 || crfIdx > 2) ? 1 : crfIdx];

    if      (resIdx == 1) tmpl.resolution = L"1920x1080";
    else if (resIdx == 2) tmpl.resolution = L"1280x720";
    else if (resIdx == 3) tmpl.resolution = L"854x480";

    if      (fpsIdx == 1) tmpl.fps = 60;
    else if (fpsIdx == 2) tmpl.fps = 30;
    else if (fpsIdx == 3) tmpl.fps = 24;

    if      (audioBitrateIdx == 1) tmpl.audioBitrate = L"320k";
    else if (audioBitrateIdx == 2) tmpl.audioBitrate = L"192k";
    else if (audioBitrateIdx == 3) tmpl.audioBitrate = L"128k";

    if      (audioRateIdx == 1) tmpl.audioSampleRate = 48000;
    else if (audioRateIdx == 2) tmpl.audioSampleRate = 44100;

    if      (channelsIdx == 1) tmpl.audioChannels = 2;
    else if (channelsIdx == 2) tmpl.audioChannels = 1;

    tmpl.volume     = ParseDouble(volume,    1.0);
    tmpl.speed      = ParseDouble(speed,     1.0);
    tmpl.brightness = ParseDouble(bright,    0.0);
    tmpl.contrast   = ParseDouble(contrast,  1.0);
    tmpl.saturation = ParseDouble(sat,       1.0);
    tmpl.sharpen    = ParseDouble(sharpen,   0.0);

    g_lastDstDir = dstDir;

    ThreadData* data = new ThreadData();
    data->hwnd        = hwnd;
    data->files       = g_fileList;
    data->dstDir      = dstDir;
    data->ffmpegPath  = ffmpegPath;
    data->tmpl        = tmpl;
    data->total       = (int)g_fileList.size();

    g_isConverting.store(true);
    EnableWindow(hBtnConvert, FALSE);
    EnableWindow(hBtnOpenDst, FALSE);
    SetWindowTextW(hStatus,
        isStreamCopy ? L"状态：仅换容器转换中..." : L"状态：正在批量转换...");

    unsigned tid = 0;
    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL, 0, ConversionThreadProc, data, 0, &tid);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        delete data;
        g_isConverting.store(false);
        EnableWindow(hBtnConvert, TRUE);
        EnableWindow(hBtnOpenDst, TRUE);
        MessageBoxW(hwnd, L"无法启动后台线程！", L"错误", MB_OK | MB_ICONERROR);
    }
}

// ============================================================
// 窗口过程
// ============================================================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 820;
        mmi->ptMinTrackSize.y = 680;
        return 0;
    }

    case WM_CREATE: {
        std::wstring fmp = GetFFmpegPath();
        if (!FileExists(fmp)) {
            std::wstring msg = L"警告：未找到 ffmpeg.exe！\n\n"
                              L"程序会在下面两个位置查找：\n"
                              L"  1) 程序同目录\n"
                              L"  2) 系统 PATH\n\n"
                              L"请下载 ffmpeg（https://www.gyan.dev/ffmpeg/builds/）\n"
                              L"把 bin\\ffmpeg.exe 放到程序目录下。\n\n"
                              L"查找结果：" + fmp;
            MessageBoxW(hwnd, msg.c_str(), L"缺少核心组件",
                        MB_OK | MB_ICONWARNING);
        }

        int y = 15;

        // 第1行
        CreateWindowW(L"Static", L"待转换队列:", WS_CHILD | WS_VISIBLE,
                      20, y, 70, 20, hwnd, NULL, NULL, NULL);
        hBtnSelectSrc      = CreateWindowW(L"Button", L"添加文件",   WS_CHILD | WS_VISIBLE,  90, y, 80, 22, hwnd, (HMENU)2,  NULL, NULL);
        hBtnSelectFolder   = CreateWindowW(L"Button", L"添加文件夹", WS_CHILD | WS_VISIBLE, 180, y, 90, 22, hwnd, (HMENU)30, NULL, NULL);
        hBtnRemoveSelected = CreateWindowW(L"Button", L"移除选中",   WS_CHILD | WS_VISIBLE, 280, y, 80, 22, hwnd, (HMENU)41, NULL, NULL);
        hBtnClearList      = CreateWindowW(L"Button", L"清空列表",   WS_CHILD | WS_VISIBLE, 370, y, 80, 22, hwnd, (HMENU)40, NULL, NULL);

        CreateWindowW(L"Static", L"按格式:", WS_CHILD | WS_VISIBLE,
                      460, y, 50, 20, hwnd, NULL, NULL, NULL);
        hComboFilterFormat = CreateWindowW(L"ComboBox", L"",
                      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                      510, y, 90, 200, hwnd, (HMENU)50, NULL, NULL);
        hBtnFilter         = CreateWindowW(L"Button", L"过滤并全选",
                      WS_CHILD | WS_VISIBLE, 610, y, 90, 22, hwnd, (HMENU)51, NULL, NULL);
        EnableWindow(hComboFilterFormat, FALSE);
        y += 30;

        // 文件列表
        hListFiles = CreateWindowW(L"ListBox", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL
                      | LBS_EXTENDEDSEL | LBS_NOTIFY,
                      20, y, 760, 250, hwnd, (HMENU)33, NULL, NULL);
        y += 260;

        // 输出目录
        MakeMovableLabel(hwnd, L"输出目录:", 20, y, 60, 20);
        hEditDst      = CreateWindowW(L"Edit", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      80, y, 440, 22, hwnd, (HMENU)3, NULL, NULL);
        hBtnSelectDst = CreateWindowW(L"Button", L"选择输出目录",
                      WS_CHILD | WS_VISIBLE, 530, y, 110, 22, hwnd, (HMENU)4, NULL, NULL);
        hBtnOpenDst   = CreateWindowW(L"Button", L"打开输出目录",
                      WS_CHILD | WS_VISIBLE, 650, y, 110, 22, hwnd, (HMENU)60, NULL, NULL);
        y += 40;

        // 格式 / 预设 / CRF
        MakeMovableLabel(hwnd, L"输出格式:", 20, y, 60, 20);
        hComboFormat = CreateWindowW(L"ComboBox", L"",
                      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                      80, y, 110, 260, hwnd, (HMENU)5, NULL, NULL);

        MakeMovableLabel(hwnd, L"编码预设:", 200, y, 60, 20);
        hComboPreset = CreateWindowW(L"ComboBox", L"",
                      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                      260, y, 90, 200, hwnd, (HMENU)20, NULL, NULL);
        SendMessageW(hComboPreset, CB_ADDSTRING, 0, (LPARAM)L"medium(默认)");
        SendMessageW(hComboPreset, CB_ADDSTRING, 0, (LPARAM)L"fast(快速)");
        SendMessageW(hComboPreset, CB_ADDSTRING, 0, (LPARAM)L"slow(慢速)");
        SendMessageW(hComboPreset, CB_SETCURSEL, 0, 0);

        MakeMovableLabel(hwnd, L"画质(CRF):", 370, y, 60, 20);
        hComboCRF = CreateWindowW(L"ComboBox", L"",
                      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                      430, y, 90, 200, hwnd, (HMENU)21, NULL, NULL);
        SendMessageW(hComboCRF, CB_ADDSTRING, 0, (LPARAM)L"18(高画质)");
        SendMessageW(hComboCRF, CB_ADDSTRING, 0, (LPARAM)L"23(均衡)");
        SendMessageW(hComboCRF, CB_ADDSTRING, 0, (LPARAM)L"28(压缩优先)");
        SendMessageW(hComboCRF, CB_SETCURSEL, 1, 0);
        y += 30;

        // 分辨率 / 帧率 / 音频码率
        MakeMovableLabel(hwnd, L"分辨率:", 20, y, 60, 20);
        hComboRes = CreateWindowW(L"ComboBox", L"",
                      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                      80, y, 90, 200, hwnd, (HMENU)14, NULL, NULL);
        SendMessageW(hComboRes, CB_ADDSTRING, 0, (LPARAM)L"保持原样");
        SendMessageW(hComboRes, CB_ADDSTRING, 0, (LPARAM)L"1920x1080");
        SendMessageW(hComboRes, CB_ADDSTRING, 0, (LPARAM)L"1280x720");
        SendMessageW(hComboRes, CB_ADDSTRING, 0, (LPARAM)L"854x480");
        SendMessageW(hComboRes, CB_SETCURSEL, 0, 0);

        MakeMovableLabel(hwnd, L"帧率(FPS):", 190, y, 60, 20);
        hComboFPS = CreateWindowW(L"ComboBox", L"",
                      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                      250, y, 90, 200, hwnd, (HMENU)13, NULL, NULL);
        SendMessageW(hComboFPS, CB_ADDSTRING, 0, (LPARAM)L"保持原样");
        SendMessageW(hComboFPS, CB_ADDSTRING, 0, (LPARAM)L"60");
        SendMessageW(hComboFPS, CB_ADDSTRING, 0, (LPARAM)L"30");
        SendMessageW(hComboFPS, CB_ADDSTRING, 0, (LPARAM)L"24");
        SendMessageW(hComboFPS, CB_SETCURSEL, 0, 0);

        MakeMovableLabel(hwnd, L"音频码率:", 360, y, 60, 20);
        hComboAudioBitrate = CreateWindowW(L"ComboBox", L"",
                      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                      420, y, 90, 200, hwnd, (HMENU)6, NULL, NULL);
        SendMessageW(hComboAudioBitrate, CB_ADDSTRING, 0, (LPARAM)L"默认/自动");
        SendMessageW(hComboAudioBitrate, CB_ADDSTRING, 0, (LPARAM)L"320 kbps");
        SendMessageW(hComboAudioBitrate, CB_ADDSTRING, 0, (LPARAM)L"192 kbps");
        SendMessageW(hComboAudioBitrate, CB_ADDSTRING, 0, (LPARAM)L"128 kbps");
        SendMessageW(hComboAudioBitrate, CB_SETCURSEL, 0, 0);
        y += 30;

        // 采样率 / 声道 / 音量
        MakeMovableLabel(hwnd, L"采样率:", 20, y, 60, 20);
        hComboAudioRate = CreateWindowW(L"ComboBox", L"",
                      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                      80, y, 90, 200, hwnd, (HMENU)15, NULL, NULL);
        SendMessageW(hComboAudioRate, CB_ADDSTRING, 0, (LPARAM)L"保持原样");
        SendMessageW(hComboAudioRate, CB_ADDSTRING, 0, (LPARAM)L"48000 Hz");
        SendMessageW(hComboAudioRate, CB_ADDSTRING, 0, (LPARAM)L"44100 Hz");
        SendMessageW(hComboAudioRate, CB_SETCURSEL, 0, 0);

        MakeMovableLabel(hwnd, L"声道:", 190, y, 60, 20);
        hComboChannels = CreateWindowW(L"ComboBox", L"",
                      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                      250, y, 90, 200, hwnd, (HMENU)16, NULL, NULL);
        SendMessageW(hComboChannels, CB_ADDSTRING, 0, (LPARAM)L"保持原样");
        SendMessageW(hComboChannels, CB_ADDSTRING, 0, (LPARAM)L"立体声(2)");
        SendMessageW(hComboChannels, CB_ADDSTRING, 0, (LPARAM)L"单声道(1)");
        SendMessageW(hComboChannels, CB_SETCURSEL, 0, 0);

        MakeMovableLabel(hwnd, L"音量倍数:", 360, y, 60, 20);
        hEditVolume = CreateWindowW(L"Edit", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      420, y, 90, 22, hwnd, (HMENU)22, NULL, NULL);
        y += 40;

        // 颜色滤镜
        MakeMovableLabel(hwnd, L"亮度(-1~1):", 20, y, 70, 20);
        hEditBrightness = CreateWindowW(L"Edit", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      90, y, 50, 22, hwnd, (HMENU)17, NULL, NULL);

        MakeMovableLabel(hwnd, L"对比度(0~3):", 150, y, 70, 20);
        hEditContrast = CreateWindowW(L"Edit", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      220, y, 50, 22, hwnd, (HMENU)18, NULL, NULL);

        MakeMovableLabel(hwnd, L"饱和度(0~3):", 280, y, 70, 20);
        hEditSaturation = CreateWindowW(L"Edit", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      350, y, 50, 22, hwnd, (HMENU)19, NULL, NULL);

        MakeMovableLabel(hwnd, L"锐化(0~5):", 410, y, 60, 20);
        hEditSharpen = CreateWindowW(L"Edit", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      470, y, 50, 22, hwnd, (HMENU)23, NULL, NULL);

        MakeMovableLabel(hwnd, L"倍速:", 530, y, 40, 20);
        hEditSpeed = CreateWindowW(L"Edit", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      570, y, 50, 22, hwnd, (HMENU)7, NULL, NULL);
        y += 40;

        // 时间裁剪
        MakeMovableLabel(hwnd, L"起始时间:", 20, y, 60, 20);
        hEditStart = CreateWindowW(L"Edit", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      80, y, 100, 22, hwnd, (HMENU)8, NULL, NULL);

        MakeMovableLabel(hwnd, L"持续时长:", 190, y, 60, 20);
        hEditDuration = CreateWindowW(L"Edit", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      250, y, 100, 22, hwnd, (HMENU)9, NULL, NULL);
        y += 40;

        // 按钮 / 状态
        hBtnConvert = CreateWindowW(L"Button", L"开始转换",
                      WS_CHILD | WS_VISIBLE, 20, y, 100, 30, hwnd, (HMENU)10, NULL, NULL);
        hStatus     = CreateWindowW(L"Static", L"状态：就绪",
                      WS_CHILD | WS_VISIBLE, 140, y + 5, 640, 25, hwnd, (HMENU)11, NULL, NULL);

        EnableWindow(hComboFormat, FALSE);
        EnableWindow(hBtnOpenDst, FALSE);
        break;
    }

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        if (hListFiles == NULL) return 0;

        int listH = h - 520;
        if (listH < 150) listH = 150;
        MoveWindow(hListFiles, 20, 45, w - 40, listH, TRUE);

        int yOffset = listH - 250;

        MoveWindow(hEditDst,      80,        305 + yOffset, w - 320, 22, TRUE);
        MoveWindow(hBtnSelectDst, w - 230,   305 + yOffset, 110,     22, TRUE);
        MoveWindow(hBtnOpenDst,   w - 110,   305 + yOffset, 110,     22, TRUE);

        MoveWindow(hComboFormat,   80,  345 + yOffset, 110, 260, TRUE);
        MoveWindow(hComboPreset,  260,  345 + yOffset, 90, 200, TRUE);
        MoveWindow(hComboCRF,     430,  345 + yOffset, 90, 200, TRUE);

        MoveWindow(hComboRes,          80,  375 + yOffset, 90, 200, TRUE);
        MoveWindow(hComboFPS,         250,  375 + yOffset, 90, 200, TRUE);
        MoveWindow(hComboAudioBitrate, 420, 375 + yOffset, 90, 200, TRUE);

        MoveWindow(hComboAudioRate,  80,  405 + yOffset, 90, 200, TRUE);
        MoveWindow(hComboChannels,  250,  405 + yOffset, 90, 200, TRUE);
        MoveWindow(hEditVolume,     420,  405 + yOffset, 90, 22,  TRUE);

        MoveWindow(hEditBrightness,  90,  445 + yOffset, 50, 22, TRUE);
        MoveWindow(hEditContrast,   220,  445 + yOffset, 50, 22, TRUE);
        MoveWindow(hEditSaturation, 350,  445 + yOffset, 50, 22, TRUE);
        MoveWindow(hEditSharpen,    470,  445 + yOffset, 50, 22, TRUE);
        MoveWindow(hEditSpeed,      570,  445 + yOffset, 50, 22, TRUE);

        MoveWindow(hEditStart,     80,  485 + yOffset, 100, 22, TRUE);
        MoveWindow(hEditDuration, 250,  485 + yOffset, 100, 22, TRUE);

        MoveWindow(hBtnConvert,  20,  525 + yOffset, 100, 30, TRUE);
        MoveWindow(hStatus,     140,  530 + yOffset, w - 160, 25, TRUE);

        for (size_t i = 0; i < g_movableLabels.size(); ++i) {
            MovableLabel& ml = g_movableLabels[i];
            MoveWindow(ml.hwnd, ml.x, ml.y + yOffset, ml.w, ml.h, TRUE);
        }
        return 0;
    }

    case WM_COMMAND: {
        int wmId    = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        if (wmId == 5  && wmEvent == CBN_SELCHANGE)  UpdateControlStates(hwnd);
        if (wmId == 33 && wmEvent == LBN_SELCHANGE)  UpdateFormatCombo(hwnd);

        if (wmId == 2) {   // 添加文件
            OPENFILENAMEW ofn = { 0 };
            wchar_t szFile[26000] = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hwnd;
            ofn.lpstrFile   = szFile;
            ofn.nMaxFile    = sizeof(szFile) / sizeof(wchar_t);
            ofn.lpstrFilter =
                L"所有文件\0*.*\0"
                L"视频文件\0*.mp4;*.avi;*.mkv;*.mov;*.webm\0"
                L"音频文件\0*.mp3;*.wav;*.aac;*.flac;*.m4a\0";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST
                      | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

            if (GetOpenFileNameW(&ofn) == TRUE) {
                wchar_t* p = szFile;
                std::wstring dir = p;
                p += dir.length() + 1;
                if (*p == L'\0') {
                    AddToQueue(dir);
                } else {
                    while (*p) {
                        AddToQueue(dir + L"\\" + p);
                        p += wcslen(p) + 1;
                    }
                }
                RefreshListBox();
                AutoSelectFirst(hwnd);
                SetWindowTextW(hStatus, L"状态：文件已加入队列");
            }
        }
        else if (wmId == 30) {  // 添加文件夹
            BROWSEINFOW bi = { 0 };
            bi.hwndOwner  = hwnd;
            bi.lpszTitle  = L"请选择包含音视频的文件夹";
            bi.ulFlags    = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH] = { 0 };
                if (SHGetPathFromIDListW(pidl, path)) {
                    std::vector<std::wstring> found;
                    ScanFolder(path, found);

                    size_t before = g_fileList.size();
                    for (size_t i = 0; i < found.size(); ++i) AddToQueue(found[i]);
                    int added = (int)(g_fileList.size() - before);

                    RefreshListBox();
                    AutoSelectFirst(hwnd);

                    if (found.empty()) {
                        MessageBoxW(hwnd, L"未找到支持的音视频文件。",
                                    L"提示", MB_OK | MB_ICONWARNING);
                    } else {
                        std::wstring info = L"扫描完成！\n本次新增 "
                                          + std::to_wstring(added)
                                          + L" 个文件。\n当前队列共 "
                                          + std::to_wstring(g_fileList.size())
                                          + L" 个。";
                        MessageBoxW(hwnd, info.c_str(), L"提示",
                                    MB_OK | MB_ICONINFORMATION);
                    }
                }
                CoTaskMemFree(pidl);
            }
        }
        else if (wmId == 40) {  // 清空列表
            g_fileList.clear();
            RefreshListBox();
            UpdateFormatCombo(hwnd);
            SetWindowTextW(hStatus, L"状态：列表已清空");
        }
        else if (wmId == 41) {  // 移除选中
            int count = (int)SendMessageW(hListFiles, LB_GETSELCOUNT, 0, 0);
            if (count > 0) {
                std::vector<int> indices(count);
                SendMessageW(hListFiles, LB_GETSELITEMS, count, (LPARAM)&indices[0]);
                std::sort(indices.begin(), indices.end(), std::greater<int>());
                for (size_t i = 0; i < indices.size(); ++i)
                    g_fileList.erase(g_fileList.begin() + indices[i]);
                RefreshListBox();
                AutoSelectFirst(hwnd);
            }
        }
        else if (wmId == 51) {  // 过滤并全选
            int fmtIdx = (int)SendMessageW(hComboFilterFormat, CB_GETCURSEL, 0, 0);
            if (fmtIdx < 0) {
                MessageBoxW(hwnd, L"当前列表中没有可过滤的格式！",
                            L"提示", MB_OK | MB_ICONWARNING);
                break;
            }
            wchar_t fmtBuf[50] = { 0 };
            SendMessageW(hComboFilterFormat, CB_GETLBTEXT, fmtIdx, (LPARAM)fmtBuf);
            std::wstring targetExt = ToLower(fmtBuf);

            std::vector<std::wstring> newList;
            for (size_t i = 0; i < g_fileList.size(); ++i)
                if (GetFileExtension(g_fileList[i]) == targetExt)
                    newList.push_back(g_fileList[i]);

            if (newList.empty()) {
                MessageBoxW(hwnd, L"过滤后没有剩余文件！",
                            L"提示", MB_OK | MB_ICONWARNING);
                break;
            }

            g_fileList = newList;
            RefreshListBox();

            int cnt = (int)SendMessageW(hListFiles, LB_GETCOUNT, 0, 0);
            if (cnt > 0) {
                SendMessageW(hListFiles, LB_SETSEL, TRUE, -1);
                SendMessageW(hListFiles, LB_SELITEMRANGE, TRUE, MAKELPARAM(0, cnt - 1));
                SendMessageW(hListFiles, LB_SETCARETINDEX, 0, FALSE);
            }
            UpdateFormatCombo(hwnd);

            std::wstring status = L"状态：已过滤出 " + std::to_wstring(cnt)
                                + L" 个 " + targetExt + L" 文件";
            SetWindowTextW(hStatus, status.c_str());
        }
        else if (wmId == 4) {   // 选择输出目录
            BROWSEINFOW bi = { 0 };
            bi.hwndOwner = hwnd;
            bi.lpszTitle = L"请选择输出文件夹";
            bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH] = { 0 };
                if (SHGetPathFromIDListW(pidl, path))
                    SetWindowTextW(hEditDst, path);
                CoTaskMemFree(pidl);
            }
        }
        else if (wmId == 60) {  // 打开输出目录
            wchar_t path[260] = { 0 };
            GetWindowTextW(hEditDst, path, 260);
            if (wcslen(path) > 0) {
                ShellExecuteW(hwnd, L"open", path, NULL, NULL, SW_SHOWNORMAL);
            } else if (!g_lastDstDir.empty()) {
                ShellExecuteW(hwnd, L"open", g_lastDstDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
            } else {
                MessageBoxW(hwnd, L"还没有输出目录。", L"提示", MB_OK | MB_ICONINFORMATION);
            }
        }
        else if (wmId == 10) {  // 开始转换
            StartConversion(hwnd);
        }
        break;
    }

    case WM_APP_PROGRESS: {
        std::wstring* p = (std::wstring*)lParam;
        if (p) { SetWindowTextW(hStatus, p->c_str()); delete p; }
        return 0;
    }

    case WM_APP_FINISHED: {
        FinishedPayload* p = (FinishedPayload*)lParam;
        if (p) {
            g_isConverting.store(false);
            EnableWindow(hBtnConvert, TRUE);
            EnableWindow(hBtnOpenDst, TRUE);

            std::wstring msg = L"批量转换完成！\n成功: "
                             + std::to_wstring(p->success) + L" / "
                             + std::to_wstring(p->total);
            if (!p->dstDir.empty())
                msg += L"\n输出目录: " + p->dstDir;
            if (!p->targetExt.empty())
                msg += L"\n输出格式: " + p->targetExt;
            if (p->skipped > 0)
                msg += L"\n跳过（音频转视频）: " + std::to_wstring(p->skipped);
            if (p->failed > 0) {
                msg += L"\n失败: " + std::to_wstring(p->failed);
                msg += L"\n\n错误摘要（完整日志见 log.txt）：";
                for (size_t i = 0; i < p->errors.size() && i < 5; ++i)
                    msg += L"\n" + p->errors[i];
                if (p->errors.size() > 5)
                    msg += L"\n... 还有 "
                         + std::to_wstring(p->errors.size() - 5) + L" 条";
            }

            MessageBoxW(hwnd, msg.c_str(), L"完成",
                        MB_OK | (p->failed > 0 ? MB_ICONWARNING : MB_ICONINFORMATION));

            SetWindowTextW(hStatus,
                p->failed > 0
                    ? (L"状态：完成，失败 " + std::to_wstring(p->failed) + L" 个").c_str()
                    : L"状态：批量转换完成！");

            delete p;
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ============================================================
// 入口
// ============================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FFmpegGUIProMaxBatch";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"FFmpegGUIProMaxBatch",
        L"音视频批量转换与编辑工具",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 820, 780,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    CoUninitialize();
    return (int)msg.wParam;
}