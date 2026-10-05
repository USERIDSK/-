#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <thread>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")

// ================= 自定义消息（后台线程 -> UI 线程） =================
#define WM_APP_PROGRESS (WM_APP + 1)
#define WM_APP_FINISHED (WM_APP + 2)

// ================= 全局控件句柄 =================
HWND hBtnSelectSrc, hBtnSelectFolder, hBtnRemoveSelected, hBtnClearList;
HWND hComboFilterFormat, hBtnFilter;
HWND hListFiles;
HWND hEditDst, hBtnSelectDst;
HWND hComboFormat, hComboPreset, hComboCRF, hComboAudioBitrate;
HWND hComboRes, hComboFPS, hComboAudioRate, hComboChannels;
HWND hEditVolume;
HWND hEditBrightness, hEditContrast, hEditSaturation, hEditSharpen, hEditSpeed;
HWND hEditStart, hEditDuration;
HWND hBtnConvert, hStatus;

// 待转换文件队列
std::vector<std::wstring> g_fileList;

// ================= 辅助函数前向声明 =================
void UpdateControlStates(HWND hwnd);
std::wstring GetFFmpegPath();

// ================= 辅助函数实现 =================

// 动态获取 ffmpeg 路径：优先当前目录，其次环境变量
std::wstring GetFFmpegPath() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring wsExePath(exePath);
    size_t slash = wsExePath.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        std::wstring localFFmpeg = wsExePath.substr(0, slash + 1) + L"ffmpeg.exe";
        if (GetFileAttributesW(localFFmpeg.c_str()) != INVALID_FILE_ATTRIBUTES)
            return localFFmpeg;
    }
    wchar_t sysPath[MAX_PATH];
    if (SearchPathW(NULL, L"ffmpeg.exe", NULL, MAX_PATH, sysPath, NULL) > 0)
        return std::wstring(sysPath);
    return L"ffmpeg.exe";
}

// 刷新列表框显示 + 更新过滤下拉框
void RefreshListBox() {
    SendMessageW(hListFiles, LB_RESETCONTENT, 0, 0);
    for (const auto& f : g_fileList)
        SendMessageW(hListFiles, LB_ADDSTRING, 0, (LPARAM)f.c_str());

    // 提取所有独立格式
    std::set<std::wstring> exts;
    for (const auto& f : g_fileList) {
        size_t dot = f.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            std::wstring ext = f.substr(dot);
            for (auto &c : ext) c = towlower(c);
            exts.insert(ext);
        }
    }

    // 保留当前选中项
    int curSel = SendMessageW(hComboFilterFormat, CB_GETCURSEL, 0, 0);
    wchar_t curText[50] = { 0 };
    if (curSel >= 0) SendMessageW(hComboFilterFormat, CB_GETLBTEXT, curSel, (LPARAM)curText);

    SendMessageW(hComboFilterFormat, CB_RESETCONTENT, 0, 0);
    if (exts.empty()) {
        EnableWindow(hComboFilterFormat, FALSE);
    } else {
        EnableWindow(hComboFilterFormat, TRUE);
        int foundIdx = -1, idx = 0;
        for (const auto& ext : exts) {
            SendMessageW(hComboFilterFormat, CB_ADDSTRING, 0, (LPARAM)ext.c_str());
            if (_wcsicmp(ext.c_str(), curText) == 0) foundIdx = idx;
            idx++;
        }
        SendMessageW(hComboFilterFormat, CB_SETCURSEL, foundIdx >= 0 ? foundIdx : 0, 0);
    }
}

// 根据列表中【当前选中】的文件，智能生成输出格式下拉框
void UpdateFormatCombo(HWND hwnd) {
    int selCount = SendMessageW(hListFiles, LB_GETSELCOUNT, 0, 0);
    if (selCount == 0) {
        SendMessageW(hComboFormat, CB_RESETCONTENT, 0, 0);
        EnableWindow(hComboFormat, FALSE);
        return;
    }
    EnableWindow(hComboFormat, TRUE);

    int selIndex = -1;
    SendMessageW(hListFiles, LB_GETSELITEMS, 1, (LPARAM)&selIndex);
    if (selIndex < 0) return;

    wchar_t buf[260];
    SendMessageW(hListFiles, LB_GETTEXT, selIndex, (LPARAM)buf);
    std::wstring srcPath(buf);
    size_t dot = srcPath.find_last_of(L'.');
    std::wstring ext = (dot != std::wstring::npos) ? srcPath.substr(dot) : L"";
    for (auto &c : ext) c = towlower(c);

    bool isVideo = (ext == L".mp4" || ext == L".avi" || ext == L".mkv" || ext == L".mov" || ext == L".webm");

    SendMessageW(hComboFormat, CB_RESETCONTENT, 0, 0);
    if (isVideo) {
        if (ext != L".mp4") SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"MP4 (视频)");
        if (ext != L".mkv") SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"MKV (视频)");
        if (ext != L".avi") SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"AVI (视频)");
        SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"MP3 (音频)");
        SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"WAV (音频)");
        SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"AAC (音频)");
        SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"FLAC (音频)");
    } else {
        if (ext != L".mp3") SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"MP3 (音频)");
        if (ext != L".wav") SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"WAV (音频)");
        if (ext != L".aac") SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"AAC (音频)");
        if (ext != L".flac") SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"FLAC (音频)");
    }

    if (SendMessageW(hComboFormat, CB_GETCOUNT, 0, 0) > 0)
        SendMessageW(hComboFormat, CB_SETCURSEL, 0, 0);
    UpdateControlStates(hwnd);
}

