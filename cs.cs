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
    // 1. 数据模型
    // ============================================================
    public class ConversionOptions
    {
        public string InputPath;
        public string OutputPath;
        public string TargetExt;

        public bool SourceHasAudio = true;
        public bool IsAudioOutput = false;
        public bool IsStreamCopy = false;   // 仅换容器

        public string StartTime;
        public string Duration;

        // 视频
        public string Preset = "medium";
        public int Crf = 23;
        public string Resolution;       // null = 保持原样
        public int Fps = 0;             // 0 = 保持原样
        public double Brightness = 0;
        public double Contrast = 1;
        public double Saturation = 1;
        public double Sharpen = 0;

        // 音频
        public string AudioBitrate;      // null = 自动
        public int AudioSampleRate = 0;  // 0 = 保持原样
        public int AudioChannels = 0;    // 0 = 保持原样
        public double Volume = 1;

        // 速度
        public double Speed = 1;
    }

    // 源文件媒体信息（用于 stream copy 判断）
    public class MediaInfo
    {
        public bool HasVideo = false;
        public bool HasAudio = false;
        public string VideoCodec = "";
        public string AudioCodec = "";
        public bool Valid = false;      // 探测是否成功
    }

    public class RunResult
    {
        public bool Success;
        public int ExitCode;
        public string StderrText = "";
    }

    public class ConvertResult
    {
        public bool Success;
        public string Error = "";
    }

    // ============================================================
    // 2. FFmpeg 定位
    // ============================================================
    public static class FfmpegLocator
    {
        public static string Find(string exeName)
        {
            string local = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, exeName);
            if (File.Exists(local)) return local;

            string pathEnv = Environment.GetEnvironmentVariable("PATH");
            if (!string.IsNullOrEmpty(pathEnv))
            {
                foreach (string dir in pathEnv.Split(';'))
                {
                    if (string.IsNullOrWhiteSpace(dir)) continue;
                    try
                    {
                        string full = Path.Combine(dir.Trim(), exeName);
                        if (File.Exists(full)) return full;
                    }
                    catch { }
                }
            }
            return exeName;
        }
    }

    // ============================================================
    // 3. 执行 ffmpeg
    // ============================================================
    public class FfmpegRunner
    {
        private readonly string _ffmpegPath;

        public FfmpegRunner(string ffmpegPath)
        {
            _ffmpegPath = ffmpegPath;
        }

        public RunResult Run(string args)
        {
            var result = new RunResult();
            var psi = new ProcessStartInfo
            {
                FileName = _ffmpegPath,
                Arguments = args,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardError = true,
                RedirectStandardOutput = true,
                StandardErrorEncoding = Encoding.UTF8,
                StandardOutputEncoding = Encoding.UTF8,
            };

            try
            {
                using (var p = new Process { StartInfo = psi })
                {
                    p.Start();
                    var stderrTask = p.StandardError.ReadToEndAsync();
                    var stdoutTask = p.StandardOutput.ReadToEndAsync();
                    p.WaitForExit();

                    string stderr = stderrTask.Result ?? "";
                    string stdout = stdoutTask.Result ?? "";
                    result.StderrText = stderr + (string.IsNullOrEmpty(stdout) ? "" : "\n" + stdout);
                    result.ExitCode = p.ExitCode;
                    result.Success = (p.ExitCode == 0);
                }
            }
            catch (Exception ex)
            {
                result.StderrText = "[错误] " + ex.Message;
                result.Success = false;
            }
            return result;
        }
    }

    // ============================================================
    // 4. 媒体探测 + 容器兼容性判断
    // ============================================================
    public static class MediaProbe
    {
        public static MediaInfo Probe(string ffmpegPath, string inputPath)
        {
            var info = new MediaInfo();
            var runner = new FfmpegRunner(ffmpegPath);
            RunResult r = runner.Run("-hide_banner -i " + FfmpegArgumentBuilder.Quote(inputPath));
            string s = r.StderrText ?? "";

            // 解析 "Video: h264 (High) ..."
            int vpos = s.IndexOf("Video: ", StringComparison.OrdinalIgnoreCase);
            if (vpos >= 0)
            {
                int start = vpos + 7;
                int end = IndexOfAny(s, start, ' ', ',', '(');
                if (end > start)
                {
                    info.VideoCodec = s.Substring(start, end - start).Trim().ToLowerInvariant();
                    info.HasVideo = info.VideoCodec.Length > 0;
                }
            }

            // 解析 "Audio: aac (LC) ..."
            int apos = s.IndexOf("Audio: ", StringComparison.OrdinalIgnoreCase);
            if (apos >= 0)
            {
                int start = apos + 7;
                int end = IndexOfAny(s, start, ' ', ',', '(');
                if (end > start)
                {
                    info.AudioCodec = s.Substring(start, end - start).Trim().ToLowerInvariant();
                    info.HasAudio = info.AudioCodec.Length > 0;
                }
            }

            info.Valid = info.HasVideo || info.HasAudio;
            return info;
        }

        private static int IndexOfAny(string s, int start, params char[] chars)
        {
            for (int i = start; i < s.Length; i++)
            {
                for (int j = 0; j < chars.Length; j++)
                    if (s[i] == chars[j]) return i;
            }
            return -1;
        }

        // 目标容器支持的视频编码
        public static bool ContainerSupportsVideoCodec(string container, string codec)
        {
            switch (container)
            {
                case ".mkv":
                    return codec == "h264" || codec == "hevc" || codec == "h265"
                        || codec == "mpeg4" || codec == "vp8" || codec == "vp9"
                        || codec == "av1" || codec == "mpeg2video" || codec == "vc1";
                case ".mp4":
                    return codec == "h264" || codec == "hevc" || codec == "h265"
                        || codec == "mpeg4" || codec == "av1" || codec == "vp9";
                case ".avi":
                    return codec == "h264" || codec == "mpeg4" || codec == "mjpeg";
                case ".mov":
                    return codec == "h264" || codec == "hevc" || codec == "prores";
                case ".webm":
                    return codec == "vp8" || codec == "vp9" || codec == "av1";
            }
            return false;
        }

        // 目标容器支持的音频编码
        public static bool ContainerSupportsAudioCodec(string container, string codec)
        {
            switch (container)
            {
                case ".mkv":
                    return codec == "aac" || codec == "mp3" || codec == "opus"
                        || codec == "vorbis" || codec == "flac" || codec == "ac3"
                        || codec == "dts" || codec == "eac3" || codec == "pcm_s16le";
                case ".mp4":
                    return codec == "aac" || codec == "mp3" || codec == "ac3"
                        || codec == "eac3" || codec == "alac";
                case ".avi":
                    return codec == "mp3" || codec == "ac3" || codec == "pcm_s16le";
                case ".mov":
                    return codec == "aac" || codec == "pcm_s16le" || codec == "pcm_s24le";
                case ".webm":
                    return codec == "vorbis" || codec == "opus";
            }
            return false;
        }

        // 判断能不能把 info 里的流直接 copy 进目标容器
        public static bool CanStreamCopy(MediaInfo info, string dstExt)
        {
            if (info == null || !info.Valid) return false;

            if (info.HasVideo)
            {
                if (!IsVideoExt(dstExt)) return false;
                if (!ContainerSupportsVideoCodec(dstExt, info.VideoCodec)) return false;
                if (info.HasAudio && !ContainerSupportsAudioCodec(dstExt, info.AudioCodec))
                    return false;
                return true;
            }

            // 纯音频源 → 音频容器，一般都要重编码
            return false;
        }

        public static bool IsVideoExt(string ext)
        {
            return ext == ".mp4" || ext == ".avi" || ext == ".mkv"
                || ext == ".mov" || ext == ".webm";
        }
    }

    // ============================================================
    // 5. 参数构建
    // ============================================================
    public static class FfmpegArgumentBuilder
    {
        public static string Quote(string value)
        {
            if (string.IsNullOrEmpty(value)) return "\"\"";
            return "\"" + value.Replace("\"", "\\\"") + "\"";
        }

        public static string Num(double v)
        {
            return v.ToString("0.####", CultureInfo.InvariantCulture);
        }

        public static string Build(ConversionOptions o)
        {
            var sb = new StringBuilder();
            sb.Append("-hide_banner -i ").Append(Quote(o.InputPath)).Append(' ');

            if (!string.IsNullOrWhiteSpace(o.StartTime))
                sb.Append("-ss ").Append(Quote(o.StartTime.Trim())).Append(' ');
            if (!string.IsNullOrWhiteSpace(o.Duration))
                sb.Append("-t ").Append(Quote(o.Duration.Trim())).Append(' ');

            // ---------- 仅换容器 ----------
            if (o.IsStreamCopy)
            {
                sb.Append("-map 0:v? -map 0:a? -c copy ");
                if (o.TargetExt == ".mp4") sb.Append("-movflags +faststart ");
                sb.Append(Quote(o.OutputPath)).Append(" -y");
                return sb.ToString();
            }

            bool applySpeed = o.Speed >= 0.5 && o.Speed <= 4.0
                              && Math.Abs(o.Speed - 1.0) > 0.001;
            bool hasAudio = o.SourceHasAudio;

            if (o.IsAudioOutput)
                BuildAudioOnly(sb, o, applySpeed);
            else
                BuildVideo(sb, o, applySpeed, hasAudio);

            sb.Append(Quote(o.OutputPath)).Append(" -y");
            return sb.ToString();
        }

        private static void BuildAudioOnly(StringBuilder sb, ConversionOptions o, bool applySpeed)
        {
            sb.Append("-vn ");
            switch (o.TargetExt)
            {
                case ".mp3":  sb.Append("-c:a libmp3lame "); break;
                case ".wav":  sb.Append("-c:a pcm_s16le ");  break;
                case ".aac":  sb.Append("-c:a aac ");        break;
                case ".flac": sb.Append("-c:a flac ");       break;
            }

            var filters = new List<string>();
            if (Math.Abs(o.Volume - 1.0) > 0.001)
                filters.Add("volume=" + Num(o.Volume));
            if (applySpeed)
                filters.AddRange(BuildAtempoChain(o.Speed));

            if (filters.Count > 0)
                sb.Append("-af ").Append(Quote(string.Join(",", filters))).Append(' ');

            AppendAudioQuality(sb, o);
        }

        private static void BuildVideo(StringBuilder sb, ConversionOptions o, bool applySpeed, bool hasAudio)
        {
            sb.Append("-c:v libx264 ");
            sb.Append("-preset ").Append(o.Preset).Append(' ');
            sb.Append("-crf ").Append(o.Crf).Append(' ');
            sb.Append("-pix_fmt yuv420p ");

            if (!string.IsNullOrEmpty(o.Resolution))
                sb.Append("-s ").Append(o.Resolution).Append(' ');
            if (o.Fps > 0)
                sb.Append("-r ").Append(o.Fps).Append(' ');

            var videoFilters = BuildVideoFilters(o);
            var audioFilters = hasAudio ? BuildAudioFilters(o) : new List<string>();

            if (applySpeed)
            {
                var parts = new List<string>();
                var vf = new List<string>(videoFilters);
                vf.Add("setpts=PTS/" + Num(o.Speed));
                parts.Add("[0:v]" + string.Join(",", vf) + "[v]");

                if (hasAudio)
                {
                    var af = new List<string>(audioFilters);
                    af.AddRange(BuildAtempoChain(o.Speed));
                    parts.Add("[0:a]" + string.Join(",", af) + "[a]");
                    sb.Append("-filter_complex ").Append(Quote(string.Join(";", parts))).Append(' ');
                    sb.Append("-map \"[v]\" -map \"[a]\" ");
                }
                else
                {
                    sb.Append("-filter_complex ").Append(Quote(string.Join(";", parts))).Append(' ');
                    sb.Append("-map \"[v]\" ");
                }
            }
            else
            {
                if (videoFilters.Count > 0)
                    sb.Append("-vf ").Append(Quote(string.Join(",", videoFilters))).Append(' ');
                if (hasAudio && audioFilters.Count > 0)
                    sb.Append("-af ").Append(Quote(string.Join(",", audioFilters))).Append(' ');
            }

            if (hasAudio)
            {
                if (o.TargetExt == ".avi") sb.Append("-c:a libmp3lame ");
                else                        sb.Append("-c:a aac ");
                AppendAudioQuality(sb, o);
            }
            else
            {
                sb.Append("-an ");
            }

            if (o.TargetExt == ".mp4")
                sb.Append("-movflags +faststart ");
        }

        private static List<string> BuildVideoFilters(ConversionOptions o)
        {
            var list = new List<string>();
            bool hasB = Math.Abs(o.Brightness) > 0.001;
            bool hasC = Math.Abs(o.Contrast - 1.0) > 0.001;
            bool hasS = Math.Abs(o.Saturation - 1.0) > 0.001;

            if (hasB || hasC || hasS)
            {
                var eq = new List<string>();
                if (hasB) eq.Add("brightness=" + Num(o.Brightness));
                if (hasC) eq.Add("contrast=" + Num(o.Contrast));
                if (hasS) eq.Add("saturation=" + Num(o.Saturation));
                list.Add("eq=" + string.Join(":", eq));
            }
            if (o.Sharpen > 0.001)
                list.Add("unsharp=5:5:" + Num(o.Sharpen));
            return list;
        }

        private static List<string> BuildAudioFilters(ConversionOptions o)
        {
            var list = new List<string>();
            if (Math.Abs(o.Volume - 1.0) > 0.001)
                list.Add("volume=" + Num(o.Volume));
            return list;
        }

        private static List<string> BuildAtempoChain(double speed)
        {
            var result = new List<string>();
            double remaining = speed;
            while (remaining > 2.0) { result.Add("atempo=2.0"); remaining /= 2.0; }
            while (remaining < 0.5) { result.Add("atempo=0.5"); remaining /= 0.5; }
            result.Add("atempo=" + Num(remaining));
            return result;
        }

        private static void AppendAudioQuality(StringBuilder sb, ConversionOptions o)
        {
            if (!string.IsNullOrEmpty(o.AudioBitrate))
                sb.Append("-b:a ").Append(o.AudioBitrate).Append(' ');
            if (o.AudioSampleRate > 0)
                sb.Append("-ar ").Append(o.AudioSampleRate).Append(' ');
            if (o.AudioChannels > 0)
                sb.Append("-ac ").Append(o.AudioChannels).Append(' ');
        }
    }

    // ============================================================
    // 6. 主窗体
    // ============================================================
    public class MainForm : Form
    {
        // UI
        private ListBox listFiles;
        private ComboBox comboFilter;
        private ComboBox comboFormat, comboPreset, comboCRF, comboRes, comboFPS;
        private ComboBox comboAudioBitrate, comboAudioRate, comboChannels;
        private TextBox txtOutputDir, txtStartTime, txtDuration, txtSpeed;
        private TextBox txtBrightness, txtContrast, txtSaturation, txtSharpen, txtVolume;
        private Button btnAddFiles, btnAddFolder, btnRemove, btnClear, btnFilter;
        private Button btnSelectOut, btnOpenOut, btnConvert;
        private Label lblStatus;
        private ProgressBar progressBar;

        // 数据
        private readonly List<string> fileList = new List<string>();
        private string ffmpegPath;
        private FfmpegRunner runner;
        private volatile bool isConverting;

        // 缓存：源文件 → MediaInfo（避免每次切换都重新探测）
        private readonly Dictionary<string, MediaInfo> probeCache =
            new Dictionary<string, MediaInfo>(StringComparer.OrdinalIgnoreCase);

        public MainForm()
        {
            InitializeUI();
            ffmpegPath = FfmpegLocator.Find("ffmpeg.exe");
            runner = new FfmpegRunner(ffmpegPath);

            if (!File.Exists(ffmpegPath))
            {
                MessageBox.Show(
                    "未找到 ffmpeg.exe！\n请确保它在本程序目录下，或已配置系统环境变量。\n\n"
                    + "当前查找结果：" + ffmpegPath,
                    "警告", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        // ================== UI ==================
        private void InitializeUI()
        {
            Text = "音视频批量转换工具 (C# 版)";
            Size = new Size(840, 700);
            StartPosition = FormStartPosition.CenterScreen;
            MinimumSize = new Size(820, 680);
            Font = new Font("Microsoft YaHei", 9F);

            // 顶部
            var topPanel = new Panel { Dock = DockStyle.Top, Height = 45 };
            Controls.Add(topPanel);

            btnAddFiles  = new Button { Text = "添加文件",   Location = new Point(10, 10),  Size = new Size(80, 25) };
            btnAddFolder = new Button { Text = "添加文件夹", Location = new Point(100, 10), Size = new Size(90, 25) };
            btnRemove    = new Button { Text = "移除选中",   Location = new Point(200, 10), Size = new Size(80, 25) };
            btnClear     = new Button { Text = "清空列表",   Location = new Point(290, 10), Size = new Size(80, 25) };
            comboFilter  = new ComboBox { Location = new Point(390, 10), Size = new Size(100, 25), DropDownStyle = ComboBoxStyle.DropDownList };
            btnFilter    = new Button { Text = "过滤并全选", Location = new Point(500, 10), Size = new Size(90, 25) };

            topPanel.Controls.AddRange(new Control[]
            {
                btnAddFiles, btnAddFolder, btnRemove, btnClear, comboFilter, btnFilter
            });

            // 列表
            listFiles = new ListBox
            {
                Location = new Point(10, 55),
                Size = new Size(800, 180),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right,
                SelectionMode = SelectionMode.MultiExtended,
            };
            Controls.Add(listFiles);

            // 参数面板
            var paramPanel = new Panel
            {
                Location = new Point(10, 245),
                Size = new Size(800, 340),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right | AnchorStyles.Bottom,
            };
            Controls.Add(paramPanel);

            int y = 10;

            // 输出目录
            paramPanel.Controls.Add(new Label { Text = "输出目录:", Location = new Point(10, y + 3), Size = new Size(60, 20) });
            txtOutputDir = new TextBox { Location = new Point(80, y), Size = new Size(500, 22), Anchor = AnchorStyles.Left | AnchorStyles.Right };
            btnSelectOut = new Button { Text = "选择输出目录", Location = new Point(590, y), Size = new Size(100, 22), Anchor = AnchorStyles.Right };
            btnOpenOut   = new Button { Text = "打开输出目录", Location = new Point(700, y), Size = new Size(100, 22), Anchor = AnchorStyles.Right };
            paramPanel.Controls.Add(txtOutputDir);
            paramPanel.Controls.Add(btnSelectOut);
            paramPanel.Controls.Add(btnOpenOut);
            y += 35;

            // 格式行
            paramPanel.Controls.Add(new Label { Text = "输出格式:", Location = new Point(10, y + 3), Size = new Size(60, 20) });
            comboFormat = new ComboBox { Location = new Point(80, y), Size = new Size(130, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            paramPanel.Controls.Add(comboFormat);

            paramPanel.Controls.Add(new Label { Text = "编码预设:", Location = new Point(220, y + 3), Size = new Size(60, 20) });
            comboPreset = new ComboBox { Location = new Point(280, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboPreset.Items.AddRange(new object[] { "medium (默认)", "fast (快速)", "slow (慢速)" });
            comboPreset.SelectedIndex = 0;
            paramPanel.Controls.Add(comboPreset);

            paramPanel.Controls.Add(new Label { Text = "画质(CRF):", Location = new Point(390, y + 3), Size = new Size(60, 20) });
            comboCRF = new ComboBox { Location = new Point(450, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboCRF.Items.AddRange(new object[] { "18 (高画质)", "23 (均衡)", "28 (压缩优先)" });
            comboCRF.SelectedIndex = 1;
            paramPanel.Controls.Add(comboCRF);
            y += 30;

            // 分辨率 / 帧率 / 音频码率
            paramPanel.Controls.Add(new Label { Text = "分辨率:", Location = new Point(10, y + 3), Size = new Size(60, 20) });
            comboRes = new ComboBox { Location = new Point(80, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboRes.Items.AddRange(new object[] { "保持原样", "1920x1080", "1280x720", "854x480" });
            comboRes.SelectedIndex = 0;
            paramPanel.Controls.Add(comboRes);

            paramPanel.Controls.Add(new Label { Text = "帧率(FPS):", Location = new Point(200, y + 3), Size = new Size(60, 20) });
            comboFPS = new ComboBox { Location = new Point(260, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboFPS.Items.AddRange(new object[] { "保持原样", "60", "30", "24" });
            comboFPS.SelectedIndex = 0;
            paramPanel.Controls.Add(comboFPS);

            paramPanel.Controls.Add(new Label { Text = "音频码率:", Location = new Point(380, y + 3), Size = new Size(60, 20) });
            comboAudioBitrate = new ComboBox { Location = new Point(440, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboAudioBitrate.Items.AddRange(new object[] { "默认/自动", "320 kbps", "192 kbps", "128 kbps" });
            comboAudioBitrate.SelectedIndex = 0;
            paramPanel.Controls.Add(comboAudioBitrate);
            y += 30;

            // 采样率 / 声道 / 音量
            paramPanel.Controls.Add(new Label { Text = "采样率:", Location = new Point(10, y + 3), Size = new Size(60, 20) });
            comboAudioRate = new ComboBox { Location = new Point(80, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboAudioRate.Items.AddRange(new object[] { "保持原样", "48000 Hz", "44100 Hz" });
            comboAudioRate.SelectedIndex = 0;
            paramPanel.Controls.Add(comboAudioRate);

            paramPanel.Controls.Add(new Label { Text = "声道:", Location = new Point(200, y + 3), Size = new Size(60, 20) });
            comboChannels = new ComboBox { Location = new Point(260, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboChannels.Items.AddRange(new object[] { "保持原样", "立体声(2)", "单声道(1)" });
            comboChannels.SelectedIndex = 0;
            paramPanel.Controls.Add(comboChannels);

            paramPanel.Controls.Add(new Label { Text = "音量倍数:", Location = new Point(380, y + 3), Size = new Size(60, 20) });
            txtVolume = new TextBox { Location = new Point(440, y), Size = new Size(100, 22) };
            paramPanel.Controls.Add(txtVolume);
            y += 35;

            // 颜色滤镜
            paramPanel.Controls.Add(new Label { Text = "亮度(-1~1):", Location = new Point(10, y + 3), Size = new Size(70, 20) });
            txtBrightness = new TextBox { Location = new Point(80, y), Size = new Size(50, 22) };
            paramPanel.Controls.Add(txtBrightness);

            paramPanel.Controls.Add(new Label { Text = "对比度(0~3):", Location = new Point(140, y + 3), Size = new Size(70, 20) });
            txtContrast = new TextBox { Location = new Point(210, y), Size = new Size(50, 22) };
            paramPanel.Controls.Add(txtContrast);

            paramPanel.Controls.Add(new Label { Text = "饱和度(0~3):", Location = new Point(270, y + 3), Size = new Size(70, 20) });
            txtSaturation = new TextBox { Location = new Point(340, y), Size = new Size(50, 22) };
            paramPanel.Controls.Add(txtSaturation);

            paramPanel.Controls.Add(new Label { Text = "锐化(0~5):", Location = new Point(400, y + 3), Size = new Size(60, 20) });
            txtSharpen = new TextBox { Location = new Point(460, y), Size = new Size(50, 22) };
            paramPanel.Controls.Add(txtSharpen);

            paramPanel.Controls.Add(new Label { Text = "倍速:", Location = new Point(520, y + 3), Size = new Size(40, 20) });
            txtSpeed = new TextBox { Location = new Point(560, y), Size = new Size(50, 22) };
            paramPanel.Controls.Add(txtSpeed);
            y += 35;

            // 时间裁剪
            paramPanel.Controls.Add(new Label { Text = "起始时间:", Location = new Point(10, y + 3), Size = new Size(60, 20) });
            txtStartTime = new TextBox { Location = new Point(80, y), Size = new Size(100, 22) };
            paramPanel.Controls.Add(txtStartTime);

            paramPanel.Controls.Add(new Label { Text = "持续时长:", Location = new Point(190, y + 3), Size = new Size(60, 20) });
            txtDuration = new TextBox { Location = new Point(260, y), Size = new Size(100, 22) };
            paramPanel.Controls.Add(txtDuration);

            // 底部
            var bottomPanel = new Panel { Dock = DockStyle.Bottom, Height = 60 };
            Controls.Add(bottomPanel);

            btnConvert  = new Button { Text = "开始转换", Location = new Point(10, 15), Size = new Size(100, 30) };
            lblStatus   = new Label  { Text = "状态：就绪", Location = new Point(120, 20), Size = new Size(420, 20) };
            progressBar = new ProgressBar { Location = new Point(550, 15), Size = new Size(200, 25), Maximum = 100, Anchor = AnchorStyles.Right | AnchorStyles.Top };

            bottomPanel.Controls.Add(btnConvert);
            bottomPanel.Controls.Add(lblStatus);
            bottomPanel.Controls.Add(progressBar);

            // 事件
            btnAddFiles.Click  += BtnAddFiles_Click;
            btnAddFolder.Click += BtnAddFolder_Click;
            btnRemove.Click    += BtnRemove_Click;
            btnClear.Click     += BtnClear_Click;
            btnFilter.Click    += BtnFilter_Click;
            btnConvert.Click   += BtnConvert_Click;
            btnSelectOut.Click += BtnSelectOut_Click;
            btnOpenOut.Click   += BtnOpenOut_Click;
            listFiles.SelectedIndexChanged += ListFiles_SelectedIndexChanged;
            comboFormat.SelectedIndexChanged += (s, e) => UpdateControlStates();

            comboFormat.Enabled = false;
            btnOpenOut.Enabled = false;
        }

        // ================== 置灰逻辑 ==================
        private void UpdateControlStates()
        {
            if (comboFormat.SelectedItem == null) return;
            string fmt = comboFormat.SelectedItem.ToString();
            bool isAudio = fmt.Contains("音频");
            bool isCopy  = fmt.Contains("仅换容器");

            bool videoEnabled = !isAudio && !isCopy;
            bool audioEnabled = !isCopy;

            comboPreset.Enabled       = videoEnabled;
            comboCRF.Enabled          = videoEnabled;
            comboRes.Enabled          = videoEnabled;
            comboFPS.Enabled          = videoEnabled;
            txtBrightness.Enabled     = videoEnabled;
            txtContrast.Enabled       = videoEnabled;
            txtSaturation.Enabled     = videoEnabled;
            txtSharpen.Enabled        = videoEnabled;

            comboAudioBitrate.Enabled = audioEnabled;
            comboAudioRate.Enabled    = audioEnabled;
            comboChannels.Enabled     = audioEnabled;
            txtVolume.Enabled         = audioEnabled;
            txtSpeed.Enabled          = audioEnabled;
        }

        // ================== 列表 ==================
        private void AddFilesToQueue(IEnumerable<string> paths)
        {
            int added = 0;
            foreach (string f in paths)
            {
                if (!fileList.Contains(f, StringComparer.OrdinalIgnoreCase))
                {
                    fileList.Add(f);
                    added++;
                }
            }
            if (added > 0) RefreshListBox();
        }

        private void ScanFolder(string folder)
        {
            try
            {
                string[] validExts = { ".mp4", ".avi", ".mkv", ".mov", ".webm",
                                       ".mp3", ".wav", ".aac", ".flac", ".m4a" };
                string[] files = Directory.GetFiles(folder, "*.*", SearchOption.AllDirectories);

                var toAdd = new List<string>();
                foreach (string f in files)
                {
                    string ext = Path.GetExtension(f).ToLowerInvariant();
                    if (validExts.Contains(ext) &&
                        !fileList.Contains(f, StringComparer.OrdinalIgnoreCase))
                    {
                        toAdd.Add(f);
                    }
                }

                fileList.AddRange(toAdd);
                RefreshListBox();

                MessageBox.Show(
                    "扫描完成！\n本次新增 " + toAdd.Count + " 个文件。\n当前队列共 "
                    + fileList.Count + " 个。",
                    "提示", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show("扫描文件夹出错: " + ex.Message, "错误",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void RefreshListBox()
        {
            listFiles.Items.Clear();
            listFiles.Items.AddRange(fileList.ToArray());
            UpdateFilterCombo();
        }

        private void UpdateFilterCombo()
        {
            comboFilter.Items.Clear();
            var exts = fileList
                .Select(f => Path.GetExtension(f).ToLowerInvariant())
                .Distinct()
                .OrderBy(e => e);

            foreach (var ext in exts) comboFilter.Items.Add(ext);
            if (comboFilter.Items.Count > 0) comboFilter.SelectedIndex = 0;
        }

        private void AutoSelectFirst()
        {
            if (fileList.Count == 0) return;
            listFiles.ClearSelected();
            listFiles.SetSelected(0, true);
            UpdateFormatCombo();
        }

        private void UpdateFormatCombo()
        { 
            ListFiles_SelectedIndexChanged(null, EventArgs.Empty);
        }

        private void ListFiles_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (listFiles.SelectedItems.Count == 0)
            {
                comboFormat.Enabled = false;
                return;
            }

            comboFormat.Enabled = true;
            string selectedFile = listFiles.SelectedItems[0].ToString();
            string ext = Path.GetExtension(selectedFile).ToLowerInvariant();

            // 探测媒体信息（带缓存）
            MediaInfo info;
            if (!probeCache.TryGetValue(selectedFile, out info))
            {
                info = MediaProbe.Probe(ffmpegPath, selectedFile);
                probeCache[selectedFile] = info;
            }

            comboFormat.Items.Clear();
            bool isVideo = MediaProbe.IsVideoExt(ext);

            if (isVideo)
            {
                // MP4 / MKV / AVI 三个视频目标（跳过源格式）
                AddVideoTarget(".mp4", "MP4");
                AddVideoTarget(".mkv", "MKV");
                AddVideoTarget(".avi", "AVI");

                // 音频目标（视频 → 音频，不显示“仅换容器”）
                if (ext != ".mp3")  comboFormat.Items.Add("MP3 (音频)");
                if (ext != ".wav")  comboFormat.Items.Add("WAV (音频)");
                if (ext != ".aac")  comboFormat.Items.Add("AAC (音频)");
                if (ext != ".flac") comboFormat.Items.Add("FLAC (音频)");
            }
            else
            {
                if (ext != ".mp3")  comboFormat.Items.Add("MP3 (音频)");
                if (ext != ".wav")  comboFormat.Items.Add("WAV (音频)");
                if (ext != ".aac")  comboFormat.Items.Add("AAC (音频)");
                if (ext != ".flac") comboFormat.Items.Add("FLAC (音频)");
            }

            if (comboFormat.Items.Count > 0) comboFormat.SelectedIndex = 0;
            UpdateControlStates();
        }

        // 添加视频目标项（含“仅换容器”，如果源流兼容）
        private void AddVideoTarget(string targetExt, string code)
        {
            string srcExt = Path.GetExtension(listFiles.SelectedItems[0].ToString()).ToLowerInvariant();
            if (srcExt == targetExt) return;   // 跳过源格式

            comboFormat.Items.Add(code + " (视频)");

            MediaInfo info;
            if (probeCache.TryGetValue(listFiles.SelectedItems[0].ToString(), out info))
            {
                if (MediaProbe.CanStreamCopy(info, targetExt))
                {
                    comboFormat.Items.Add(code + " (仅换容器)");
                }
            }
        }

        // ================== 按钮事件 ==================
        private void BtnAddFiles_Click(object sender, EventArgs e)
        {
            using (var ofd = new OpenFileDialog())
            {
                ofd.Multiselect = true;
                ofd.Filter = "所有文件|*.*|视频文件|*.mp4;*.avi;*.mkv;*.mov;*.webm|音频文件|*.mp3;*.wav;*.aac;*.flac;*.m4a";
                if (ofd.ShowDialog() == DialogResult.OK)
                {
                    AddFilesToQueue(ofd.FileNames);
                    AutoSelectFirst();
                    lblStatus.Text = "状态：添加了 " + ofd.FileNames.Length + " 个文件";
                }
            }
        }

        private void BtnAddFolder_Click(object sender, EventArgs e)
        {
            using (var fbd = new FolderBrowserDialog())
            {
                if (fbd.ShowDialog() == DialogResult.OK)
                {
                    lblStatus.Text = "状态：正在扫描文件夹...";
                    Application.DoEvents();
                    ScanFolder(fbd.SelectedPath);
                    AutoSelectFirst();
                }
            }
        }

        private void BtnRemove_Click(object sender, EventArgs e)
        {
            if (listFiles.SelectedItems.Count == 0) return;
            for (int i = listFiles.SelectedIndices.Count - 1; i >= 0; i--)
                fileList.RemoveAt(listFiles.SelectedIndices[i]);
            RefreshListBox();
            AutoSelectFirst();
        }

        private void BtnClear_Click(object sender, EventArgs e)
        {
            fileList.Clear();
            probeCache.Clear();
            RefreshListBox();
            lblStatus.Text = "状态：列表已清空";
            comboFormat.Items.Clear();
            comboFormat.Enabled = false;
        }

        private void BtnFilter_Click(object sender, EventArgs e)
        {
            if (comboFilter.SelectedItem == null) return;
            string targetExt = comboFilter.SelectedItem.ToString();

            var newList = fileList
                .Where(f => Path.GetExtension(f).ToLowerInvariant() == targetExt)
                .ToList();

            if (newList.Count == 0)
            {
                MessageBox.Show("过滤后没有剩余文件！", "提示",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            fileList.Clear();
            fileList.AddRange(newList);
            RefreshListBox();

            for (int i = 0; i < listFiles.Items.Count; i++)
                listFiles.SetSelected(i, true);

            UpdateFormatCombo();

            lblStatus.Text = "状态：已过滤出 " + newList.Count + " 个 " + targetExt + " 文件";
        }

        private void BtnSelectOut_Click(object sender, EventArgs e)
        {
            using (var fbd = new FolderBrowserDialog())
            {
                if (fbd.ShowDialog() == DialogResult.OK)
                    txtOutputDir.Text = fbd.SelectedPath;
            }
        }

        private void BtnOpenOut_Click(object sender, EventArgs e)
        {
            string path = txtOutputDir.Text.Trim();
            if (Directory.Exists(path))
            {
                Process.Start("explorer.exe", path);
            }
            else
            {
                MessageBox.Show("输出目录不存在。", "提示", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }

        // ================== 开始转换 ==================
        private void BtnConvert_Click(object sender, EventArgs e)
        {
            if (isConverting) return;
            if (fileList.Count == 0) { MessageBox.Show("队列为空！"); return; }
            if (string.IsNullOrWhiteSpace(txtOutputDir.Text)) { MessageBox.Show("请选择输出目录！"); return; }
            if (comboFormat.SelectedItem == null) { MessageBox.Show("请选择输出格式！"); return; }

            string format = comboFormat.SelectedItem.ToString();
            bool isAudioOutput = format.Contains("音频");
            bool isStreamCopy = format.Contains("仅换容器");
            string targetExt = MapFormatToExtension(format);

            var template = CaptureOptions(isAudioOutput, isStreamCopy, targetExt);
            var files = new List<string>(fileList);
            string outDir = txtOutputDir.Text;

            isConverting = true;
            btnConvert.Enabled = false;
            btnOpenOut.Enabled = false;
            progressBar.Minimum = 0;
            progressBar.Maximum = files.Count;
            progressBar.Value = 0;

            var t = new Thread(() => RunConversion(files, outDir, template))
            {
                IsBackground = true
            };
            t.Start();
        }

        private ConversionOptions CaptureOptions(bool isAudioOutput, bool isStreamCopy, string targetExt)
        {
            return new ConversionOptions
            {
                IsAudioOutput = isAudioOutput,
                IsStreamCopy = isStreamCopy,
                TargetExt = targetExt,
                StartTime = txtStartTime.Text,
                Duration = txtDuration.Text,
                Preset = GetPreset(),
                Crf = GetCrf(),
                Resolution = GetResolution(),
                Fps = GetFps(),
                Brightness = ParseDouble(txtBrightness.Text, 0),
                Contrast = ParseDouble(txtContrast.Text, 1),
                Saturation = ParseDouble(txtSaturation.Text, 1),
                Sharpen = ParseDouble(txtSharpen.Text, 0),
                Volume = ParseDouble(txtVolume.Text, 1),
                Speed = ParseDouble(txtSpeed.Text, 1),
                AudioBitrate = GetAudioBitrate(),
                AudioSampleRate = GetAudioSampleRate(),
                AudioChannels = GetAudioChannels(),
            };
        }

        private void RunConversion(List<string> files, string outDir, ConversionOptions template)
        {
            int success = 0, failed = 0, skipped = 0;
            var errors = new List<string>();

            for (int i = 0; i < files.Count; i++)
            {
                string srcPath = files[i];
                string srcExt = Path.GetExtension(srcPath).ToLowerInvariant();
                bool isSrcVideo = MediaProbe.IsVideoExt(srcExt);

                if (!isSrcVideo && !template.IsAudioOutput)
                {
                    skipped++;
                    continue;
                }

                int idx = i;
                SafeInvoke(() =>
                {
                    lblStatus.Text = string.Format("状态：正在转换 ({0}/{1})...", idx + 1, files.Count);
                    progressBar.Value = Math.Min(idx, progressBar.Maximum - 1);
                });

                string targetPath = GetUniquePath(
                    Path.Combine(outDir, Path.GetFileNameWithoutExtension(srcPath) + template.TargetExt));

                bool hasAudio = true;
                if (!template.IsStreamCopy)
                {
                    MediaInfo info;
                    if (!probeCache.TryGetValue(srcPath, out info))
                    {
                        info = MediaProbe.Probe(ffmpegPath, srcPath);
                        probeCache[srcPath] = info;
                    }
                    hasAudio = !isSrcVideo || info.HasAudio;
                }

                var opt = CloneOptions(template, srcPath, targetPath, hasAudio);
                ConvertResult result = ConvertOneFile(opt);

                if (result.Success) success++;
                else
                {
                    failed++;
                    string err = result.Error ?? "";
                    if (err.Length > 400) err = err.Substring(err.Length - 400);
                    errors.Add(Path.GetFileName(srcPath) + ": " + err);
                }
            }

            SafeInvoke(() =>
            {
                isConverting = false;
                btnConvert.Enabled = true;
                btnOpenOut.Enabled = true;
                progressBar.Value = progressBar.Maximum;

                var sb = new StringBuilder();
                sb.AppendLine("批量转换完成！");
                sb.AppendLine(string.Format("成功: {0} / {1}", success, files.Count));
                sb.AppendLine("输出目录: " + outDir);
                sb.AppendLine("输出格式: " + template.TargetExt
                    + (template.IsStreamCopy ? "（仅换容器）" : ""));
                if (skipped > 0) sb.AppendLine("跳过（音频转视频）: " + skipped);
                if (failed > 0)
                {
                    sb.AppendLine("失败: " + failed);
                    sb.AppendLine();
                    sb.AppendLine("错误摘要：");
                    sb.AppendLine(string.Join(Environment.NewLine, errors.Take(5).ToArray()));
                    if (errors.Count > 5)
                        sb.AppendLine("... 还有 " + (errors.Count - 5) + " 条");
                }

                MessageBox.Show(sb.ToString(), "完成", MessageBoxButtons.OK,
                    failed > 0 ? MessageBoxIcon.Warning : MessageBoxIcon.Information);

                lblStatus.Text = failed > 0
                    ? string.Format("状态：完成，失败 {0} 个", failed)
                    : "状态：批量转换完成！";
            });
        }

        private ConvertResult ConvertOneFile(ConversionOptions opt)
        {
            string args = FfmpegArgumentBuilder.Build(opt);

            WriteLog("\n============================================================\n"
                + "输入: " + opt.InputPath + "\n"
                + "输出: " + opt.OutputPath + "\n"
                + "模式: " + (opt.IsStreamCopy ? "仅换容器 (stream copy)" : "重新编码") + "\n"
                + "参数: " + args + "\n");

            RunResult r = runner.Run(args);

            WriteLog("退出码: " + r.ExitCode + "\nstderr:\n" + r.StderrText + "\n");

            if (r.Success) return new ConvertResult { Success = true };
            string err = r.StderrText ?? "";
            if (err.Length > 1500) err = err.Substring(err.Length - 1500);
            return new ConvertResult { Success = false, Error = err };
        }

        private static ConversionOptions CloneOptions(ConversionOptions src, string input, string output, bool hasAudio)
        {
            return new ConversionOptions
            {
                InputPath = input,
                OutputPath = output,
                SourceHasAudio = hasAudio,
                IsAudioOutput = src.IsAudioOutput,
                IsStreamCopy = src.IsStreamCopy,
                TargetExt = src.TargetExt,
                StartTime = src.StartTime,
                Duration = src.Duration,
                Preset = src.Preset,
                Crf = src.Crf,
                Resolution = src.Resolution,
                Fps = src.Fps,
                Brightness = src.Brightness,
                Contrast = src.Contrast,
                Saturation = src.Saturation,
                Sharpen = src.Sharpen,
                AudioBitrate = src.AudioBitrate,
                AudioSampleRate = src.AudioSampleRate,
                AudioChannels = src.AudioChannels,
                Volume = src.Volume,
                Speed = src.Speed,
            };
        }

        // ================== 辅助 ==================
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

        private void WriteLog(string text)
        {
            try
            {
                string logPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "log.txt");
                File.AppendAllText(logPath, text + Environment.NewLine, Encoding.UTF8);
            }
            catch { }
        }

        private static string GetUniquePath(string path)
        {
            if (!File.Exists(path)) return path;
            string dir = Path.GetDirectoryName(path) ?? "";
            string name = Path.GetFileNameWithoutExtension(path);
            string ext = Path.GetExtension(path);
            int i = 1;
            string candidate;
            do
            {
                candidate = Path.Combine(dir, name + "_" + i + ext);
                i++;
            } while (File.Exists(candidate));
            return candidate;
        }

        private static string MapFormatToExtension(string format)
        {
            if (format.Contains("MP4"))  return ".mp4";
            if (format.Contains("MKV"))  return ".mkv";
            if (format.Contains("AVI"))  return ".avi";
            if (format.Contains("MP3"))  return ".mp3";
            if (format.Contains("WAV"))  return ".wav";
            if (format.Contains("AAC"))  return ".aac";
            if (format.Contains("FLAC")) return ".flac";
            return ".mp4";
        }

        private string GetPreset()
        {
            switch (comboPreset.SelectedIndex)
            {
                case 1: return "fast";
                case 2: return "slow";
                default: return "medium";
            }
        }

        private int GetCrf()
        {
            switch (comboCRF.SelectedIndex)
            {
                case 0: return 18;
                case 2: return 28;
                default: return 23;
            }
        }

        private string GetResolution()
        {
            switch (comboRes.SelectedIndex)
            {
                case 1: return "1920x1080";
                case 2: return "1280x720";
                case 3: return "854x480";
                default: return null;
            }
        }

        private int GetFps()
        {
            switch (comboFPS.SelectedIndex)
            {
                case 1: return 60;
                case 2: return 30;
                case 3: return 24;
                default: return 0;
            }
        }

        private string GetAudioBitrate()
        {
            switch (comboAudioBitrate.SelectedIndex)
            {
                case 1: return "320k";
                case 2: return "192k";
                case 3: return "128k";
                default: return null;
            }
        }

        private int GetAudioSampleRate()
        {
            switch (comboAudioRate.SelectedIndex)
            {
                case 1: return 48000;
                case 2: return 44100;
                default: return 0;
            }
        }

        private int GetAudioChannels()
        {
            switch (comboChannels.SelectedIndex)
            {
                case 1: return 2;
                case 2: return 1;
                default: return 0;
            }
        }

        private static double ParseDouble(string text, double fallback)
        {
            if (string.IsNullOrWhiteSpace(text)) return fallback;
            double v;
            return double.TryParse(text.Trim(), NumberStyles.Float,
                CultureInfo.InvariantCulture, out v) ? v : fallback;
        }
    }

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