using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows.Forms;

namespace MediaConverterApp
{
    // ============================================================
    // 国际化字符串表（按系统 UI 语言自动选择）
    // ============================================================
    public static class Strings
    {
        private static readonly bool IsEnglish;

        static Strings()
        {
            string lang = CultureInfo.CurrentUICulture.TwoLetterISOLanguageName;
            IsEnglish = !string.Equals(lang, "zh", StringComparison.OrdinalIgnoreCase);
        }

        private static readonly Dictionary<string, string> Zh = new Dictionary<string, string>
        {
            { "WindowTitle",         "音视频批量转换与编辑工具" },
            { "QueueLabel",          "待转换队列:" },
            { "FilterLabel",         "按格式:" },
            { "OutputDirLabel",      "输出目录:" },
            { "OutputFormatLabel",   "输出格式:" },
            { "PresetLabel",         "编码预设:" },
            { "CrfLabel",            "画质(CRF):" },
            { "ResolutionLabel",     "分辨率:" },
            { "FpsLabel",            "帧率(FPS):" },
            { "AudioBitrateLabel",   "音频码率:" },
            { "AudioRateLabel",      "采样率:" },
            { "ChannelsLabel",       "声道:" },
            { "VolumeLabel",         "音量倍数:" },
            { "BrightnessLabel",     "亮度(-1~1):" },
            { "ContrastLabel",       "对比度(0~3):" },
            { "SaturationLabel",     "饱和度(0~3):" },
            { "SharpenLabel",        "锐化(0~5):" },
            { "SpeedLabel",          "倍速:" },
            { "StartTimeLabel",      "起始时间:" },
            { "DurationLabel",       "持续时长:" },

            { "BtnAddFiles",         "添加文件" },
            { "BtnAddFolder",        "添加文件夹" },
            { "BtnRemove",           "移除选中" },
            { "BtnClear",            "清空列表" },
            { "BtnFilter",           "过滤并全选" },
            { "BtnSelectOutput",     "选择输出目录" },
            { "BtnOpenOutput",       "打开输出目录" },
            { "BtnStart",            "开始转换" },

            { "PresetMedium",        "medium(默认)" },
            { "PresetFast",          "fast(快速)" },
            { "PresetSlow",          "slow(慢速)" },

            { "CrfHigh",             "18(高画质)" },
            { "CrfBalanced",         "23(均衡)" },
            { "CrfCompress",         "28(压缩优先)" },

            { "ResKeep",             "保持原样" },
            { "FpsKeep",             "保持原样" },
            { "AudioRateKeep",       "保持原样" },
            { "ChannelsKeep",        "保持原样" },

            { "ChStereo",            "立体声(2)" },
            { "ChMono",              "单声道(1)" },

            { "FmtMp4",              "MP4 (视频)" },
            { "FmtMkv",              "MKV (视频)" },
            { "FmtAvi",              "AVI (视频)" },
            { "FmtMp3",              "MP3 (音频)" },
            { "FmtWav",              "WAV (音频)" },
            { "FmtAac",              "AAC (音频)" },
            { "FmtFlac",             "FLAC (音频)" },
            { "FmtCopySuffix",       " (仅换容器)" },

            { "StatusReady",         "状态：就绪" },
            { "StatusFilesAdded",    "状态：文件已加入队列" },
            { "StatusListCleared",   "状态：列表已清空" },
            { "StatusConverting",    "状态：正在批量转换..." },
            { "StatusCopyConverting","状态：仅换容器转换中..." },
            { "StatusDone",          "状态：批量转换完成！" },
            { "StatusDoneFail",      "状态：完成，失败 {0} 个" },
            { "StatusFiltered",      "状态：已过滤出 {0} 个 {1} 文件" },

            { "MsgQueueEmpty",       "待转换队列为空！" },
            { "MsgNoOutputDir",      "请选择输出目录！" },
            { "MsgNoFormat",         "请选择输出格式！" },
            { "MsgScanDone",         "扫描完成！\n本次新增 {0} 个文件。\n当前队列共 {1} 个。" },
            { "MsgScanEmpty",        "未找到支持的音视频文件。" },
            { "MsgFilterNothing",    "过滤后没有剩余文件！" },
            { "MsgFilterEmpty",      "当前列表中没有可过滤的格式！" },
            { "MsgConvertDone",      "批量转换完成！\n成功: {0} / {1}\n输出目录: {2}\n输出格式: {3}" },
            { "MsgFfmpegWarn",       "警告：未找到 ffmpeg.exe！\n请把 ffmpeg.exe 放到程序目录下。" },
            { "MsgFfmpegMissing",    "未找到 ffmpeg.exe！\n\n请下载 ffmpeg 并放到程序目录下。" },
            { "MsgBusy",             "正在转换中，请稍候..." },
            { "MsgNoOutputFiles",    "还没有输出目录。" },

            { "TitleWarn",           "警告" },
            { "TitleError",          "错误" },
            { "TitleDone",           "完成" },
            { "TitleTip",            "提示" },

            { "DlgPickFolder",       "请选择包含音视频的文件夹" },
            { "DlgPickOutput",       "请选择输出文件夹" },
            { "DlgFilterAll",        "所有文件" },
            { "DlgFilterVideo",      "视频文件" },
            { "DlgFilterAudio",      "音频文件" },
        };