// 根据输出格式，启用/禁用视频与音频参数
void UpdateControlStates(HWND hwnd) {
    int fmtIdx = SendMessageW(hComboFormat, CB_GETCURSEL, 0, 0);
    if (fmtIdx < 0) return;
    wchar_t buf[50] = { 0 };
    SendMessageW(hComboFormat, CB_GETLBTEXT, fmtIdx, (LPARAM)buf);
    std::wstring fmtStr(buf);
    bool isAudio = (fmtStr.find(L"音频") != std::wstring::npos);

    EnableWindow(hComboPreset, !isAudio);
    EnableWindow(hComboCRF, !isAudio);
    EnableWindow(hComboRes, !isAudio);
    EnableWindow(hComboFPS, !isAudio);
    EnableWindow(hEditBrightness, !isAudio);
    EnableWindow(hEditContrast, !isAudio);
    EnableWindow(hEditSaturation, !isAudio);
    EnableWindow(hEditSharpen, !isAudio);
}

// 终极稳定版扫描文件夹
void ScanFolder(const std::wstring& folder, std::vector<std::wstring>& outFiles) {
    WIN32_FIND_DATAW findData;
    // 加 \\?\ 前缀，突破 260 字符限制
    std::wstring searchPath = L"\\\\?\\" + folder + L"\\*.*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                ScanFolder(folder + L"\\" + findData.cFileName, outFiles);
            }
        } else {
            std::wstring fileName = findData.cFileName;
            
            // 定义一个辅助 lambda 判断文件末尾是否是指定后缀（忽略大小写）
            auto hasExt = [&](const std::wstring& ext) -> bool {
                if (fileName.length() < ext.length()) return false;
                std::wstring tail = fileName.substr(fileName.length() - ext.length());
                for (auto &c : tail) c = towlower(c);
                return tail == ext;
            };

            // 只要文件名尾部是这些后缀，就加入队列
            if (hasExt(L".mp4") || hasExt(L".avi") || hasExt(L".mkv") || hasExt(L".mov") || hasExt(L".webm") ||
                hasExt(L".mp3") || hasExt(L".wav") || hasExt(L".aac") || hasExt(L".flac") || hasExt(L".m4a")) {
                
                // 保存时去掉 \\?\ 前缀
                outFiles.push_back(folder + L"\\" + fileName);
            }
        }
    } while (FindNextFileW(hFind, &findData));
    FindClose(hFind);
}

