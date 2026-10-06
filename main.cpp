// ============================================================
// 音视频批量转换工具 (Win32 / g++ / 中英双语)
// ============================================================
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <commctrl.h>
#include <shellapi.h>
#include <cstdarg>

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

#include "resource.h"

#define WM_APP_PROGRESS (WM_APP + 1)
#define WM_APP_FINISHED (WM_APP + 2)

// ================= 格式枚举（不依赖文本） =================
enum FmtCode {
    FMT_MP4 = 1, FMT_MKV, FMT_AVI,
    FMT_MP3, FMT_WAV, FMT_AAC, FMT_FLAC,
    FMT_MP4_COPY, FMT_MKV_COPY, FMT_AVI_COPY
};

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

struct MovableLabel { HWND hwnd; int x, y, w, h; };
static std::vector<MovableLabel> g_movableLabels;

static HWND MakeMovableLabel(HWND parent, UINT strId,
                             int x, int y, int w, int h);

static std::vector<std::wstring> g_fileList;
static std::atomic<bool> g_isConverting(false);
static std::wstring g_lastDstDir;
static HFONT g_hFont = NULL;

// ================= 国际化辅助 =================
static std::wstring LoadStr(UINT id) {
    wchar_t buf[1024] = { 0 };
    int n = LoadStringW(GetModuleHandleW(NULL), id, buf, 1024);
    if (n <= 0) return L"";
    return std::wstring(buf, n);
}

static std::wstring LoadStrF(UINT id, ...) {
    std::wstring fmt = LoadStr(id);
    wchar_t buf[2048] = { 0 };
    va_list args;
    va_start(args, id);
    _vsnwprintf(buf, 2047, fmt.c_str(), args);
    va_end(args);
    return std::wstring(buf);
}

// ================= 数据结构 =================
struct ConversionOptions {
    std::wstring inputPath;
    std::wstring outputPath;
    std::wstring targetExt;
    bool sourceHasAudio = true;
    bool isAudioOutput  = false;
    bool isStreamCopy   = false;

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

struct RunResult { bool success = false; DWORD exitCode = 0; std::wstring stderrText; };
struct ConvertResult { bool success = false; std::wstring error; };
struct FinishedPayload {
    int success = 0, failed = 0, skipped = 0, total = 0;
    std::wstring dstDir, targetExt;
    std::vector<std::wstring> errors;
};
struct MediaInfo {
    bool hasVideo = false, hasAudio = false, valid = false;
    std::wstring videoCodec, audioCodec;
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
    for (size_t i = 0; i < v.size(); ++i) { if (i) r += sep; r += v[i]; }
    return r;
}
static std::wstring ToLower(const std::wstring& s) {
    std::wstring r = s;
    for (size_t i = 0; i < r.size(); ++i) r[i] = (wchar_t)towlower(r[i]);
    return r;
}
static int WcsICmp(const wchar_t* a, const wchar_t* b) {
    while (*a && *b) {
        wchar_t ca = (wchar_t)towlower(*a), cb = (wchar_t)towlower(*b);
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
    return (end == s.c_str()) ? fallback : v;
}
static bool FileExists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}
static bool IsVideoExtension(const std::wstring& ext) {
    static const wchar_t* arr[] = { L".mp4", L".avi", L".mkv", L".mov", L".webm" };
    for (size_t i = 0; i < 5; ++i) if (ext == arr[i]) return true;
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

static void WriteLog(const std::wstring& text) {
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring p(exePath);
    size_t slash = p.find_last_of(L"\\/");
    std::wstring logPath = (slash == std::wstring::npos) ? L"log.txt"
                         : p.substr(0, slash + 1) + L"log.txt";
    HANDLE h = CreateFileW(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
    if (len > 0) {
        std::string utf8(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], len, NULL, NULL);
        DWORD written = 0;
        WriteFile(h, utf8.c_str(), (DWORD)utf8.size(), &written, NULL);
    }
    CloseHandle(h);
}

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

static RunResult RunFfmpeg(const std::wstring& ffmpegPath, const std::wstring& args) {
    RunResult result;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead = NULL, hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        result.stderrText = L"CreatePipe failed"; return result;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite; si.hStdError = hWrite;
    PROCESS_INFORMATION pi = { 0 };
    std::wstring cmdLine = QuoteArg(ffmpegPath) + L" " + args;
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');
    BOOL ok = CreateProcessW(NULL, &cmdBuf[0], NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hWrite);
    if (!ok) {
        wchar_t buf[1024];
        swprintf(buf, 1024, L"CreateProcessW failed, err=%lu\n%ls",
                 (unsigned long)GetLastError(), cmdLine.c_str());
        result.stderrText = buf; CloseHandle(hRead); return result;
    }
    std::string buffer; char temp[4096]; DWORD bytesRead = 0;
    while (ReadFile(hRead, temp, sizeof(temp), &bytesRead, NULL) && bytesRead > 0)
        buffer.append(temp, bytesRead);
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &result.exitCode);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    if (!buffer.empty()) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), (int)buffer.size(), NULL, 0);
        if (wlen > 0) {
            result.stderrText.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, buffer.data(), (int)buffer.size(),
                                &result.stderrText[0], wlen);
        }
    }
    result.success = (result.exitCode == 0);
    return result;
}