        private static readonly Dictionary<string, string> En = new Dictionary<string, string>
        {
            { "WindowTitle",         "Batch Audio/Video Converter" },
            { "QueueLabel",          "Queue:" },
            { "FilterLabel",         "Filter:" },
            { "OutputDirLabel",      "Output dir:" },
            { "OutputFormatLabel",   "Output format:" },
            { "PresetLabel",         "Preset:" },
            { "CrfLabel",            "Quality(CRF):" },
            { "ResolutionLabel",     "Resolution:" },
            { "FpsLabel",            "FPS:" },
            { "AudioBitrateLabel",   "Audio bitrate:" },
            { "AudioRateLabel",      "Sample rate:" },
            { "ChannelsLabel",       "Channels:" },
            { "VolumeLabel",         "Volume:" },
            { "BrightnessLabel",     "Brightness(-1~1):" },
            { "ContrastLabel",       "Contrast(0~3):" },
            { "SaturationLabel",     "Saturation(0~3):" },
            { "SharpenLabel",        "Sharpen(0~5):" },
            { "SpeedLabel",          "Speed:" },
            { "StartTimeLabel",      "Start:" },
            { "DurationLabel",       "Duration:" },

            { "BtnAddFiles",         "Add Files" },
            { "BtnAddFolder",        "Add Folder" },
            { "BtnRemove",           "Remove" },
            { "BtnClear",            "Clear" },
            { "BtnFilter",           "Filter && Select" },
            { "BtnSelectOutput",     "Select Output" },
            { "BtnOpenOutput",       "Open Output" },
            { "BtnStart",            "Start" },

            { "PresetMedium",        "medium(default)" },
            { "PresetFast",          "fast" },
            { "PresetSlow",          "slow" },

            { "CrfHigh",             "18(high)" },
            { "CrfBalanced",         "23(balanced)" },
            { "CrfCompress",         "28(compress)" },

            { "ResKeep",             "Keep original" },
            { "FpsKeep",             "Keep original" },
            { "AudioRateKeep",       "Keep original" },
            { "ChannelsKeep",        "Keep original" },

            { "ChStereo",            "Stereo(2)" },
            { "ChMono",              "Mono(1)" },

            { "FmtMp4",              "MP4 (Video)" },
            { "FmtMkv",              "MKV (Video)" },
            { "FmtAvi",              "AVI (Video)" },
            { "FmtMp3",              "MP3 (Audio)" },
            { "FmtWav",              "WAV (Audio)" },
            { "FmtAac",              "AAC (Audio)" },
            { "FmtFlac",             "FLAC (Audio)" },
            { "FmtCopySuffix",       " (copy)" },

            { "StatusReady",         "Ready" },
            { "StatusFilesAdded",    "Files added to queue" },
            { "StatusListCleared",   "List cleared" },
            { "StatusConverting",    "Converting..." },
            { "StatusCopyConverting","Stream copy in progress..." },
            { "StatusDone",          "All done!" },
            { "StatusDoneFail",      "Done, {0} failed" },
            { "StatusFiltered",      "Filtered: {0} {1} file(s)" },

            { "MsgQueueEmpty",       "Queue is empty!" },
            { "MsgNoOutputDir",      "Please select an output directory!" },
            { "MsgNoFormat",         "Please select an output format!" },
            { "MsgScanDone",         "Scan complete!\nAdded: {0}\nTotal in queue: {1}" },
            { "MsgScanEmpty",        "No supported audio/video files found." },
            { "MsgFilterNothing",    "No files left after filtering!" },
            { "MsgFilterEmpty",      "Nothing to filter by!" },
            { "MsgConvertDone",      "Conversion complete!\nSuccess: {0} / {1}\nOutput dir: {2}\nOutput format: {3}" },
            { "MsgFfmpegWarn",       "Warning: ffmpeg.exe not found!\nPlease put ffmpeg.exe next to this program." },
            { "MsgFfmpegMissing",    "ffmpeg.exe not found!\n\nPlease download ffmpeg and put it next to this program." },
            { "MsgBusy",             "Conversion in progress, please wait..." },
            { "MsgNoOutputFiles",    "No output directory set." },

            { "TitleWarn",           "Warning" },
            { "TitleError",          "Error" },
            { "TitleDone",           "Done" },
            { "TitleTip",            "Notice" },

            { "DlgPickFolder",       "Select a folder with media files" },
            { "DlgPickOutput",       "Select output folder" },
            { "DlgFilterAll",        "All Files" },
            { "DlgFilterVideo",      "Video Files" },
            { "DlgFilterAudio",      "Audio Files" },
        };

        public static string Get(string key)
        {
            Dictionary<string, string> primary = IsEnglish ? En : Zh;
            Dictionary<string, string> fallback = IsEnglish ? Zh : En;
            string v;
            if (primary.TryGetValue(key, out v)) return v;
            if (fallback.TryGetValue(key, out v)) return v;
            return key;
        }

        public static string Fmt(string key, params object[] args)
        {
            return string.Format(Get(key), args);
        }
    }

    // ============================================================
    // 数据模型
    // ============================================================
    public enum FmtCode
    {
        Mp4 = 1, Mkv, Avi,
        Mp3, Wav, Aac, Flac,
        Mp4Copy, MkvCopy, AviCopy
    }

    public class ConversionOptions
    {
        public string InputPath;
        public string OutputPath;
        public string TargetExt;
        public bool SourceHasAudio = true;
        public bool IsAudioOutput;
        public bool IsStreamCopy;

        public string StartTime;
        public string Duration;

        public string Preset = "medium";
        public int Crf = 23;
        public string Resolution;
        public int Fps;
        public double Brightness;
        public double Contrast = 1;
        public double Saturation = 1;
        public double Sharpen;

        public string AudioBitrate;
        public int AudioSampleRate;
        public int AudioChannels;
        public double Volume = 1;

        public double Speed = 1;
    }

    public class MediaInfo
    {
        public bool HasVideo;
        public bool HasAudio;
        public string VideoCodec = "";
        public string AudioCodec = "";
        public bool Valid;
    }

    public class ConvertResult
    {
        public bool Success;
        public string Error = "";
    }

    // ============================================================
    // FFmpeg 工具
    // ============================================================
    public static class FfmpegUtil
    {
        private static string _ffmpegPath;

        public static string GetPath()
        {
            if (_ffmpegPath != null) return _ffmpegPath;

            string exeDir = Path.GetDirectoryName(Application.ExecutablePath);
            string local = Path.Combine(exeDir, "ffmpeg.exe");
            if (File.Exists(local)) { _ffmpegPath = local; return _ffmpegPath; }

            string pathEnv = Environment.GetEnvironmentVariable("PATH");
            if (pathEnv != null)
            {
                foreach (string dir in pathEnv.Split(';'))
                {
                    if (string.IsNullOrWhiteSpace(dir)) continue;
                    try
                    {
                        string full = Path.Combine(dir.Trim(), "ffmpeg.exe");
                        if (File.Exists(full)) { _ffmpegPath = full; return _ffmpegPath; }
                    }
                    catch { }
                }
            }
            _ffmpegPath = "ffmpeg.exe";
            return _ffmpegPath;
        }

        public static bool Exists()
        {
            string p = GetPath();
            return File.Exists(p) || p == "ffmpeg.exe";
        }