// 执行单个文件的转换
void ConvertOneFile(const std::wstring& srcPath, const std::wstring& dstDir,
                    const std::wstring& targetExt, bool isAudioOutput,
                    int presetIdx, int crfIdx, int audioBitrateIdx, int fpsIdx,
                    int resIdx, int audioRateIdx, int channelsIdx,
                    const std::wstring& wsStart, const std::wstring& wsDur,
                    const std::wstring& wsSpeed, const std::wstring& wsBright,
                    const std::wstring& wsContrast, const std::wstring& wsSat,
                    const std::wstring& wsSharpen, const std::wstring& wsVolume) {
    std::wstring fileName = srcPath.substr(srcPath.find_last_of(L"\\/") + 1);
    std::wstring baseName = fileName.substr(0, fileName.find_last_of(L'.'));

    std::wstring targetPath = dstDir + L"\\" + baseName + targetExt;
    int counter = 1;
    while (GetFileAttributesW(targetPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        targetPath = dstDir + L"\\" + baseName + L"_" + std::to_wstring(counter) + targetExt;
        counter++;
    }

    std::wstring cmd = L"\"" + GetFFmpegPath() + L"\" -i \"" + srcPath + L"\" ";
    if (!wsStart.empty()) cmd += L"-ss " + wsStart + L" ";
    if (!wsDur.empty()) cmd += L"-t " + wsDur + L" ";

    if (isAudioOutput) {
        cmd += L"-vn ";
        if (targetExt == L".mp3") cmd += L"-acodec libmp3lame ";
        else if (targetExt == L".wav") cmd += L"-acodec pcm_s16le ";
        else if (targetExt == L".aac") cmd += L"-acodec aac ";
        else if (targetExt == L".flac") cmd += L"-acodec flac ";

        if (audioBitrateIdx == 1) cmd += L"-b:a 320k ";
        else if (audioBitrateIdx == 2) cmd += L"-b:a 192k ";
        else if (audioBitrateIdx == 3) cmd += L"-b:a 128k ";
        if (audioRateIdx == 1) cmd += L"-ar 48000 ";
        else if (audioRateIdx == 2) cmd += L"-ar 44100 ";
        if (channelsIdx == 1) cmd += L"-ac 2 ";
        else if (channelsIdx == 2) cmd += L"-ac 1 ";

        std::wstring audioFilters = L"";
        if (!wsVolume.empty() && wsVolume != L"1.0" && wsVolume != L"1") audioFilters += L"volume=" + wsVolume;
        if (!wsSpeed.empty()) {
            double speed = _wtof(wsSpeed.c_str());
            if (speed >= 0.5 && speed <= 2.0) {
                if (!audioFilters.empty()) audioFilters += L",";
                audioFilters += L"atempo=" + wsSpeed;
            } else if (speed > 2.0 && speed <= 4.0) {
                if (!audioFilters.empty()) audioFilters += L",";
                audioFilters += L"atempo=2.0,atempo=" + std::to_wstring(speed / 2.0);
            }
        }
        if (!audioFilters.empty()) cmd += L"-af \"" + audioFilters + L"\" ";
    } else {
        cmd += L"-c:v libx264 ";
        const wchar_t* preset[] = { L"medium", L"fast", L"slow", L"veryslow" };
        cmd += L"-preset " + std::wstring(preset[presetIdx]) + L" ";
        const wchar_t* crf[] = { L"18", L"23", L"28", L"32" };
        cmd += L"-crf " + std::wstring(crf[crfIdx]) + L" ";
        if (resIdx == 1) cmd += L"-s 1920x1080 ";
        else if (resIdx == 2) cmd += L"-s 1280x720 ";
        else if (resIdx == 3) cmd += L"-s 854x480 ";
        if (fpsIdx == 1) cmd += L"-r 60 ";
        else if (fpsIdx == 2) cmd += L"-r 30 ";
        else if (fpsIdx == 3) cmd += L"-r 24 ";

        // 构建视频滤镜
        std::wstring vf = L"";
        if (!wsBright.empty() && wsBright != L"0") vf += L"eq=brightness=" + wsBright + L":";
        if (!wsContrast.empty() && wsContrast != L"1.0" && wsContrast != L"1") {
            if (vf.empty()) vf += L"eq=contrast=" + wsContrast + L":";
            else vf += L"contrast=" + wsContrast + L":";
        }
        if (!wsSat.empty() && wsSat != L"1.0" && wsSat != L"1") {
            if (vf.empty()) vf += L"eq=saturation=" + wsSat;
            else vf += L"saturation=" + wsSat;
        }
        if (!vf.empty() && vf.back() == L':') vf.pop_back();
        if (!wsSharpen.empty()) {
            if (!vf.empty()) vf += L",";
            vf += L"unsharp=5:5:" + wsSharpen;
        }

        if (!wsSpeed.empty()) {
            double speed = _wtof(wsSpeed.c_str());
            if (speed >= 0.5 && speed <= 2.0) {
                std::wstring vChain = vf.empty() ? L"" : vf + L",";
                cmd += L"-filter_complex \"[0:v]" + vChain + L"setpts=1/" + wsSpeed + L"*PTS[v];[0:a]atempo=" + wsSpeed + L"[a]\" -map \"[v]\" -map \"[a]\" ";
            }
        } else {
            if (!vf.empty()) cmd += L"-vf \"" + vf + L"\" ";
        }
        cmd += L"-c:a aac -b:a 192k ";
        if (targetExt == L".mp4") cmd += L"-movflags +faststart ";
    }
    cmd += L"\"" + targetPath + L"\" -y";
    _wsystem(cmd.c_str());
}

// ================= 窗口过程 =================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 820;
        mmi->ptMinTrackSize.y = 680;
        return 0;
    }
    case WM_CREATE: {
        if (GetFileAttributesW(GetFFmpegPath().c_str()) == INVALID_FILE_ATTRIBUTES)
            MessageBoxW(hwnd, L"警告：未找到 ffmpeg.exe！\n请确保它在本程序目录下，或已配置系统环境变量。",
                        L"缺少核心组件", MB_OK | MB_ICONWARNING);

        int y = 15;
        // ============ 第1行：顶部操作 + 过滤（都在同一行） ============
        CreateWindowW(L"Static", L"待转换队列:", WS_CHILD | WS_VISIBLE, 20, y, 70, 20, hwnd, NULL, NULL, NULL);
        hBtnSelectSrc      = CreateWindowW(L"Button", L"添加文件", WS_CHILD | WS_VISIBLE, 90,  y, 80, 22, hwnd, (HMENU)2,  NULL, NULL);
        hBtnSelectFolder   = CreateWindowW(L"Button", L"添加文件夹", WS_CHILD | WS_VISIBLE, 180, y, 90, 22, hwnd, (HMENU)30, NULL, NULL);
        hBtnRemoveSelected = CreateWindowW(L"Button", L"移除选中", WS_CHILD | WS_VISIBLE, 280, y, 80, 22, hwnd, (HMENU)41, NULL, NULL);
        hBtnClearList      = CreateWindowW(L"Button", L"清空列表", WS_CHILD | WS_VISIBLE, 370, y, 80, 22, hwnd, (HMENU)40, NULL, NULL);
        // 过滤面板（在清空列表的右侧）
        CreateWindowW(L"Static", L"按格式:", WS_CHILD | WS_VISIBLE, 460, y, 50, 20, hwnd, NULL, NULL, NULL);
        hComboFilterFormat = CreateWindowW(L"ComboBox", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 510, y, 90, 200, hwnd, (HMENU)50, NULL, NULL);
        hBtnFilter         = CreateWindowW(L"Button", L"过滤并全选", WS_CHILD | WS_VISIBLE, 610, y, 90, 22, hwnd, (HMENU)51, NULL, NULL);
        EnableWindow(hComboFilterFormat, FALSE);
        y += 30;

        // ============ 第2行：文件列表框 ============
        hListFiles = CreateWindowW(L"ListBox", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL
                                   | LBS_EXTENDEDSEL | LBS_NOTIFY, 20, y, 720, 250, hwnd, (HMENU)33, NULL, NULL);
        y += 260;

        // ============ 第3行：输出目录 ============
        CreateWindowW(L"Static", L"输出目录:", WS_CHILD | WS_VISIBLE, 20, y, 60, 20, hwnd, NULL, NULL, NULL);
        hEditDst      = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 80,  y, 520, 22, hwnd, (HMENU)3, NULL, NULL);
        hBtnSelectDst = CreateWindowW(L"Button", L"选择输出目录", WS_CHILD | WS_VISIBLE, 610, y, 100, 22, hwnd, (HMENU)4, NULL, NULL);
        y += 40;

        // ============ 第4行：格式/预设/CRF ============
        CreateWindowW(L"Static", L"输出格式:", WS_CHILD | WS_VISIBLE, 20, y, 60, 20, hwnd, NULL, NULL, NULL);
        hComboFormat = CreateWindowW(L"ComboBox", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 80, y, 90, 200, hwnd, (HMENU)5, NULL, NULL);

        CreateWindowW(L"Static", L"编码预设:", WS_CHILD | WS_VISIBLE, 190, y, 60, 20, hwnd, NULL, NULL, NULL);
        hComboPreset = CreateWindowW(L"ComboBox", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 250, y, 90, 200, hwnd, (HMENU)20, NULL, NULL);
        SendMessageW(hComboPreset, CB_ADDSTRING, 0, (LPARAM)L"medium(默认)");
        SendMessageW(hComboPreset, CB_ADDSTRING, 0, (LPARAM)L"fast(快速)");
        SendMessageW(hComboPreset, CB_ADDSTRING, 0, (LPARAM)L"slow(慢速)");
        SendMessageW(hComboPreset, CB_SETCURSEL, 0, 0);

        CreateWindowW(L"Static", L"画质(CRF):", WS_CHILD | WS_VISIBLE, 360, y, 60, 20, hwnd, NULL, NULL, NULL);
        hComboCRF = CreateWindowW(L"ComboBox", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 420, y, 90, 200, hwnd, (HMENU)21, NULL, NULL);
        SendMessageW(hComboCRF, CB_ADDSTRING, 0, (LPARAM)L"18(高画质)");
        SendMessageW(hComboCRF, CB_ADDSTRING, 0, (LPARAM)L"23(均衡)");
        SendMessageW(hComboCRF, CB_ADDSTRING, 0, (LPARAM)L"28(压缩优先)");
        SendMessageW(hComboCRF, CB_SETCURSEL, 1, 0);
        y += 30;

        // ============ 第5行：分辨率/帧率/音频码率 ============
        CreateWindowW(L"Static", L"分辨率:", WS_CHILD | WS_VISIBLE, 20, y, 60, 20, hwnd, NULL, NULL, NULL);
        hComboRes = CreateWindowW(L"ComboBox", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 80, y, 90, 200, hwnd, (HMENU)14, NULL, NULL);
        SendMessageW(hComboRes, CB_ADDSTRING, 0, (LPARAM)L"保持原样");
        SendMessageW(hComboRes, CB_ADDSTRING, 0, (LPARAM)L"1920x1080");
        SendMessageW(hComboRes, CB_ADDSTRING, 0, (LPARAM)L"1280x720");
        SendMessageW(hComboRes, CB_ADDSTRING, 0, (LPARAM)L"854x480");
        SendMessageW(hComboRes, CB_SETCURSEL, 0, 0);

        CreateWindowW(L"Static", L"帧率(FPS):", WS_CHILD | WS_VISIBLE, 190, y, 60, 20, hwnd, NULL, NULL, NULL);
        hComboFPS = CreateWindowW(L"ComboBox", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 250, y, 90, 200, hwnd, (HMENU)13, NULL, NULL);
        SendMessageW(hComboFPS, CB_ADDSTRING, 0, (LPARAM)L"保持原样");
        SendMessageW(hComboFPS, CB_ADDSTRING, 0, (LPARAM)L"60");
        SendMessageW(hComboFPS, CB_ADDSTRING, 0, (LPARAM)L"30");
        SendMessageW(hComboFPS, CB_ADDSTRING, 0, (LPARAM)L"24");
        SendMessageW(hComboFPS, CB_SETCURSEL, 0, 0);

        CreateWindowW(L"Static", L"音频码率:", WS_CHILD | WS_VISIBLE, 360, y, 60, 20, hwnd, NULL, NULL, NULL);
        hComboAudioBitrate = CreateWindowW(L"ComboBox", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 420, y, 90, 200, hwnd, (HMENU)6, NULL, NULL);
        SendMessageW(hComboAudioBitrate, CB_ADDSTRING, 0, (LPARAM)L"默认/自动");
        SendMessageW(hComboAudioBitrate, CB_ADDSTRING, 0, (LPARAM)L"320 kbps");
        SendMessageW(hComboAudioBitrate, CB_ADDSTRING, 0, (LPARAM)L"192 kbps");
        SendMessageW(hComboAudioBitrate, CB_ADDSTRING, 0, (LPARAM)L"128 kbps");
        SendMessageW(hComboAudioBitrate, CB_SETCURSEL, 0, 0);
        y += 30;

        // ============ 第6行：采样率/声道/音量 ============
        CreateWindowW(L"Static", L"采样率:", WS_CHILD | WS_VISIBLE, 20, y, 60, 20, hwnd, NULL, NULL, NULL);
        hComboAudioRate = CreateWindowW(L"ComboBox", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 80, y, 90, 200, hwnd, (HMENU)15, NULL, NULL);
        SendMessageW(hComboAudioRate, CB_ADDSTRING, 0, (LPARAM)L"保持原样");
        SendMessageW(hComboAudioRate, CB_ADDSTRING, 0, (LPARAM)L"48000 Hz");
        SendMessageW(hComboAudioRate, CB_ADDSTRING, 0, (LPARAM)L"44100 Hz");
        SendMessageW(hComboAudioRate, CB_SETCURSEL, 0, 0);

        CreateWindowW(L"Static", L"声道:", WS_CHILD | WS_VISIBLE, 190, y, 60, 20, hwnd, NULL, NULL, NULL);
        hComboChannels = CreateWindowW(L"ComboBox", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 250, y, 90, 200, hwnd, (HMENU)16, NULL, NULL);
        SendMessageW(hComboChannels, CB_ADDSTRING, 0, (LPARAM)L"保持原样");
        SendMessageW(hComboChannels, CB_ADDSTRING, 0, (LPARAM)L"立体声(2)");
        SendMessageW(hComboChannels, CB_ADDSTRING, 0, (LPARAM)L"单声道(1)");
        SendMessageW(hComboChannels, CB_SETCURSEL, 0, 0);

        CreateWindowW(L"Static", L"音量倍数:", WS_CHILD | WS_VISIBLE, 360, y, 60, 20, hwnd, NULL, NULL, NULL);
        hEditVolume = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 420, y, 90, 22, hwnd, (HMENU)22, NULL, NULL);
        y += 40;

        // ============ 第7行：颜色滤镜 ============
        CreateWindowW(L"Static", L"亮度(-1~1):", WS_CHILD | WS_VISIBLE, 20, y, 70, 20, hwnd, NULL, NULL, NULL);
        hEditBrightness = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 90, y, 50, 22, hwnd, (HMENU)17, NULL, NULL);
        CreateWindowW(L"Static", L"对比度(0~3):", WS_CHILD | WS_VISIBLE, 150, y, 70, 20, hwnd, NULL, NULL, NULL);
        hEditContrast = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 220, y, 50, 22, hwnd, (HMENU)18, NULL, NULL);
        CreateWindowW(L"Static", L"饱和度(0~3):", WS_CHILD | WS_VISIBLE, 280, y, 70, 20, hwnd, NULL, NULL, NULL);
        hEditSaturation = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 350, y, 50, 22, hwnd, (HMENU)19, NULL, NULL);
        CreateWindowW(L"Static", L"锐化(0~5):", WS_CHILD | WS_VISIBLE, 410, y, 60, 20, hwnd, NULL, NULL, NULL);
        hEditSharpen = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 470, y, 50, 22, hwnd, (HMENU)23, NULL, NULL);
        CreateWindowW(L"Static", L"倍速:", WS_CHILD | WS_VISIBLE, 530, y, 40, 20, hwnd, NULL, NULL, NULL);
        hEditSpeed = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 570, y, 50, 22, hwnd, (HMENU)7, NULL, NULL);
        y += 40;

        // ============ 第8行：时间裁剪 ============
        CreateWindowW(L"Static", L"起始时间:", WS_CHILD | WS_VISIBLE, 20, y, 60, 20, hwnd, NULL, NULL, NULL);
        hEditStart = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 80, y, 100, 22, hwnd, (HMENU)8, NULL, NULL);
        CreateWindowW(L"Static", L"持续时长:", WS_CHILD | WS_VISIBLE, 190, y, 60, 20, hwnd, NULL, NULL, NULL);
        hEditDuration = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 250, y, 100, 22, hwnd, (HMENU)9, NULL, NULL);
        y += 40;

        // ============ 第9行：开始按钮 ============
        hBtnConvert = CreateWindowW(L"Button", L"开始转换", WS_CHILD | WS_VISIBLE, 20, y, 100, 30, hwnd, (HMENU)10, NULL, NULL);
        hStatus = CreateWindowW(L"Static", L"状态：就绪", WS_CHILD | WS_VISIBLE, 140, y + 5, 600, 25, hwnd, (HMENU)11, NULL, NULL);

        EnableWindow(hComboFormat, FALSE);
        break;
    }

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        if (hListFiles == NULL) return 0;

        int listH = h - 500;
        if (listH < 150) listH = 150;
        int yOffset = listH - 250;

        // 列表框宽度自适应
        MoveWindow(hListFiles, 20, 45, w - 40, listH, TRUE);

        // 输出目录行
        MoveWindow(hEditDst, 80, 335 + yOffset, w - 280, 22, TRUE);
        MoveWindow(hBtnSelectDst, w - 180, 335 + yOffset, 100, 22, TRUE);

        // 格式/预设/CRF
        MoveWindow(hComboFormat, 80, 375 + yOffset, 90, 200, TRUE);
        MoveWindow(hComboPreset, 250, 375 + yOffset, 90, 200, TRUE);
        MoveWindow(hComboCRF, 420, 375 + yOffset, 90, 200, TRUE);

        // 分辨率/帧率/音频码率
        MoveWindow(hComboRes, 80, 405 + yOffset, 90, 200, TRUE);
        MoveWindow(hComboFPS, 250, 405 + yOffset, 90, 200, TRUE);
        MoveWindow(hComboAudioBitrate, 420, 405 + yOffset, 90, 200, TRUE);

        // 采样率/声道/音量
        MoveWindow(hComboAudioRate, 80, 435 + yOffset, 90, 200, TRUE);
        MoveWindow(hComboChannels, 250, 435 + yOffset, 90, 200, TRUE);
        MoveWindow(hEditVolume, 420, 435 + yOffset, 90, 22, TRUE);

        // 颜色滤镜
        MoveWindow(hEditBrightness, 90, 475 + yOffset, 50, 22, TRUE);
        MoveWindow(hEditContrast, 220, 475 + yOffset, 50, 22, TRUE);
        MoveWindow(hEditSaturation, 350, 475 + yOffset, 50, 22, TRUE);
        MoveWindow(hEditSharpen, 470, 475 + yOffset, 50, 22, TRUE);
        MoveWindow(hEditSpeed, 570, 475 + yOffset, 50, 22, TRUE);

        // 时间裁剪
        MoveWindow(hEditStart, 80, 515 + yOffset, 100, 22, TRUE);
        MoveWindow(hEditDuration, 250, 515 + yOffset, 100, 22, TRUE);

        // 按钮 + 状态
        MoveWindow(hBtnConvert, 20, 555 + yOffset, 100, 30, TRUE);
        MoveWindow(hStatus, 140, 560 + yOffset, w - 160, 25, TRUE);
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);

        if (wmId == 5 && HIWORD(wParam) == CBN_SELCHANGE) UpdateControlStates(hwnd);
        if (wmId == 33 && HIWORD(wParam) == LBN_SELCHANGE) UpdateFormatCombo(hwnd);

        // ---- 添加文件 ----
        if (wmId == 2) {
            OPENFILENAMEW ofn = { 0 };
            wchar_t szFile[26000] = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
            ofn.lpstrFilter = L"所有文件\0*.*\0视频文件\0*.mp4;*.avi;*.mkv;*.mov;*.webm\0音频文件\0*.mp3;*.wav;*.aac;*.flac;*.m4a\0";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
            if (GetOpenFileNameW(&ofn) == TRUE) {
                wchar_t* p = szFile;
                std::wstring dir = p; p += dir.length() + 1;
                if (*p == L'\0') g_fileList.push_back(dir);
                else { while (*p) { g_fileList.push_back(dir + L"\\" + p); p += wcslen(p) + 1; } }
                RefreshListBox();
                UpdateFormatCombo(hwnd);
                SetWindowTextW(hStatus, L"状态：文件已加入队列");
            }
        }
        // ---- 添加文件夹 ----
        else if (wmId == 30) {
            BROWSEINFOW bi = { 0 };
            bi.hwndOwner = hwnd;
            bi.lpszTitle = L"请选择包含音视频的文件夹";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl != NULL) {
                wchar_t path[260];
                if (SHGetPathFromIDListW(pidl, path)) {
                    std::vector<std::wstring> foundFiles;
                    ScanFolder(path, foundFiles);
                    if (foundFiles.empty()) {
                        MessageBoxW(hwnd, L"未找到支持的音视频文件。", L"提示", MB_OK | MB_ICONWARNING);
                    } else {
                        for (const auto& f : foundFiles) g_fileList.push_back(f);
                        RefreshListBox();
                        UpdateFormatCombo(hwnd);
                        MessageBoxW(hwnd, (L"扫描完成！\n已添加 " + std::to_wstring(foundFiles.size()) + L" 个文件到队列。").c_str(), L"提示", MB_OK | MB_ICONINFORMATION);
                    }
                }
                CoTaskMemFree(pidl);
            }
        }
        // ---- 清空列表 ----
        else if (wmId == 40) {
            g_fileList.clear();
            RefreshListBox();
            UpdateFormatCombo(hwnd);
            SetWindowTextW(hStatus, L"状态：列表已清空");
        }
        // ---- 移除选中 ----
        else if (wmId == 41) {
            int count = SendMessageW(hListFiles, LB_GETSELCOUNT, 0, 0);
            if (count > 0) {
                int* indices = new int[count];
                SendMessageW(hListFiles, LB_GETSELITEMS, count, (LPARAM)indices);
                for (int i = count - 1; i >= 0; --i)
                    g_fileList.erase(g_fileList.begin() + indices[i]);
                delete[] indices;
                RefreshListBox();
                UpdateFormatCombo(hwnd);
            }
        }
        // ---- 过滤并全选 ----
        else if (wmId == 51) {
            int fmtIdx = SendMessageW(hComboFilterFormat, CB_GETCURSEL, 0, 0);
            if (fmtIdx < 0) { MessageBoxW(hwnd, L"当前列表中没有可过滤的格式！", L"提示", MB_OK | MB_ICONWARNING); break; }
            wchar_t fmtBuf[50] = { 0 };
            SendMessageW(hComboFilterFormat, CB_GETLBTEXT, fmtIdx, (LPARAM)fmtBuf);
            std::wstring targetExt(fmtBuf);
            for (auto &c : targetExt) c = towlower(c);

            std::vector<std::wstring> newList;
            for (const auto& f : g_fileList) {
                size_t dot = f.find_last_of(L'.');
                if (dot != std::wstring::npos) {
                    std::wstring ext = f.substr(dot);
                    for (auto &c : ext) c = towlower(c);
                    if (ext == targetExt) newList.push_back(f);
                }
            }
            if (newList.empty()) { MessageBoxW(hwnd, L"过滤后没有剩余文件！", L"提示", MB_OK | MB_ICONWARNING); break; }
            g_fileList = newList;
            RefreshListBox();

            int count = SendMessageW(hListFiles, LB_GETCOUNT, 0, 0);
            if (count > 0) {
                SendMessageW(hListFiles, LB_SETSEL, TRUE, -1);
                SendMessageW(hListFiles, LB_SELITEMRANGE, TRUE, MAKELPARAM(0, count - 1));
            }
            UpdateFormatCombo(hwnd);
            std::wstring status = L"状态：已过滤出 " + std::to_wstring(count) + L" 个 " + targetExt + L" 文件";
            SetWindowTextW(hStatus, status.c_str());
        }
        // ---- 选择输出目录 ----
        else if (wmId == 4) {
            BROWSEINFOW bi = { 0 };
            bi.hwndOwner = hwnd;
            bi.lpszTitle = L"请选择输出文件夹";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl != NULL) {
                wchar_t path[260];
                if (SHGetPathFromIDListW(pidl, path)) SetWindowTextW(hEditDst, path);
                CoTaskMemFree(pidl);
            }
        }
        // ---- 开始转换 ----
        else if (wmId == 10) {
            if (g_fileList.empty()) { MessageBoxW(hwnd, L"待转换队列为空！", L"提示", MB_OK | MB_ICONWARNING); break; }
            wchar_t dstDir[260];
            GetWindowTextW(hEditDst, dstDir, 260);
            if (wcslen(dstDir) == 0) { MessageBoxW(hwnd, L"请选择输出目录！", L"提示", MB_OK | MB_ICONWARNING); break; }

            int fmtIdx = SendMessageW(hComboFormat, CB_GETCURSEL, 0, 0);
            if (fmtIdx < 0) { MessageBoxW(hwnd, L"请选择输出格式！", L"提示", MB_OK); break; }

            wchar_t fmtBuf[50] = { 0 };
            SendMessageW(hComboFormat, CB_GETLBTEXT, fmtIdx, (LPARAM)fmtBuf);
            std::wstring fmtStr(fmtBuf);
            std::wstring targetExt = L".mp4";
            if (fmtStr == L"MP4 (视频)")       targetExt = L".mp4";
            else if (fmtStr == L"MKV (视频)")  targetExt = L".mkv";
            else if (fmtStr == L"AVI (视频)")  targetExt = L".avi";
            else if (fmtStr == L"MP3 (音频)")  targetExt = L".mp3";
            else if (fmtStr == L"WAV (音频)")  targetExt = L".wav";
            else if (fmtStr == L"AAC (音频)")  targetExt = L".aac";
            else if (fmtStr == L"FLAC (音频)") targetExt = L".flac";

            int presetIdx        = SendMessageW(hComboPreset, CB_GETCURSEL, 0, 0);
            int crfIdx           = SendMessageW(hComboCRF, CB_GETCURSEL, 0, 0);
            int audioBitrateIdx  = SendMessageW(hComboAudioBitrate, CB_GETCURSEL, 0, 0);
            int fpsIdx           = SendMessageW(hComboFPS, CB_GETCURSEL, 0, 0);
            int resIdx           = SendMessageW(hComboRes, CB_GETCURSEL, 0, 0);
            int audioRateIdx     = SendMessageW(hComboAudioRate, CB_GETCURSEL, 0, 0);
            int channelsIdx      = SendMessageW(hComboChannels, CB_GETCURSEL, 0, 0);

            wchar_t startTime[50] = {0}, duration[50] = {0}, speed[20] = {0};
            wchar_t bright[20] = {0}, contrast[20] = {0}, sat[20] = {0};
            wchar_t sharpen[20] = {0}, volume[20] = {0};
            GetWindowTextW(hEditStart, startTime, 50);
            GetWindowTextW(hEditDuration, duration, 50);
            GetWindowTextW(hEditSpeed, speed, 20);
            GetWindowTextW(hEditBrightness, bright, 20);
            GetWindowTextW(hEditContrast, contrast, 20);
            GetWindowTextW(hEditSaturation, sat, 20);
            GetWindowTextW(hEditSharpen, sharpen, 20);
            GetWindowTextW(hEditVolume, volume, 20);

            std::wstring wsStart(startTime), wsDur(duration), wsSpeed(speed);
            std::wstring wsBright(bright), wsContrast(contrast), wsSat(sat);
            std::wstring wsSharpen(sharpen), wsVolume(volume);
            std::vector<std::wstring> filesToConvert = g_fileList;
            std::wstring dstDirStr(dstDir);

            SetWindowTextW(hStatus, L"状态：正在批量转换...");
            EnableWindow(hBtnConvert, FALSE);

            std::thread([hwnd, filesToConvert, dstDirStr, targetExt, presetIdx, crfIdx,
                         audioBitrateIdx, fpsIdx, resIdx, audioRateIdx, channelsIdx,
                         wsStart, wsDur, wsSpeed, wsBright, wsContrast, wsSat, wsSharpen, wsVolume]() {
                int success = 0, skipped = 0;
                bool isAudioOutput = (targetExt == L".mp3" || targetExt == L".wav"
                                   || targetExt == L".aac" || targetExt == L".flac");

                for (size_t i = 0; i < filesToConvert.size(); ++i) {
                    std::wstring srcPath = filesToConvert[i];
                    size_t dot = srcPath.find_last_of(L'.');
                    std::wstring srcExt = (dot != std::wstring::npos) ? srcPath.substr(dot) : L"";
                    for (auto &c : srcExt) c = towlower(c);
                    bool isSrcVideo = (srcExt == L".mp4" || srcExt == L".avi" || srcExt == L".mkv"
                                     || srcExt == L".mov" || srcExt == L".webm");
                    if (!isSrcVideo && !isAudioOutput) { skipped++; continue; }

                    std::wstring* pMsg = new std::wstring(
                        L"状态：正在转换 (" + std::to_wstring(i + 1) + L"/"
                        + std::to_wstring(filesToConvert.size()) + L")..."
                    );
                    PostMessageW(hwnd, WM_APP_PROGRESS, 0, (LPARAM)pMsg);

                    ConvertOneFile(srcPath, dstDirStr, targetExt, isAudioOutput,
                                   presetIdx, crfIdx, audioBitrateIdx, fpsIdx,
                                   resIdx, audioRateIdx, channelsIdx,
                                   wsStart, wsDur, wsSpeed, wsBright,
                                   wsContrast, wsSat, wsSharpen, wsVolume);
                    success++;
                }
                PostMessageW(hwnd, WM_APP_FINISHED, (WPARAM)success, (LPARAM)skipped);
            }).detach();
        }
        break;
    }

    // 后台线程进度消息
    case WM_APP_PROGRESS: {
        std::wstring* pMsg = (std::wstring*)lParam;
        if (pMsg) { SetWindowTextW(hStatus, pMsg->c_str()); delete pMsg; }
        return 0;
    }
    // 后台线程完成消息
    case WM_APP_FINISHED: {
        int success = (int)wParam;
        int skipped = (int)lParam;
        EnableWindow(hBtnConvert, TRUE);
        std::wstring finalMsg = L"批量转换完成！\n成功: " + std::to_wstring(success)
                              + L" / " + std::to_wstring(g_fileList.size());
        if (skipped > 0) finalMsg += L"\n跳过（音频无法转视频）: " + std::to_wstring(skipped);
        MessageBoxW(hwnd, finalMsg.c_str(), L"完成", MB_OK | MB_ICONINFORMATION);
        SetWindowTextW(hStatus, L"状态：批量转换完成！");
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FFmpegGUIProMaxBatch";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"FFmpegGUIProMaxBatch", L"音视频批量转换与编辑工具",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 820, 750,
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