static MediaInfo ProbeMediaInfo(const std::wstring& ffmpegPath, const std::wstring& inputPath) {
    MediaInfo info;
    RunResult r = RunFfmpeg(ffmpegPath, L"-hide_banner -i " + QuoteArg(inputPath));
    const std::wstring& s = r.stderrText;
    size_t vpos = s.find(L"Video: ");
    if (vpos != std::wstring::npos) {
        size_t start = vpos + 7;
        size_t end = s.find_first_of(L" ,(", start);
        if (end != std::wstring::npos) {
            info.videoCodec = ToLower(s.substr(start, end - start));
            info.hasVideo = !info.videoCodec.empty();
        }
    }
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

static bool ContainerSupportsVideoCodec(const std::wstring& c, const std::wstring& codec) {
    if (c == L".mkv") return codec==L"h264"||codec==L"hevc"||codec==L"h265"||codec==L"mpeg4"
                        ||codec==L"vp8"||codec==L"vp9"||codec==L"av1"||codec==L"mpeg2video"||codec==L"vc1";
    if (c == L".mp4") return codec==L"h264"||codec==L"hevc"||codec==L"h265"||codec==L"mpeg4"
                        ||codec==L"av1"||codec==L"vp9";
    if (c == L".avi") return codec==L"h264"||codec==L"mpeg4"||codec==L"mjpeg";
    if (c == L".mov") return codec==L"h264"||codec==L"hevc"||codec==L"prores";
    if (c == L".webm") return codec==L"vp8"||codec==L"vp9"||codec==L"av1";
    return false;
}
static bool ContainerSupportsAudioCodec(const std::wstring& c, const std::wstring& codec) {
    if (c == L".mkv") return codec==L"aac"||codec==L"mp3"||codec==L"opus"||codec==L"vorbis"
                        ||codec==L"flac"||codec==L"ac3"||codec==L"dts"||codec==L"eac3"||codec==L"pcm_s16le";
    if (c == L".mp4") return codec==L"aac"||codec==L"mp3"||codec==L"ac3"||codec==L"eac3"||codec==L"alac";
    if (c == L".avi") return codec==L"mp3"||codec==L"ac3"||codec==L"pcm_s16le";
    if (c == L".mov") return codec==L"aac"||codec==L"pcm_s16le"||codec==L"pcm_s24le";
    if (c == L".webm") return codec==L"vorbis"||codec==L"opus";
    return false;
}
static bool CanStreamCopy(const MediaInfo& info, const std::wstring& dstExt) {
    if (!info.valid) return false;
    if (info.hasVideo) {
        if (!IsVideoExtension(dstExt)) return false;
        if (!ContainerSupportsVideoCodec(dstExt, info.videoCodec)) return false;
        if (info.hasAudio && !ContainerSupportsAudioCodec(dstExt, info.audioCodec)) return false;
        return true;
    }
    return false;
}
static bool HasAudioStream(const std::wstring& ffmpegPath, const std::wstring& inputPath) {
    return ProbeMediaInfo(ffmpegPath, inputPath).hasAudio;
}

// ============================================================
// 参数构建
// ============================================================
static std::wstring BuildFfmpegArgs(const ConversionOptions& o) {
    std::wstring args;
    args += L"-hide_banner -i " + QuoteArg(o.inputPath) + L" ";
    if (!o.startTime.empty()) args += L"-ss " + QuoteArg(o.startTime) + L" ";
    if (!o.duration.empty())  args += L"-t "  + QuoteArg(o.duration)  + L" ";

    if (o.isStreamCopy) {
        args += L"-map 0:v? -map 0:a? -c copy ";
        if (o.targetExt == L".mp4") args += L"-movflags +faststart ";
        args += QuoteArg(o.outputPath) + L" -y";
        return args;
    }

    bool applySpeed = (o.speed >= 0.5 && o.speed <= 4.0 && fabs(o.speed - 1.0) > 0.001);

    if (o.isAudioOutput) {
        args += L"-vn ";
        if      (o.targetExt == L".mp3")  args += L"-c:a libmp3lame ";
        else if (o.targetExt == L".wav")  args += L"-c:a pcm_s16le ";
        else if (o.targetExt == L".aac")  args += L"-c:a aac ";
        else if (o.targetExt == L".flac") args += L"-c:a flac ";
        std::vector<std::wstring> af;
        if (fabs(o.volume - 1.0) > 0.001) af.push_back(L"volume=" + Num(o.volume));
        if (applySpeed) {
            std::vector<std::wstring> c = BuildAtempoChain(o.speed);
            af.insert(af.end(), c.begin(), c.end());
        }
        if (!af.empty()) args += L"-af " + QuoteArg(Join(af, L",")) + L" ";
        if (!o.audioBitrate.empty()) args += L"-b:a " + o.audioBitrate + L" ";
        if (o.audioSampleRate > 0)   args += L"-ar " + std::to_wstring(o.audioSampleRate) + L" ";
        if (o.audioChannels > 0)     args += L"-ac " + std::to_wstring(o.audioChannels)   + L" ";
        args += QuoteArg(o.outputPath) + L" -y";
        return args;
    }

    args += L"-c:v libx264 ";
    args += L"-preset " + o.preset + L" ";
    args += L"-crf " + std::to_wstring(o.crf) + L" ";
    args += L"-pix_fmt yuv420p ";
    if (!o.resolution.empty()) args += L"-s " + o.resolution + L" ";
    if (o.fps > 0)             args += L"-r " + std::to_wstring(o.fps) + L" ";

    std::vector<std::wstring> vf;
    if (fabs(o.brightness) > 0.001 || fabs(o.contrast - 1) > 0.001 ||
        fabs(o.saturation - 1) > 0.001) {
        std::vector<std::wstring> eq;
        if (fabs(o.brightness) > 0.001)     eq.push_back(L"brightness=" + Num(o.brightness));
        if (fabs(o.contrast - 1) > 0.001)   eq.push_back(L"contrast="   + Num(o.contrast));
        if (fabs(o.saturation - 1) > 0.001) eq.push_back(L"saturation=" + Num(o.saturation));
        vf.push_back(L"eq=" + Join(eq, L":"));
    }
    if (o.sharpen > 0.001) vf.push_back(L"unsharp=5:5:" + Num(o.sharpen));

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
            std::vector<std::wstring> c = BuildAtempoChain(o.speed);
            afCopy.insert(afCopy.end(), c.begin(), c.end());
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
    } else args += L"-an ";

    if (o.targetExt == L".mp4") args += L"-movflags +faststart ";
    args += QuoteArg(o.outputPath) + L" -y";
    return args;
}