        // 运行 ffmpeg，返回退出码 + stderr
        public static ConvertResult Run(string args)
        {
            ConvertResult result = new ConvertResult();
            try
            {
                ProcessStartInfo psi = new ProcessStartInfo();
                psi.FileName = GetPath();
                psi.Arguments = args;
                psi.UseShellExecute = false;
                psi.CreateNoWindow = true;
                psi.RedirectStandardError = true;
                psi.RedirectStandardOutput = true;

                using (Process p = new Process())
                {
                    p.StartInfo = psi;
                    p.Start();

                    string stderr = p.StandardError.ReadToEnd();
                    string stdout = p.StandardOutput.ReadToEnd();
                    p.WaitForExit();

                    result.Success = (p.ExitCode == 0);
                    result.Error = stderr;
                }
            }
            catch (Exception ex)
            {
                result.Success = false;
                result.Error = ex.Message;
            }
            return result;
        }

        public static MediaInfo Probe(string inputPath)
        {
            MediaInfo info = new MediaInfo();
            ConvertResult r = Run("-hide_banner -i \"" + inputPath + "\"");
            string s = r.Error;   // ffmpeg 把探测信息写到 stderr

            int vpos = s.IndexOf("Video: ", StringComparison.Ordinal);
            if (vpos >= 0)
            {
                int start = vpos + 7;
                int end = IndexOfAny(s, start, new char[] { ' ', ',', '(' });
                if (end > start)
                {
                    info.VideoCodec = s.Substring(start, end - start).ToLower();
                    info.HasVideo = true;
                }
            }
            int apos = s.IndexOf("Audio: ", StringComparison.Ordinal);
            if (apos >= 0)
            {
                int start = apos + 7;
                int end = IndexOfAny(s, start, new char[] { ' ', ',', '(' });
                if (end > start)
                {
                    info.AudioCodec = s.Substring(start, end - start).ToLower();
                    info.HasAudio = true;
                }
            }
            info.Valid = info.HasVideo || info.HasAudio;
            return info;
        }

        private static int IndexOfAny(string s, int start, char[] chars)
        {
            for (int i = start; i < s.Length; ++i)
                for (int j = 0; j < chars.Length; ++j)
                    if (s[i] == chars[j]) return i;
            return -1;
        }

        public static bool ContainerSupportsVideo(string container, string codec)
        {
            switch (container)
            {
                case ".mkv":
                    return codec == "h264" || codec == "hevc" || codec == "h265" ||
                           codec == "mpeg4" || codec == "vp8" || codec == "vp9" ||
                           codec == "av1" || codec == "mpeg2video" || codec == "vc1";
                case ".mp4":
                    return codec == "h264" || codec == "hevc" || codec == "h265" ||
                           codec == "mpeg4" || codec == "av1" || codec == "vp9";
                case ".avi":
                    return codec == "h264" || codec == "mpeg4" || codec == "mjpeg";
                case ".webm":
                    return codec == "vp8" || codec == "vp9" || codec == "av1";
            }
            return false;
        }

        public static bool ContainerSupportsAudio(string container, string codec)
        {
            switch (container)
            {
                case ".mkv":
                    return codec == "aac" || codec == "mp3" || codec == "opus" ||
                           codec == "vorbis" || codec == "flac" || codec == "ac3" ||
                           codec == "dts" || codec == "eac3" || codec == "pcm_s16le";
                case ".mp4":
                    return codec == "aac" || codec == "mp3" || codec == "ac3" ||
                           codec == "eac3" || codec == "alac";
                case ".avi":
                    return codec == "mp3" || codec == "ac3" || codec == "pcm_s16le";
                case ".webm":
                    return codec == "vorbis" || codec == "opus";
            }
            return false;
        }

        public static bool CanStreamCopy(MediaInfo info, string dstExt)
        {
            if (!info.Valid) return false;
            if (!info.HasVideo) return false;
            if (!ContainerSupportsVideo(dstExt, info.VideoCodec)) return false;
            if (info.HasAudio && !ContainerSupportsAudio(dstExt, info.AudioCodec)) return false;
            return true;
        }

        public static string Num(double v)
        {
            return v.ToString("0.###", CultureInfo.InvariantCulture);
        }

        public static string Quote(string s)
        {
            return "\"" + s.Replace("\"", "\\\"") + "\"";
        }

        // 构建 ffmpeg 参数
        public static string BuildArgs(ConversionOptions o)
        {
            StringBuilder sb = new StringBuilder();
            sb.Append("-hide_banner -i ").Append(Quote(o.InputPath)).Append(' ');

            if (!string.IsNullOrWhiteSpace(o.StartTime))
                sb.Append("-ss ").Append(Quote(o.StartTime.Trim())).Append(' ');
            if (!string.IsNullOrWhiteSpace(o.Duration))
                sb.Append("-t ").Append(Quote(o.Duration.Trim())).Append(' ');

            // ---- 仅换容器 ----
            if (o.IsStreamCopy)
            {
                sb.Append("-map 0:v? -map 0:a? -c copy ");
                if (o.TargetExt == ".mp4") sb.Append("-movflags +faststart ");
                sb.Append(Quote(o.OutputPath)).Append(" -y");
                return sb.ToString();
            }

            bool applySpeed = (o.Speed >= 0.5 && o.Speed <= 4.0 &&
                               Math.Abs(o.Speed - 1.0) > 0.001);

            // ---- 纯音频 ----
            if (o.IsAudioOutput)
            {
                sb.Append("-vn ");
                if (o.TargetExt == ".mp3")       sb.Append("-c:a libmp3lame ");
                else if (o.TargetExt == ".wav")  sb.Append("-c:a pcm_s16le ");
                else if (o.TargetExt == ".aac")  sb.Append("-c:a aac ");
                else if (o.TargetExt == ".flac") sb.Append("-c:a flac ");

                List<string> af = new List<string>();
                if (Math.Abs(o.Volume - 1.0) > 0.001)
                    af.Add("volume=" + Num(o.Volume));
                if (applySpeed)
                    af.AddRange(BuildAtempoChain(o.Speed));
                if (af.Count > 0)
                    sb.Append("-af ").Append(Quote(string.Join(",", af.ToArray()))).Append(' ');

                if (!string.IsNullOrEmpty(o.AudioBitrate))
                    sb.Append("-b:a ").Append(o.AudioBitrate).Append(' ');
                if (o.AudioSampleRate > 0)
                    sb.Append("-ar ").Append(o.AudioSampleRate).Append(' ');
                if (o.AudioChannels > 0)
                    sb.Append("-ac ").Append(o.AudioChannels).Append(' ');

                sb.Append(Quote(o.OutputPath)).Append(" -y");
                return sb.ToString();
            }

            // ---- 视频重编码 ----
            sb.Append("-c:v libx264 ");
            sb.Append("-preset ").Append(o.Preset).Append(' ');
            sb.Append("-crf ").Append(o.Crf).Append(' ');
            sb.Append("-pix_fmt yuv420p ");
            if (!string.IsNullOrEmpty(o.Resolution))
                sb.Append("-s ").Append(o.Resolution).Append(' ');
            if (o.Fps > 0)
                sb.Append("-r ").Append(o.Fps).Append(' ');

            List<string> vf = new List<string>();
            if (Math.Abs(o.Brightness) > 0.001 ||
                Math.Abs(o.Contrast - 1) > 0.001 ||
                Math.Abs(o.Saturation - 1) > 0.001)
            {
                List<string> eq = new List<string>();
                if (Math.Abs(o.Brightness) > 0.001) eq.Add("brightness=" + Num(o.Brightness));
                if (Math.Abs(o.Contrast - 1) > 0.001) eq.Add("contrast=" + Num(o.Contrast));
                if (Math.Abs(o.Saturation - 1) > 0.001) eq.Add("saturation=" + Num(o.Saturation));
                vf.Add("eq=" + string.Join(":", eq.ToArray()));
            }
            if (o.Sharpen > 0.001) vf.Add("unsharp=5:5:" + Num(o.Sharpen));

            List<string> af2 = new List<string>();
            if (o.SourceHasAudio && Math.Abs(o.Volume - 1.0) > 0.001)
                af2.Add("volume=" + Num(o.Volume));

            if (applySpeed)
            {
                List<string> parts = new List<string>();
                List<string> vfCopy = new List<string>(vf);
                vfCopy.Add("setpts=PTS/" + Num(o.Speed));
                parts.Add("[0:v]" + string.Join(",", vfCopy.ToArray()) + "[v]");

                if (o.SourceHasAudio)
                {
                    List<string> afCopy = new List<string>(af2);
                    afCopy.AddRange(BuildAtempoChain(o.Speed));
                    parts.Add("[0:a]" + string.Join(",", afCopy.ToArray()) + "[a]");
                    sb.Append("-filter_complex ")
                      .Append(Quote(string.Join(";", parts.ToArray())))
                      .Append(" -map \"[v]\" -map \"[a]\" ");
                }
                else
                {
                    sb.Append("-filter_complex ")
                      .Append(Quote(string.Join(";", parts.ToArray())))
                      .Append(" -map \"[v]\" ");
                }
            }
            else
            {
                if (vf.Count > 0) sb.Append("-vf ").Append(Quote(string.Join(",", vf.ToArray()))).Append(' ');
                if (af2.Count > 0) sb.Append("-af ").Append(Quote(string.Join(",", af2.ToArray()))).Append(' ');
            }

            if (o.SourceHasAudio)
            {
                sb.Append(o.TargetExt == ".avi" ? "-c:a libmp3lame " : "-c:a aac ");
                if (!string.IsNullOrEmpty(o.AudioBitrate))
                    sb.Append("-b:a ").Append(o.AudioBitrate).Append(' ');
                if (o.AudioSampleRate > 0)
                    sb.Append("-ar ").Append(o.AudioSampleRate).Append(' ');
                if (o.AudioChannels > 0)
                    sb.Append("-ac ").Append(o.AudioChannels).Append(' ');
            }
            else sb.Append("-an ");

            if (o.TargetExt == ".mp4") sb.Append("-movflags +faststart ");
            sb.Append(Quote(o.OutputPath)).Append(" -y");
            return sb.ToString();
        }

        private static List<string> BuildAtempoChain(double speed)
        {
            List<string> chain = new List<string>();
            double remaining = speed;
            while (remaining > 2.0) { chain.Add("atempo=2.0"); remaining /= 2.0; }
            while (remaining < 0.5) { chain.Add("atempo=0.5"); remaining /= 0.5; }
            chain.Add("atempo=" + Num(remaining));
            return chain;
        }
    }

    // ============================================================
    // 主窗体
    // ============================================================
    public class MainForm : Form
    {
        // 控件
        private ListBox listFiles;
        private ComboBox comboFilter, comboFormat, comboPreset, comboCrf, comboRes, comboFps;
        private ComboBox comboAudioBitrate, comboAudioRate, comboChannels;
        private TextBox txtOutputDir, txtStartTime, txtDuration, txtSpeed;
        private TextBox txtBrightness, txtContrast, txtSaturation, txtSharpen, txtVolume;
        private Button btnAddFiles, btnAddFolder, btnRemove, btnClear, btnFilter;
        private Button btnSelectOut, btnOpenOut, btnConvert;
        private Label lblStatus;

        // 数据
        private readonly List<string> fileList = new List<string>();
        private bool isConverting = false;