static ConvertResult ConvertOneFile(const std::wstring& ffmpegPath, ConversionOptions& opt) {
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
    WriteLog(L"\n====\nInput: " + opt.inputPath + L"\nOutput: " + opt.outputPath
           + L"\nMode: " + (opt.isStreamCopy ? L"copy" : L"reencode")
           + L"\nArgs: " + args + L"\n");
    RunResult r = RunFfmpeg(ffmpegPath, args);
    WriteLog(L"Exit: " + std::to_wstring((int)r.exitCode) + L"\n"
           + r.stderrText + L"\n");
    result.success = r.success;
    if (!r.success) {
        std::wstring err = r.stderrText;
        if (err.size() > 1500) err = err.substr(err.size() - 1500);
        result.error = err;
    }
    return result;
}

// ============================================================
// UI 更新
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
    int code = (int)SendMessageW(hComboFormat, CB_GETITEMDATA, fmtIdx, 0);

    bool isAudio = (code == FMT_MP3 || code == FMT_WAV || code == FMT_AAC || code == FMT_FLAC);
    bool isCopy  = (code == FMT_MP4_COPY || code == FMT_MKV_COPY || code == FMT_AVI_COPY);

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
}

static void AddFmtItem(HWND hc, UINT strId, int code) {
    std::wstring s = LoadStr(strId);
    int idx = (int)SendMessageW(hc, CB_ADDSTRING, 0, (LPARAM)s.c_str());
    SendMessageW(hc, CB_SETITEMDATA, idx, code);
}
static void AddFmtItemCopy(HWND hc, UINT strId, int code) {
    std::wstring s = LoadStr(strId) + LoadStr(IDS_FMT_COPY_SUFFIX);
    int idx = (int)SendMessageW(hc, CB_ADDSTRING, 0, (LPARAM)s.c_str());
    SendMessageW(hc, CB_SETITEMDATA, idx, code);
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
    MediaInfo info = ProbeMediaInfo(GetFFmpegPath(), srcPath);

    SendMessageW(hComboFormat, CB_RESETCONTENT, 0, 0);

    if (isVideo) {
        struct VT { UINT strId; const wchar_t* ext; int code; int copyCode; };
        static const VT vts[] = {
            { IDS_FMT_MP4, L".mp4", FMT_MP4, FMT_MP4_COPY },
            { IDS_FMT_MKV, L".mkv", FMT_MKV, FMT_MKV_COPY },
            { IDS_FMT_AVI, L".avi", FMT_AVI, FMT_AVI_COPY },
        };
        for (size_t i = 0; i < 3; ++i) {
            if (ext == vts[i].ext) continue;
            AddFmtItem(hComboFormat, vts[i].strId, vts[i].code);
            if (CanStreamCopy(info, vts[i].ext))
                AddFmtItemCopy(hComboFormat, vts[i].strId, vts[i].copyCode);
        }
        if (ext != L".mp3")  AddFmtItem(hComboFormat, IDS_FMT_MP3,  FMT_MP3);
        if (ext != L".wav")  AddFmtItem(hComboFormat, IDS_FMT_WAV,  FMT_WAV);
        if (ext != L".aac")  AddFmtItem(hComboFormat, IDS_FMT_AAC,  FMT_AAC);
        if (ext != L".flac") AddFmtItem(hComboFormat, IDS_FMT_FLAC, FMT_FLAC);
    } else {
        if (ext != L".mp3")  AddFmtItem(hComboFormat, IDS_FMT_MP3,  FMT_MP3);
        if (ext != L".wav")  AddFmtItem(hComboFormat, IDS_FMT_WAV,  FMT_WAV);
        if (ext != L".aac")  AddFmtItem(hComboFormat, IDS_FMT_AAC,  FMT_AAC);
        if (ext != L".flac") AddFmtItem(hComboFormat, IDS_FMT_FLAC, FMT_FLAC);
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
            if (IsVideoExtension(ext) || ext == L".mp3" || ext == L".wav" ||
                ext == L".aac" || ext == L".flac" || ext == L".m4a")
                out.push_back(folder + L"\\" + fd.cFileName);
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
    std::wstring dstDir, ffmpegPath;
    ConversionOptions tmpl;
    int total;
};

static unsigned __stdcall ConversionThreadProc(void* param) {
    ThreadData* data = (ThreadData*)param;
    HWND hwnd = data->hwnd;
    std::wstring dst = data->dstDir, fmp = data->ffmpegPath;
    ConversionOptions tmpl = data->tmpl;
    int total = data->total;

    FinishedPayload* payload = new FinishedPayload();
    payload->total = total;
    payload->dstDir = dst;
    payload->targetExt = tmpl.targetExt;

    for (size_t i = 0; i < data->files.size(); ++i) {
        const std::wstring& srcPath = data->files[i];
        std::wstring srcExt = GetFileExtension(srcPath);
        bool isSrcVideo = IsVideoExtension(srcExt);
        if (!isSrcVideo && !tmpl.isAudioOutput) { payload->skipped++; continue; }

        std::wstring* msg = new std::wstring(
            LoadStrF(IDS_STATUS_CONVERTING) + L" (" + std::to_wstring(i + 1)
            + L"/" + std::to_wstring(data->files.size()) + L")...");
        PostMessageW(hwnd, WM_APP_PROGRESS, 0, (LPARAM)msg);

        ConversionOptions opt = tmpl;
        opt.inputPath  = srcPath;
        opt.outputPath = dst + L"\\" + GetBaseName(srcPath) + opt.targetExt;
        if (!opt.isStreamCopy)
            opt.sourceHasAudio = !isSrcVideo || HasAudioStream(fmp, srcPath);

        ConvertResult r = ConvertOneFile(fmp, opt);
        if (r.success) payload->success++;
        else {
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
// 开始转换
// ============================================================
static void StartConversion(HWND hwnd) {
    if (g_isConverting.load()) {
        MessageBoxW(hwnd, LoadStr(IDS_MSG_BUSY).c_str(),
                    LoadStr(IDS_MSG_TIP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (g_fileList.empty()) {
        MessageBoxW(hwnd, LoadStr(IDS_MSG_QUEUE_EMPTY).c_str(),
                    LoadStr(IDS_MSG_TIP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
        return;
    }
    wchar_t dstDir[260] = { 0 };
    GetWindowTextW(hEditDst, dstDir, 260);
    if (wcslen(dstDir) == 0) {
        MessageBoxW(hwnd, LoadStr(IDS_MSG_NO_OUTPUT_DIR).c_str(),
                    LoadStr(IDS_MSG_TIP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
        return;
    }
    std::wstring ffmpegPath = GetFFmpegPath();
    if (!FileExists(ffmpegPath)) {
        MessageBoxW(hwnd, LoadStr(IDS_MSG_FFMPEG_MISSING).c_str(),
                    LoadStr(IDS_MSG_ERROR_TITLE).c_str(), MB_OK | MB_ICONERROR);
        return;
    }
    int fmtIdx = (int)SendMessageW(hComboFormat, CB_GETCURSEL, 0, 0);
    if (fmtIdx < 0) {
        MessageBoxW(hwnd, LoadStr(IDS_MSG_NO_FORMAT).c_str(),
                    LoadStr(IDS_MSG_TIP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
        return;
    }
    int code = (int)SendMessageW(hComboFormat, CB_GETITEMDATA, fmtIdx, 0);

    bool isStreamCopy = (code == FMT_MP4_COPY || code == FMT_MKV_COPY || code == FMT_AVI_COPY);
    std::wstring targetExt;
    bool isAudioOutput = false;
    switch (code) {
        case FMT_MP4: case FMT_MP4_COPY: targetExt = L".mp4"; break;
        case FMT_MKV: case FMT_MKV_COPY: targetExt = L".mkv"; break;
        case FMT_AVI: case FMT_AVI_COPY: targetExt = L".avi"; break;
        case FMT_MP3:  targetExt = L".mp3";  isAudioOutput = true; break;
        case FMT_WAV:  targetExt = L".wav";  isAudioOutput = true; break;
        case FMT_AAC:  targetExt = L".aac";  isAudioOutput = true; break;
        case FMT_FLAC: targetExt = L".flac"; isAudioOutput = true; break;
        default: targetExt = L".mp4"; break;
    }

    static const wchar_t* PRESETS[] = { L"medium", L"fast", L"slow" };
    static const int       CRFS[]    = { 18, 23, 28 };
    int presetIdx       = (int)SendMessageW(hComboPreset,       CB_GETCURSEL, 0, 0);
    int crfIdx          = (int)SendMessageW(hComboCRF,          CB_GETCURSEL, 0, 0);
    int audioBitrateIdx = (int)SendMessageW(hComboAudioBitrate, CB_GETCURSEL, 0, 0);
    int fpsIdx          = (int)SendMessageW(hComboFPS,          CB_GETCURSEL, 0, 0);
    int resIdx          = (int)SendMessageW(hComboRes,          CB_GETCURSEL, 0, 0);
    int audioRateIdx    = (int)SendMessageW(hComboAudioRate,    CB_GETCURSEL, 0, 0);
    int channelsIdx     = (int)SendMessageW(hComboChannels,     CB_GETCURSEL, 0, 0);

    wchar_t startTime[50]={0}, duration[50]={0}, speed[20]={0};
    wchar_t bright[20]={0}, contrast[20]={0}, sat[20]={0};
    wchar_t sharpen[20]={0}, volume[20]={0};
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
    tmpl.isAudioOutput = isAudioOutput;
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
    data->hwnd = hwnd;
    data->files = g_fileList;
    data->dstDir = dstDir;
    data->ffmpegPath = ffmpegPath;
    data->tmpl = tmpl;
    data->total = (int)g_fileList.size();

    g_isConverting.store(true);
    EnableWindow(hBtnConvert, FALSE);
    EnableWindow(hBtnOpenDst, FALSE);
    SetWindowTextW(hStatus,
        (isStreamCopy ? LoadStr(IDS_STATUS_COPY_CONVERTING)
                      : LoadStr(IDS_STATUS_CONVERTING)).c_str());

    unsigned tid = 0;
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, ConversionThreadProc, data, 0, &tid);
    if (hThread) CloseHandle(hThread);
    else {
        delete data;
        g_isConverting.store(false);
        EnableWindow(hBtnConvert, TRUE);
        EnableWindow(hBtnOpenDst, TRUE);
        MessageBoxW(hwnd, L"thread failed", L"Error", MB_OK | MB_ICONERROR);
    }
}

// ============================================================
// WM_CREATE 里的控件创建封装
// ============================================================
static HWND MakeLabel(HWND hwnd, UINT strId, int x, int y, int w, int h) {
    HWND hw = CreateWindowW(L"Static", LoadStr(strId).c_str(), WS_CHILD | WS_VISIBLE,
                            x, y, w, h, hwnd, NULL, NULL, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return hw;
}
static HWND MakeMovableLabelImpl(HWND hwnd, UINT strId, int x, int y, int w, int h) {
    HWND hw = MakeLabel(hwnd, strId, x, y, w, h);
    MovableLabel ml = { hw, x, y, w, h };
    g_movableLabels.push_back(ml);
    return hw;
}
static HWND MakeMovableLabel(HWND parent, UINT strId, int x, int y, int w, int h) {
    return MakeMovableLabelImpl(parent, strId, x, y, w, h);
}
static HWND MakeButton(HWND hwnd, UINT strId, int id, int x, int y, int w, int h) {
    HWND hw = CreateWindowW(L"Button", LoadStr(strId).c_str(), WS_CHILD | WS_VISIBLE,
                            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return hw;
}
static HWND MakeCombo(HWND hwnd, int id, int x, int y, int w, int h) {
    HWND hw = CreateWindowW(L"ComboBox", L"",
                            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return hw;
}
static HWND MakeEdit(HWND hwnd, int id, int x, int y, int w, int h) {
    HWND hw = CreateWindowW(L"Edit", L"",
                            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return hw;
}
static void FillComboItems(HWND hc, const UINT* ids, int count) {
    for (int i = 0; i < count; ++i)
        SendMessageW(hc, CB_ADDSTRING, 0, (LPARAM)LoadStr(ids[i]).c_str());
    SendMessageW(hc, CB_SETCURSEL, 0, 0);
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
        // 用系统默认 UI 字体（跟随语言）
        NONCLIENTMETRICSW ncm = { sizeof(NONCLIENTMETRICSW) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        g_hFont = CreateFontIndirectW(&ncm.lfMessageFont);

        SetWindowTextW(hwnd, LoadStr(IDS_WINDOW_TITLE).c_str());

        if (!FileExists(GetFFmpegPath()))
            MessageBoxW(hwnd, LoadStr(IDS_MSG_FFMPEG_WARN).c_str(),
                        LoadStr(IDS_MSG_WARN_TITLE).c_str(), MB_OK | MB_ICONWARNING);

        int y = 15;

        // 第1行
        MakeLabel(hwnd, IDS_QUEUE_LABEL, 20, y, 70, 20);
        hBtnSelectSrc      = MakeButton(hwnd, IDS_BTN_ADD_FILES,  2,  90, y, 80, 22);
        hBtnSelectFolder   = MakeButton(hwnd, IDS_BTN_ADD_FOLDER, 30, 180, y, 90, 22);
        hBtnRemoveSelected = MakeButton(hwnd, IDS_BTN_REMOVE,     41, 280, y, 80, 22);
        hBtnClearList      = MakeButton(hwnd, IDS_BTN_CLEAR,      40, 370, y, 80, 22);
        MakeLabel(hwnd, IDS_FILTER_LABEL, 460, y, 50, 20);
        hComboFilterFormat = MakeCombo(hwnd, 50, 510, y, 90, 200);
        hBtnFilter         = MakeButton(hwnd, IDS_BTN_FILTER, 51, 610, y, 90, 22);
        EnableWindow(hComboFilterFormat, FALSE);
        y += 30;

        // 文件列表
        hListFiles = CreateWindowW(L"ListBox", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL
                      | LBS_EXTENDEDSEL | LBS_NOTIFY,
                      20, y, 760, 250, hwnd, (HMENU)33, NULL, NULL);
        SendMessageW(hListFiles, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        y += 260;

        // 输出目录
        MakeMovableLabel(hwnd, IDS_OUTPUT_DIR_LABEL, 20, y, 60, 20);
        hEditDst      = MakeEdit(hwnd, 3, 80, y, 440, 22);
        hBtnSelectDst = MakeButton(hwnd, IDS_BTN_SELECT_OUTPUT, 4,  530, y, 110, 22);
        hBtnOpenDst   = MakeButton(hwnd, IDS_BTN_OPEN_OUTPUT,   60, 650, y, 110, 22);
        y += 40;

        // 格式 / 预设 / CRF
        MakeMovableLabel(hwnd, IDS_OUTPUT_FORMAT_LABEL, 20, y, 60, 20);
        hComboFormat = MakeCombo(hwnd, 5, 80, y, 110, 260);
        MakeMovableLabel(hwnd, IDS_PRESET_LABEL, 200, y, 60, 20);
        hComboPreset = MakeCombo(hwnd, 20, 260, y, 90, 200);
        MakeMovableLabel(hwnd, IDS_CRF_LABEL, 370, y, 60, 20);
        hComboCRF = MakeCombo(hwnd, 21, 430, y, 90, 200);
        {
            UINT ids[] = { IDS_PRESET_MEDIUM, IDS_PRESET_FAST, IDS_PRESET_SLOW };
            FillComboItems(hComboPreset, ids, 3);
            UINT ids2[] = { IDS_CRF_HIGH, IDS_CRF_BALANCED, IDS_CRF_COMPRESS };
            FillComboItems(hComboCRF, ids2, 3);
            SendMessageW(hComboCRF, CB_SETCURSEL, 1, 0);
        }
        y += 30;

        // 分辨率 / 帧率 / 音频码率
        MakeMovableLabel(hwnd, IDS_RESOLUTION_LABEL, 20, y, 60, 20);
        hComboRes = MakeCombo(hwnd, 14, 80, y, 90, 200);
        MakeMovableLabel(hwnd, IDS_FPS_LABEL, 190, y, 60, 20);
        hComboFPS = MakeCombo(hwnd, 13, 250, y, 90, 200);
        MakeMovableLabel(hwnd, IDS_AUDIO_BITRATE_LABEL, 360, y, 60, 20);
        hComboAudioBitrate = MakeCombo(hwnd, 6, 420, y, 90, 200);
        {
            UINT ids[] = { IDS_RES_KEEP, IDS_RES_1080P, IDS_RES_720P, IDS_RES_480P };
            FillComboItems(hComboRes, ids, 4);
            UINT ids2[] = { IDS_FPS_KEEP, IDS_FPS_60, IDS_FPS_30, IDS_FPS_24 };
            FillComboItems(hComboFPS, ids2, 4);
            UINT ids3[] = { IDS_ABR_AUTO, IDS_ABR_320, IDS_ABR_192, IDS_ABR_128 };
            FillComboItems(hComboAudioBitrate, ids3, 4);
        }
        y += 30;

        // 采样率 / 声道 / 音量
        MakeMovableLabel(hwnd, IDS_AUDIO_RATE_LABEL, 20, y, 60, 20);
        hComboAudioRate = MakeCombo(hwnd, 15, 80, y, 90, 200);
        MakeMovableLabel(hwnd, IDS_CHANNELS_LABEL, 190, y, 60, 20);
        hComboChannels = MakeCombo(hwnd, 16, 250, y, 90, 200);
        MakeMovableLabel(hwnd, IDS_VOLUME_LABEL, 360, y, 60, 20);
        hEditVolume = MakeEdit(hwnd, 22, 420, y, 90, 22);
        {
            UINT ids[] = { IDS_AR_KEEP, IDS_AR_48000, IDS_AR_44100 };
            FillComboItems(hComboAudioRate, ids, 3);
            UINT ids2[] = { IDS_CH_KEEP, IDS_CH_STEREO, IDS_CH_MONO };
            FillComboItems(hComboChannels, ids2, 3);
        }
        y += 40;

        // 颜色滤镜
        MakeMovableLabel(hwnd, IDS_BRIGHTNESS_LABEL, 20, y, 70, 20);
        hEditBrightness = MakeEdit(hwnd, 17, 90, y, 50, 22);
        MakeMovableLabel(hwnd, IDS_CONTRAST_LABEL, 150, y, 70, 20);
        hEditContrast = MakeEdit(hwnd, 18, 220, y, 50, 22);
        MakeMovableLabel(hwnd, IDS_SATURATION_LABEL, 280, y, 70, 20);
        hEditSaturation = MakeEdit(hwnd, 19, 350, y, 50, 22);
        MakeMovableLabel(hwnd, IDS_SHARPEN_LABEL, 410, y, 60, 20);
        hEditSharpen = MakeEdit(hwnd, 23, 470, y, 50, 22);
        MakeMovableLabel(hwnd, IDS_SPEED_LABEL, 530, y, 40, 20);
        hEditSpeed = MakeEdit(hwnd, 7, 570, y, 50, 22);
        y += 40;

        // 时间裁剪
        MakeMovableLabel(hwnd, IDS_START_TIME_LABEL, 20, y, 60, 20);
        hEditStart = MakeEdit(hwnd, 8, 80, y, 100, 22);
        MakeMovableLabel(hwnd, IDS_DURATION_LABEL, 190, y, 60, 20);
        hEditDuration = MakeEdit(hwnd, 9, 250, y, 100, 22);
        y += 40;

        // 按钮 / 状态
        hBtnConvert = MakeButton(hwnd, IDS_BTN_START, 10, 20, y, 100, 30);
        hStatus = CreateWindowW(L"Static", LoadStr(IDS_STATUS_READY).c_str(),
                      WS_CHILD | WS_VISIBLE, 140, y + 5, 640, 25, hwnd, (HMENU)11, NULL, NULL);
        SendMessageW(hStatus, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        EnableWindow(hComboFormat, FALSE);
        EnableWindow(hBtnOpenDst, FALSE);
        break;
    }

    case WM_SIZE: {
        int w = LOWORD(lParam), h = HIWORD(lParam);
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

        for (size_t i = 0; i < g_movableLabels.size(); ++i)
            MoveWindow(g_movableLabels[i].hwnd, g_movableLabels[i].x,
                       g_movableLabels[i].y + yOffset,
                       g_movableLabels[i].w, g_movableLabels[i].h, TRUE);
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam), wmEvent = HIWORD(wParam);
        if (wmId == 5  && wmEvent == CBN_SELCHANGE) UpdateControlStates(hwnd);
        if (wmId == 33 && wmEvent == LBN_SELCHANGE) UpdateFormatCombo(hwnd);

        if (wmId == 2) {   // 添加文件
            OPENFILENAMEW ofn = { 0 };
            wchar_t szFile[26000] = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
            // 动态拼 filter
            std::wstring f1 = LoadStr(IDS_DLG_FILTER_ALL);
            std::wstring f2 = LoadStr(IDS_DLG_FILTER_VIDEO);
            std::wstring f3 = LoadStr(IDS_DLG_FILTER_AUDIO);
            std::wstring filter = f1 + L"\0*.*\0"
                                + f2 + L"\0*.mp4;*.avi;*.mkv;*.mov;*.webm\0"
                                + f3 + L"\0*.mp3;*.wav;*.aac;*.flac;*.m4a\0\0";
            ofn.lpstrFilter = filter.c_str();
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST
                      | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
            if (GetOpenFileNameW(&ofn) == TRUE) {
                wchar_t* p = szFile;
                std::wstring dir = p;
                p += dir.length() + 1;
                if (*p == L'\0') AddToQueue(dir);
                else while (*p) { AddToQueue(dir + L"\\" + p); p += wcslen(p) + 1; }
                RefreshListBox();
                AutoSelectFirst(hwnd);
                SetWindowTextW(hStatus, LoadStr(IDS_STATUS_FILES_ADDED).c_str());
            }
        }
        else if (wmId == 30) {   // 添加文件夹
            BROWSEINFOW bi = { 0 };
            bi.hwndOwner = hwnd;
            std::wstring title = LoadStr(IDS_DLG_PICK_FOLDER);
            bi.lpszTitle = title.c_str();
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
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
                        MessageBoxW(hwnd, LoadStr(IDS_MSG_SCAN_EMPTY).c_str(),
                                    LoadStr(IDS_MSG_TIP_TITLE).c_str(),
                                    MB_OK | MB_ICONWARNING);
                    } else {
                        std::wstring info = LoadStrF(IDS_MSG_SCAN_DONE,
                                            added, (int)g_fileList.size());
                        MessageBoxW(hwnd, info.c_str(),
                                    LoadStr(IDS_MSG_TIP_TITLE).c_str(),
                                    MB_OK | MB_ICONINFORMATION);
                    }
                }
                CoTaskMemFree(pidl);
            }
        }
        else if (wmId == 40) {   // 清空
            g_fileList.clear();
            RefreshListBox();
            UpdateFormatCombo(hwnd);
            SetWindowTextW(hStatus, LoadStr(IDS_STATUS_LIST_CLEARED).c_str());
        }
        else if (wmId == 41) {   // 移除
            int count = (int)SendMessageW(hListFiles, LB_GETSELCOUNT, 0, 0);
            if (count > 0) {
                std::vector<int> idx(count);
                SendMessageW(hListFiles, LB_GETSELITEMS, count, (LPARAM)&idx[0]);
                std::sort(idx.begin(), idx.end(), std::greater<int>());
                for (size_t i = 0; i < idx.size(); ++i)
                    g_fileList.erase(g_fileList.begin() + idx[i]);
                RefreshListBox();
                AutoSelectFirst(hwnd);
            }
        }
        else if (wmId == 51) {   // 过滤
            int fmtIdx = (int)SendMessageW(hComboFilterFormat, CB_GETCURSEL, 0, 0);
            if (fmtIdx < 0) {
                MessageBoxW(hwnd, LoadStr(IDS_MSG_FILTER_EMPTY).c_str(),
                            LoadStr(IDS_MSG_TIP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
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
                MessageBoxW(hwnd, LoadStr(IDS_MSG_FILTER_NOTHING).c_str(),
                            LoadStr(IDS_MSG_TIP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
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
            std::wstring status = LoadStrF(IDS_STATUS_FILTERED, cnt, targetExt.c_str());
            SetWindowTextW(hStatus, status.c_str());
        }
        else if (wmId == 4) {   // 选输出目录
            BROWSEINFOW bi = { 0 };
            bi.hwndOwner = hwnd;
            std::wstring title = LoadStr(IDS_DLG_PICK_OUTPUT);
            bi.lpszTitle = title.c_str();
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
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
            if (wcslen(path) > 0)
                ShellExecuteW(hwnd, L"open", path, NULL, NULL, SW_SHOWNORMAL);
            else if (!g_lastDstDir.empty())
                ShellExecuteW(hwnd, L"open", g_lastDstDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
            else
                MessageBoxW(hwnd, LoadStr(IDS_MSG_NO_OUTPUT_FILES).c_str(),
                            LoadStr(IDS_MSG_TIP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        }
        else if (wmId == 10) StartConversion(hwnd);
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
            std::wstring msg = LoadStrF(IDS_MSG_CONVERT_DONE, p->success, p->total);
            if (!p->dstDir.empty())    msg += L"\n" + p->dstDir;
            if (!p->targetExt.empty()) msg += L"  " + p->targetExt;
            if (p->skipped > 0) msg += L"\n(skipped: " + std::to_wstring(p->skipped) + L")";
            if (p->failed > 0) {
                msg += L"\nFailed: " + std::to_wstring(p->failed);
                for (size_t i = 0; i < p->errors.size() && i < 5; ++i)
                    msg += L"\n" + p->errors[i];
                if (p->errors.size() > 5)
                    msg += L"\n... +" + std::to_wstring(p->errors.size() - 5);
            }
            MessageBoxW(hwnd, msg.c_str(), LoadStr(IDS_MSG_DONE_TITLE).c_str(),
                        MB_OK | (p->failed > 0 ? MB_ICONWARNING : MB_ICONINFORMATION));
            SetWindowTextW(hStatus,
                p->failed > 0
                    ? LoadStrF(IDS_STATUS_DONE_FAIL, p->failed).c_str()
                    : LoadStr(IDS_STATUS_DONE).c_str());
            delete p;
        }
        return 0;
    }

    case WM_DESTROY:
        if (g_hFont) { DeleteObject(g_hFont); g_hFont = NULL; }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FFmpegGUIProMaxBatch";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"FFmpegGUIProMaxBatch",
        LoadStr(IDS_WINDOW_TITLE).c_str(),
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