        public MainForm()
        {
            InitializeUI();
            if (!FfmpegUtil.Exists())
                MessageBox.Show(Strings.Get("MsgFfmpegWarn"),
                    Strings.Get("TitleWarn"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }

        private void InitializeUI()
        {
            Text = Strings.Get("WindowTitle");
            Size = new Size(820, 780);
            StartPosition = FormStartPosition.CenterScreen;
            MinimumSize = new Size(800, 720);
            Font = new Font("Microsoft YaHei UI", 9F);
            if (Font.Name != "Microsoft YaHei UI")
                Font = new Font("Segoe UI", 9F);

            int y = 15;

            // ===== 第 1 行 =====
            AddLabel(Strings.Get("QueueLabel"), 20, y, 70, 20);
            btnAddFiles = AddButton(Strings.Get("BtnAddFiles"), 90, y, 80, 22);
            btnAddFolder = AddButton(Strings.Get("BtnAddFolder"), 180, y, 90, 22);
            btnRemove = AddButton(Strings.Get("BtnRemove"), 280, y, 80, 22);
            btnClear = AddButton(Strings.Get("BtnClear"), 370, y, 80, 22);
            AddLabel(Strings.Get("FilterLabel"), 460, y, 50, 20);
            comboFilter = AddCombo(510, y, 90, 200);
            btnFilter = AddButton(Strings.Get("BtnFilter"), 610, y, 90, 22);
            comboFilter.Enabled = false;
            y += 30;

            // ===== 文件列表 =====
            listFiles = new ListBox();
            listFiles.Location = new Point(20, y);
            listFiles.Size = new Size(760, 250);
            listFiles.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            listFiles.SelectionMode = SelectionMode.MultiExtended;
            listFiles.SelectedIndexChanged += ListFiles_SelectedIndexChanged;
            Controls.Add(listFiles);
            y += 260;

            // ===== 输出目录 =====
            AddLabel(Strings.Get("OutputDirLabel"), 20, y, 60, 20);
            txtOutputDir = new TextBox();
            txtOutputDir.Location = new Point(80, y);
            txtOutputDir.Size = new Size(440, 22);
            txtOutputDir.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            Controls.Add(txtOutputDir);
            btnSelectOut = AddButton(Strings.Get("BtnSelectOutput"), 530, y, 110, 22);
            btnSelectOut.Anchor = AnchorStyles.Top | AnchorStyles.Right;
            btnOpenOut = AddButton(Strings.Get("BtnOpenOutput"), 650, y, 110, 22);
            btnOpenOut.Anchor = AnchorStyles.Top | AnchorStyles.Right;
            y += 40;

            // ===== 格式 / 预设 / CRF =====
            AddLabel(Strings.Get("OutputFormatLabel"), 20, y, 60, 20);
            comboFormat = AddCombo(80, y, 110, 260);
            comboFormat.SelectedIndexChanged += delegate { UpdateControlStates(); };

            AddLabel(Strings.Get("PresetLabel"), 200, y, 60, 20);
            comboPreset = AddCombo(260, y, 90, 200);
            comboPreset.Items.AddRange(new object[] {
                Strings.Get("PresetMedium"), Strings.Get("PresetFast"), Strings.Get("PresetSlow")
            });
            comboPreset.SelectedIndex = 0;

            AddLabel(Strings.Get("CrfLabel"), 370, y, 60, 20);
            comboCrf = AddCombo(430, y, 90, 200);
            comboCrf.Items.AddRange(new object[] {
                Strings.Get("CrfHigh"), Strings.Get("CrfBalanced"), Strings.Get("CrfCompress")
            });
            comboCrf.SelectedIndex = 1;
            y += 30;

            // ===== 分辨率 / 帧率 / 音频码率 =====
            AddLabel(Strings.Get("ResolutionLabel"), 20, y, 60, 20);
            comboRes = AddCombo(80, y, 90, 200);
            comboRes.Items.AddRange(new object[] {
                Strings.Get("ResKeep"), "1920x1080", "1280x720", "854x480"
            });
            comboRes.SelectedIndex = 0;

            AddLabel(Strings.Get("FpsLabel"), 190, y, 60, 20);
            comboFps = AddCombo(250, y, 90, 200);
            comboFps.Items.AddRange(new object[] {
                Strings.Get("FpsKeep"), "60", "30", "24"
            });
            comboFps.SelectedIndex = 0;

            AddLabel(Strings.Get("AudioBitrateLabel"), 360, y, 60, 20);
            comboAudioBitrate = AddCombo(420, y, 90, 200);
            comboAudioBitrate.Items.AddRange(new object[] {
                "Auto", "320 kbps", "192 kbps", "128 kbps"
            });
            comboAudioBitrate.SelectedIndex = 0;
            y += 30;

            // ===== 采样率 / 声道 / 音量 =====
            AddLabel(Strings.Get("AudioRateLabel"), 20, y, 60, 20);
            comboAudioRate = AddCombo(80, y, 90, 200);
            comboAudioRate.Items.AddRange(new object[] {
                Strings.Get("AudioRateKeep"), "48000 Hz", "44100 Hz"
            });
            comboAudioRate.SelectedIndex = 0;

            AddLabel(Strings.Get("ChannelsLabel"), 190, y, 60, 20);
            comboChannels = AddCombo(250, y, 90, 200);
            comboChannels.Items.AddRange(new object[] {
                Strings.Get("ChannelsKeep"), Strings.Get("ChStereo"), Strings.Get("ChMono")
            });
            comboChannels.SelectedIndex = 0;

            AddLabel(Strings.Get("VolumeLabel"), 360, y, 60, 20);
            txtVolume = AddTextBox(420, y, 90, 22);
            y += 40;

            // ===== 颜色滤镜 =====
            AddLabel(Strings.Get("BrightnessLabel"), 20, y, 70, 20);
            txtBrightness = AddTextBox(90, y, 50, 22);
            AddLabel(Strings.Get("ContrastLabel"), 150, y, 70, 20);
            txtContrast = AddTextBox(220, y, 50, 22);
            AddLabel(Strings.Get("SaturationLabel"), 280, y, 70, 20);
            txtSaturation = AddTextBox(350, y, 50, 22);
            AddLabel(Strings.Get("SharpenLabel"), 410, y, 60, 20);
            txtSharpen = AddTextBox(470, y, 50, 22);
            AddLabel(Strings.Get("SpeedLabel"), 530, y, 40, 20);
            txtSpeed = AddTextBox(570, y, 50, 22);
            y += 40;

            // ===== 时间裁剪 =====
            AddLabel(Strings.Get("StartTimeLabel"), 20, y, 60, 20);
            txtStartTime = AddTextBox(80, y, 100, 22);
            AddLabel(Strings.Get("DurationLabel"), 190, y, 60, 20);
            txtDuration = AddTextBox(250, y, 100, 22);
            y += 40;

            // ===== 按钮 / 状态 =====
            btnConvert = AddButton(Strings.Get("BtnStart"), 20, y, 100, 30);
            btnConvert.Click += BtnConvert_Click;
            lblStatus = new Label();
            lblStatus.Text = Strings.Get("StatusReady");
            lblStatus.Location = new Point(140, y + 5);
            lblStatus.Size = new Size(640, 25);
            lblStatus.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            Controls.Add(lblStatus);

            // 事件绑定
            btnAddFiles.Click += BtnAddFiles_Click;
            btnAddFolder.Click += BtnAddFolder_Click;
            btnRemove.Click += BtnRemove_Click;
            btnClear.Click += BtnClear_Click;
            btnFilter.Click += BtnFilter_Click;
            btnSelectOut.Click += BtnSelectOut_Click;
            btnOpenOut.Click += BtnOpenOut_Click;

            comboFormat.Enabled = false;
            btnOpenOut.Enabled = false;
        }

        // ===== 控件工厂 =====
        private Label AddLabel(string text, int x, int y, int w, int h)
        {
            Label l = new Label();
            l.Text = text;
            l.Location = new Point(x, y);
            l.Size = new Size(w, h);
            Controls.Add(l);
            return l;
        }
        private Button AddButton(string text, int x, int y, int w, int h)
        {
            Button b = new Button();
            b.Text = text;
            b.Location = new Point(x, y);
            b.Size = new Size(w, h);
            Controls.Add(b);
            return b;
        }
        private ComboBox AddCombo(int x, int y, int w, int h)
        {
            ComboBox c = new ComboBox();
            c.Location = new Point(x, y);
            c.Size = new Size(w, h);
            c.DropDownStyle = ComboBoxStyle.DropDownList;
            Controls.Add(c);
            return c;
        }
        private TextBox AddTextBox(int x, int y, int w, int h)
        {
            TextBox t = new TextBox();
            t.Location = new Point(x, y);
            t.Size = new Size(w, h);
            Controls.Add(t);
            return t;
        }

        // ===== 逻辑 =====
        private void UpdateControlStates()
        {
            FmtItem fi = comboFormat.SelectedItem as FmtItem;
            if (fi == null) return;
            FmtCode code = fi.Code;

            bool isAudio = (code == FmtCode.Mp3 || code == FmtCode.Wav ||
                    code == FmtCode.Aac || code == FmtCode.Flac);
            bool isCopy = (code == FmtCode.Mp4Copy || code == FmtCode.MkvCopy ||
                   code == FmtCode.AviCopy);

            bool videoEnabled = !isAudio && !isCopy;
            bool audioEnabled = !isCopy;

            comboPreset.Enabled = videoEnabled;
            comboCrf.Enabled = videoEnabled;
            comboRes.Enabled = videoEnabled;
            comboFps.Enabled = videoEnabled;
            txtBrightness.Enabled = videoEnabled;
            txtContrast.Enabled = videoEnabled;
            txtSaturation.Enabled = videoEnabled;
            txtSharpen.Enabled = videoEnabled;

            comboAudioBitrate.Enabled = audioEnabled;
            comboAudioRate.Enabled = audioEnabled;
            comboChannels.Enabled = audioEnabled;
            txtVolume.Enabled = audioEnabled;
            txtSpeed.Enabled = audioEnabled;
        }

        private static bool IsVideoExt(string ext)
        {
            return ext == ".mp4" || ext == ".avi" || ext == ".mkv" ||
                   ext == ".mov" || ext == ".webm";
        }

        private void ListFiles_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (listFiles.SelectedItems.Count == 0)
            {
                comboFormat.Items.Clear();
                comboFormat.Enabled = false;
                return;
            }
            comboFormat.Enabled = true;

            string srcPath = listFiles.SelectedItems[0].ToString();
            string ext = Path.GetExtension(srcPath).ToLower();
            bool isVideo = IsVideoExt(ext);

            MediaInfo info = FfmpegUtil.Probe(srcPath);

            comboFormat.Items.Clear();
            if (isVideo)
            {
                AddFormat(Strings.Get("FmtMp4"), FmtCode.Mp4, ext == ".mp4");
                AddFormat(Strings.Get("FmtMkv"), FmtCode.Mkv, ext == ".mkv");
                AddFormat(Strings.Get("FmtAvi"), FmtCode.Avi, ext == ".avi");

                // 仅换容器
                if (ext != ".mp4" && FfmpegUtil.CanStreamCopy(info, ".mp4"))
                    AddFormat(Strings.Get("FmtMp4") + Strings.Get("FmtCopySuffix"), FmtCode.Mp4Copy, false);
                if (ext != ".mkv" && FfmpegUtil.CanStreamCopy(info, ".mkv"))
                    AddFormat(Strings.Get("FmtMkv") + Strings.Get("FmtCopySuffix"), FmtCode.MkvCopy, false);
                if (ext != ".avi" && FfmpegUtil.CanStreamCopy(info, ".avi"))
                    AddFormat(Strings.Get("FmtAvi") + Strings.Get("FmtCopySuffix"), FmtCode.AviCopy, false);

                if (ext != ".mp3")  AddFormat(Strings.Get("FmtMp3"), FmtCode.Mp3, false);
                if (ext != ".wav")  AddFormat(Strings.Get("FmtWav"), FmtCode.Wav, false);
                if (ext != ".aac")  AddFormat(Strings.Get("FmtAac"), FmtCode.Aac, false);
                if (ext != ".flac") AddFormat(Strings.Get("FmtFlac"), FmtCode.Flac, false);
            }
            else
            {
                if (ext != ".mp3")  AddFormat(Strings.Get("FmtMp3"), FmtCode.Mp3, false);
                if (ext != ".wav")  AddFormat(Strings.Get("FmtWav"), FmtCode.Wav, false);
                if (ext != ".aac")  AddFormat(Strings.Get("FmtAac"), FmtCode.Aac, false);
                if (ext != ".flac") AddFormat(Strings.Get("FmtFlac"), FmtCode.Flac, false);
            }
            if (comboFormat.Items.Count > 0) comboFormat.SelectedIndex = 0;
            UpdateControlStates();
        }

        // 跳过 skip 为 true 的项
        private void AddFormat(string label, FmtCode code, bool skip)
        {
            if (skip) return;
            comboFormat.Items.Add(new FmtItem(label, code));
        }

        private class FmtItem
        {
            public string Label;
            public FmtCode Code;
            public FmtItem(string l, FmtCode c) { Label = l; Code = c; }
            public override string ToString() { return Label; }
        }

        private void AddToQueue(string f)
        {
            foreach (string e in fileList)
                if (string.Equals(e, f, StringComparison.OrdinalIgnoreCase)) return;
            fileList.Add(f);
        }

        private void RefreshListBox()
        {
            listFiles.Items.Clear();
            foreach (string f in fileList) listFiles.Items.Add(f);

            comboFilter.Items.Clear();
            List<string> exts = new List<string>();
            foreach (string f in fileList)
            {
                string e = Path.GetExtension(f).ToLower();
                if (!exts.Contains(e)) exts.Add(e);
            }
            exts.Sort();
            comboFilter.Items.AddRange(exts.ToArray());
            comboFilter.Enabled = exts.Count > 0;
            if (comboFilter.Items.Count > 0) comboFilter.SelectedIndex = 0;
        }

        private void AutoSelectFirst()
        {
            if (fileList.Count == 0) return;
            listFiles.ClearSelected();
            listFiles.SetSelected(0, true);
        }

        private void BtnAddFiles_Click(object sender, EventArgs e)
        {
            OpenFileDialog ofd = new OpenFileDialog();
            ofd.Multiselect = true;
            ofd.Filter = Strings.Get("DlgFilterAll") + "|*.*|"
                       + Strings.Get("DlgFilterVideo") + "|*.mp4;*.avi;*.mkv;*.mov;*.webm|"
                       + Strings.Get("DlgFilterAudio") + "|*.mp3;*.wav;*.aac;*.flac;*.m4a";
            if (ofd.ShowDialog() == DialogResult.OK)
            {
                foreach (string f in ofd.FileNames) AddToQueue(f);
                RefreshListBox();
                AutoSelectFirst();
                lblStatus.Text = Strings.Get("StatusFilesAdded");
            }
        }

        private void BtnAddFolder_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog fbd = new FolderBrowserDialog();
            fbd.Description = Strings.Get("DlgPickFolder");
            if (fbd.ShowDialog() != DialogResult.OK) return;

            lblStatus.Text = Strings.Get("StatusConverting");
            Application.DoEvents();

            string[] validExts = { ".mp4", ".avi", ".mkv", ".mov", ".webm",
                                   ".mp3", ".wav", ".aac", ".flac", ".m4a" };
            int added = 0;
            try
            {
                string[] all = Directory.GetFiles(fbd.SelectedPath, "*.*", SearchOption.AllDirectories);
                foreach (string f in all)
                {
                    string ext = Path.GetExtension(f).ToLower();
                    if (Array.IndexOf(validExts, ext) >= 0)
                    {
                        int before = fileList.Count;
                        AddToQueue(f);
                        if (fileList.Count > before) added++;
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message, Strings.Get("TitleError"), MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            RefreshListBox();
            AutoSelectFirst();
            MessageBox.Show(Strings.Fmt("MsgScanDone", added, fileList.Count),
                Strings.Get("TitleTip"), MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void BtnRemove_Click(object sender, EventArgs e)
        {
            if (listFiles.SelectedIndices.Count == 0) return;
            List<int> idx = new List<int>();
            foreach (int i in listFiles.SelectedIndices) idx.Add(i);
            idx.Sort();
            for (int i = idx.Count - 1; i >= 0; --i) fileList.RemoveAt(idx[i]);
            RefreshListBox();
            AutoSelectFirst();
        }

        private void BtnClear_Click(object sender, EventArgs e)
        {
            fileList.Clear();
            RefreshListBox();
            comboFormat.Items.Clear();
            comboFormat.Enabled = false;
            lblStatus.Text = Strings.Get("StatusListCleared");
        }

        private void BtnFilter_Click(object sender, EventArgs e)
        {
            if (comboFilter.SelectedIndex < 0)
            {
                MessageBox.Show(Strings.Get("MsgFilterEmpty"),
                    Strings.Get("TitleTip"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            string targetExt = comboFilter.SelectedItem.ToString();
            List<string> newList = new List<string>();
            foreach (string f in fileList)
                if (Path.GetExtension(f).ToLower() == targetExt) newList.Add(f);

            if (newList.Count == 0)
            {
                MessageBox.Show(Strings.Get("MsgFilterNothing"),
                    Strings.Get("TitleTip"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            fileList.Clear();
            fileList.AddRange(newList);
            RefreshListBox();
            for (int i = 0; i < listFiles.Items.Count; ++i) listFiles.SetSelected(i, true);

            lblStatus.Text = Strings.Fmt("StatusFiltered", newList.Count, targetExt);
        }

        private void BtnSelectOut_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog fbd = new FolderBrowserDialog();
            fbd.Description = Strings.Get("DlgPickOutput");
            if (fbd.ShowDialog() == DialogResult.OK)
                txtOutputDir.Text = fbd.SelectedPath;
        }

        private void BtnOpenOut_Click(object sender, EventArgs e)
        {
            string p = txtOutputDir.Text;
            if (string.IsNullOrEmpty(p))
            {
                MessageBox.Show(Strings.Get("MsgNoOutputFiles"),
                    Strings.Get("TitleTip"), MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            try { Process.Start("explorer.exe", "\"" + p + "\""); }
            catch { }
        }

        // ===== 开始转换 =====
        private void BtnConvert_Click(object sender, EventArgs e)
        {
            if (isConverting)
            {
                MessageBox.Show(Strings.Get("MsgBusy"),
                    Strings.Get("TitleTip"), MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            if (fileList.Count == 0)
            {
                MessageBox.Show(Strings.Get("MsgQueueEmpty"),
                    Strings.Get("TitleTip"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            if (string.IsNullOrEmpty(txtOutputDir.Text))
            {
                MessageBox.Show(Strings.Get("MsgNoOutputDir"),
                    Strings.Get("TitleTip"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            if (!FfmpegUtil.Exists())
            {
                MessageBox.Show(Strings.Get("MsgFfmpegMissing"),
                    Strings.Get("TitleError"), MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (comboFormat.SelectedIndex < 0)
            {
                MessageBox.Show(Strings.Get("MsgNoFormat"),
                    Strings.Get("TitleTip"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            FmtItem fi = comboFormat.SelectedItem as FmtItem;
            if (fi == null) return;

            ConversionOptions tmpl = CaptureOptions(fi.Code);
            List<string> files = new List<string>(fileList);
            string dst = txtOutputDir.Text;

            isConverting = true;
            btnConvert.Enabled = false;
            btnOpenOut.Enabled = false;
            lblStatus.Text = tmpl.IsStreamCopy
                ? Strings.Get("StatusCopyConverting")
                : Strings.Get("StatusConverting");

            Thread t = new Thread(delegate() { RunConversion(files, dst, tmpl); });
            t.IsBackground = true;
            t.Start();
        }

        private ConversionOptions CaptureOptions(FmtCode code)
        {
            ConversionOptions o = new ConversionOptions();
            o.IsStreamCopy = (code == FmtCode.Mp4Copy ||
                              code == FmtCode.MkvCopy ||
                              code == FmtCode.AviCopy);

            switch (code)
            {
                case FmtCode.Mp4: case FmtCode.Mp4Copy: o.TargetExt = ".mp4"; break;
                case FmtCode.Mkv: case FmtCode.MkvCopy: o.TargetExt = ".mkv"; break;
                case FmtCode.Avi: case FmtCode.AviCopy: o.TargetExt = ".avi"; break;
                case FmtCode.Mp3:  o.TargetExt = ".mp3";  o.IsAudioOutput = true; break;
                case FmtCode.Wav:  o.TargetExt = ".wav";  o.IsAudioOutput = true; break;
                case FmtCode.Aac:  o.TargetExt = ".aac";  o.IsAudioOutput = true; break;
                case FmtCode.Flac: o.TargetExt = ".flac"; o.IsAudioOutput = true; break;
            }

            o.StartTime = txtStartTime.Text;
            o.Duration = txtDuration.Text;

            string[] presets = { "medium", "fast", "slow" };
            int[] crfs = { 18, 23, 28 };
            o.Preset = presets[Math.Max(0, Math.Min(2, comboPreset.SelectedIndex))];
            o.Crf = crfs[Math.Max(0, Math.Min(2, comboCrf.SelectedIndex))];

            int ri = comboRes.SelectedIndex;
            if (ri == 1) o.Resolution = "1920x1080";
            else if (ri == 2) o.Resolution = "1280x720";
            else if (ri == 3) o.Resolution = "854x480";

            int fi = comboFps.SelectedIndex;
            if (fi == 1) o.Fps = 60;
            else if (fi == 2) o.Fps = 30;
            else if (fi == 3) o.Fps = 24;

            int abi = comboAudioBitrate.SelectedIndex;
            if (abi == 1) o.AudioBitrate = "320k";
            else if (abi == 2) o.AudioBitrate = "192k";
            else if (abi == 3) o.AudioBitrate = "128k";

            int ari = comboAudioRate.SelectedIndex;
            if (ari == 1) o.AudioSampleRate = 48000;
            else if (ari == 2) o.AudioSampleRate = 44100;

            int ci = comboChannels.SelectedIndex;
            if (ci == 1) o.AudioChannels = 2;
            else if (ci == 2) o.AudioChannels = 1;

            o.Volume = ParseD(txtVolume.Text, 1.0);
            o.Speed = ParseD(txtSpeed.Text, 1.0);
            o.Brightness = ParseD(txtBrightness.Text, 0);
            o.Contrast = ParseD(txtContrast.Text, 1.0);
            o.Saturation = ParseD(txtSaturation.Text, 1.0);
            o.Sharpen = ParseD(txtSharpen.Text, 0);

            return o;
        }

        private static double ParseD(string s, double fallback)
        {
            if (string.IsNullOrWhiteSpace(s)) return fallback;
            double v;
            if (double.TryParse(s.Trim(), NumberStyles.Float,
                CultureInfo.InvariantCulture, out v)) return v;
            return fallback;
        }

        private void RunConversion(List<string> files, string dst, ConversionOptions tmpl)
        {
            int success = 0, failed = 0, skipped = 0;
            List<string> errors = new List<string>();

            for (int i = 0; i < files.Count; ++i)
            {
                string src = files[i];
                string srcExt = Path.GetExtension(src).ToLower();
                bool isSrcVideo = IsVideoExt(srcExt);

                if (!isSrcVideo && !tmpl.IsAudioOutput) { skipped++; continue; }

                int idx = i;
                SafeInvoke(delegate()
                {
                    lblStatus.Text = Strings.Get("StatusConverting") +
                        " (" + (idx + 1) + "/" + files.Count + ")...";
                });

                string outPath = Path.Combine(dst, Path.GetFileNameWithoutExtension(src) + tmpl.TargetExt);
                int counter = 1;
                while (File.Exists(outPath))
                {
                    outPath = Path.Combine(dst,
                        Path.GetFileNameWithoutExtension(src) + "_" + counter + tmpl.TargetExt);
                    counter++;
                }

                ConversionOptions o = CloneOptions(tmpl, src, outPath);
                if (!o.IsStreamCopy)
                {
                    MediaInfo mi = FfmpegUtil.Probe(src);
                    o.SourceHasAudio = mi.HasAudio;
                }

                string args = FfmpegUtil.BuildArgs(o);
                ConvertResult r = FfmpegUtil.Run(args);
                if (r.Success) success++;
                else
                {
                    failed++;
                    string err = r.Error;
                    if (err.Length > 400) err = err.Substring(err.Length - 400);
                    errors.Add(Path.GetFileName(src) + ": " + err);
                }
            }

            string outDir = dst;
            string ext = tmpl.TargetExt;
            int sc = success, tt = files.Count, sk = skipped, fl = failed;
            List<string> errs = errors;

            SafeInvoke(delegate()
            {
                isConverting = false;
                btnConvert.Enabled = true;
                btnOpenOut.Enabled = true;

                string msg = Strings.Fmt("MsgConvertDone", sc, tt, outDir, ext);
                if (sk > 0) msg += "\n(skipped: " + sk + ")";
                if (fl > 0)
                {
                    msg += "\nFailed: " + fl;
                    for (int j = 0; j < errs.Count && j < 5; ++j)
                        msg += "\n" + errs[j];
                    if (errs.Count > 5) msg += "\n... +" + (errs.Count - 5);
                }

                MessageBox.Show(msg, Strings.Get("TitleDone"), MessageBoxButtons.OK,
                    fl > 0 ? MessageBoxIcon.Warning : MessageBoxIcon.Information);
                lblStatus.Text = fl > 0
                    ? Strings.Fmt("StatusDoneFail", fl)
                    : Strings.Get("StatusDone");
            });
        }

        private static ConversionOptions CloneOptions(ConversionOptions src, string input, string output)
        {
            ConversionOptions o = new ConversionOptions();
            o.InputPath = input;
            o.OutputPath = output;
            o.TargetExt = src.TargetExt;
            o.SourceHasAudio = src.SourceHasAudio;
            o.IsAudioOutput = src.IsAudioOutput;
            o.IsStreamCopy = src.IsStreamCopy;
            o.StartTime = src.StartTime;
            o.Duration = src.Duration;
            o.Preset = src.Preset;
            o.Crf = src.Crf;
            o.Resolution = src.Resolution;
            o.Fps = src.Fps;
            o.Brightness = src.Brightness;
            o.Contrast = src.Contrast;
            o.Saturation = src.Saturation;
            o.Sharpen = src.Sharpen;
            o.AudioBitrate = src.AudioBitrate;
            o.AudioSampleRate = src.AudioSampleRate;
            o.AudioChannels = src.AudioChannels;
            o.Volume = src.Volume;
            o.Speed = src.Speed;
            return o;
        }

        private void SafeInvoke(Action action)
        {
            if (IsDisposed || !IsHandleCreated) return;
            try
            {
                if (InvokeRequired) Invoke(action);
                else action();
            }
            catch (ObjectDisposedException) { }
            catch (InvalidOperationException) { }
        }
    }

    // ============================================================
    // 入口
    // ============================================================
    static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }
    }
}