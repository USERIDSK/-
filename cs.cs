using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading;
using System.Windows.Forms;

namespace MediaConverterApp
{
    public class MainForm : Form
    {
        // UI 控件
        private ListBox listFiles;
        private ComboBox comboFilter, comboFormat, comboPreset, comboCRF, comboRes, comboFPS, comboAudioBitrate, comboAudioRate, comboChannels;
        private TextBox txtOutputDir, txtStartTime, txtDuration, txtSpeed, txtBrightness, txtContrast, txtSaturation, txtSharpen, txtVolume;
        private Button btnAddFiles, btnAddFolder, btnRemove, btnClear, btnFilter, btnSelectOut, btnConvert;
        private Label lblStatus;
        private ProgressBar progressBar;

        // 数据
        private List<string> fileList = new List<string>();
        private string ffmpegPath = "";

        public MainForm()
        {
            InitializeUI();
            ffmpegPath = GetFFmpegPath();
            if (string.IsNullOrEmpty(ffmpegPath) || !File.Exists(ffmpegPath))
            {
                MessageBox.Show("未找到 ffmpeg.exe！\n请确保它在本程序目录下，或已配置系统环境变量。", "警告", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        // ================== 辅助函数：获取 FFmpeg 路径 ==================
        private string GetFFmpegPath()
        {
            string localPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "ffmpeg.exe");
            if (File.Exists(localPath)) return localPath;

            string pathEnv = Environment.GetEnvironmentVariable("PATH");
            if (pathEnv != null)
            {
                foreach (string path in pathEnv.Split(';'))
                {
                    try
                    {
                        string fullPath = Path.Combine(path.Trim(), "ffmpeg.exe");
                        if (File.Exists(fullPath)) return fullPath;
                    }
                    catch { }
                }
            }
            return "ffmpeg.exe";
        }

        // ================== 初始化界面 ==================
        private void InitializeUI()
        {
            this.Text = "音视频批量转换工具 (C# 完整版)";
            this.Size = new Size(820, 680);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.MinimumSize = new Size(780, 650);
            this.Font = new Font("Microsoft YaHei", 9F);

            // 顶部按钮区
            Panel topPanel = new Panel();
            topPanel.Dock = DockStyle.Top;
            topPanel.Height = 45;
            this.Controls.Add(topPanel);

            btnAddFiles = new Button() { Text = "添加文件", Location = new Point(10, 10), Size = new Size(80, 25) };
            btnAddFolder = new Button() { Text = "添加文件夹", Location = new Point(100, 10), Size = new Size(90, 25) };
            btnRemove = new Button() { Text = "移除选中", Location = new Point(200, 10), Size = new Size(80, 25) };
            btnClear = new Button() { Text = "清空列表", Location = new Point(290, 10), Size = new Size(80, 25) };
            comboFilter = new ComboBox() { Location = new Point(390, 10), Size = new Size(100, 25), DropDownStyle = ComboBoxStyle.DropDownList };
            btnFilter = new Button() { Text = "过滤并全选", Location = new Point(500, 10), Size = new Size(90, 25) };

            topPanel.Controls.Add(btnAddFiles);
            topPanel.Controls.Add(btnAddFolder);
            topPanel.Controls.Add(btnRemove);
            topPanel.Controls.Add(btnClear);
            topPanel.Controls.Add(comboFilter);
            topPanel.Controls.Add(btnFilter);

            // 文件列表
            listFiles = new ListBox();
            listFiles.Location = new Point(10, 55);
            listFiles.Size = new Size(780, 180);
            listFiles.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            listFiles.SelectionMode = SelectionMode.MultiExtended;
            this.Controls.Add(listFiles);

            // 参数面板
            Panel paramPanel = new Panel();
            paramPanel.Location = new Point(10, 245);
            paramPanel.Size = new Size(780, 320);
            paramPanel.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right | AnchorStyles.Bottom;
            this.Controls.Add(paramPanel);

            int y = 10;
            // 输出目录
            paramPanel.Controls.Add(new Label() { Text = "输出目录:", Location = new Point(10, y + 3), Size = new Size(60, 20) });
            txtOutputDir = new TextBox() { Location = new Point(80, y), Size = new Size(600, 22), Anchor = AnchorStyles.Left | AnchorStyles.Right };
            btnSelectOut = new Button() { Text = "选择...", Location = new Point(690, y), Size = new Size(80, 22), Anchor = AnchorStyles.Right };
            paramPanel.Controls.Add(txtOutputDir);
            paramPanel.Controls.Add(btnSelectOut);
            y += 35;

            // 第一行参数
            paramPanel.Controls.Add(new Label() { Text = "输出格式:", Location = new Point(10, y + 3), Size = new Size(60, 20) });
            comboFormat = new ComboBox() { Location = new Point(80, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            paramPanel.Controls.Add(comboFormat);
            
            paramPanel.Controls.Add(new Label() { Text = "编码预设:", Location = new Point(200, y + 3), Size = new Size(60, 20) });
            comboPreset = new ComboBox() { Location = new Point(260, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboPreset.Items.AddRange(new object[] { "medium (默认)", "fast (快速)", "slow (慢速)" });
            comboPreset.SelectedIndex = 0;
            paramPanel.Controls.Add(comboPreset);

            paramPanel.Controls.Add(new Label() { Text = "画质(CRF):", Location = new Point(380, y + 3), Size = new Size(60, 20) });
            comboCRF = new ComboBox() { Location = new Point(440, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboCRF.Items.AddRange(new object[] { "18 (高画质)", "23 (均衡)", "28 (压缩优先)" });
            comboCRF.SelectedIndex = 1;
            paramPanel.Controls.Add(comboCRF);
            y += 35;

            // 第二行参数
            paramPanel.Controls.Add(new Label() { Text = "分辨率:", Location = new Point(10, y + 3), Size = new Size(60, 20) });
            comboRes = new ComboBox() { Location = new Point(80, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboRes.Items.AddRange(new object[] { "保持原样", "1920x1080", "1280x720", "854x480" });
            comboRes.SelectedIndex = 0;
            paramPanel.Controls.Add(comboRes);

            paramPanel.Controls.Add(new Label() { Text = "帧率(FPS):", Location = new Point(200, y + 3), Size = new Size(60, 20) });
            comboFPS = new ComboBox() { Location = new Point(260, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboFPS.Items.AddRange(new object[] { "保持原样", "60", "30", "24" });
            comboFPS.SelectedIndex = 0;
            paramPanel.Controls.Add(comboFPS);

            paramPanel.Controls.Add(new Label() { Text = "音频码率:", Location = new Point(380, y + 3), Size = new Size(60, 20) });
            comboAudioBitrate = new ComboBox() { Location = new Point(440, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboAudioBitrate.Items.AddRange(new object[] { "默认/自动", "320 kbps", "192 kbps", "128 kbps" });
            comboAudioBitrate.SelectedIndex = 0;
            paramPanel.Controls.Add(comboAudioBitrate);
            y += 35;

            // 第三行参数
            paramPanel.Controls.Add(new Label() { Text = "采样率:", Location = new Point(10, y + 3), Size = new Size(60, 20) });
            comboAudioRate = new ComboBox() { Location = new Point(80, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboAudioRate.Items.AddRange(new object[] { "保持原样", "48000 Hz", "44100 Hz" });
            comboAudioRate.SelectedIndex = 0;
            paramPanel.Controls.Add(comboAudioRate);

            paramPanel.Controls.Add(new Label() { Text = "声道:", Location = new Point(200, y + 3), Size = new Size(60, 20) });
            comboChannels = new ComboBox() { Location = new Point(260, y), Size = new Size(100, 22), DropDownStyle = ComboBoxStyle.DropDownList };
            comboChannels.Items.AddRange(new object[] { "保持原样", "立体声(2)", "单声道(1)" });
            comboChannels.SelectedIndex = 0;
            paramPanel.Controls.Add(comboChannels);

            paramPanel.Controls.Add(new Label() { Text = "音量倍数:", Location = new Point(380, y + 3), Size = new Size(60, 20) });
            txtVolume = new TextBox() { Location = new Point(440, y), Size = new Size(100, 22) };
            paramPanel.Controls.Add(txtVolume);
            y += 35;

            // 第四行参数（滤镜）
            paramPanel.Controls.Add(new Label() { Text = "亮度(-1~1):", Location = new Point(10, y + 3), Size = new Size(70, 20) });
            txtBrightness = new TextBox() { Location = new Point(80, y), Size = new Size(50, 22) };
            paramPanel.Controls.Add(txtBrightness);
            paramPanel.Controls.Add(new Label() { Text = "对比度(0~3):", Location = new Point(140, y + 3), Size = new Size(70, 20) });
            txtContrast = new TextBox() { Location = new Point(210, y), Size = new Size(50, 22) };
            paramPanel.Controls.Add(txtContrast);
            paramPanel.Controls.Add(new Label() { Text = "饱和度(0~3):", Location = new Point(270, y + 3), Size = new Size(70, 20) });
            txtSaturation = new TextBox() { Location = new Point(340, y), Size = new Size(50, 22) };
            paramPanel.Controls.Add(txtSaturation);
            paramPanel.Controls.Add(new Label() { Text = "锐化(0~5):", Location = new Point(400, y + 3), Size = new Size(60, 20) });
            txtSharpen = new TextBox() { Location = new Point(460, y), Size = new Size(50, 22) };
            paramPanel.Controls.Add(txtSharpen);
            paramPanel.Controls.Add(new Label() { Text = "倍速:", Location = new Point(520, y + 3), Size = new Size(40, 20) });
            txtSpeed = new TextBox() { Location = new Point(560, y), Size = new Size(50, 22) };
            paramPanel.Controls.Add(txtSpeed);
            y += 35;

            // 第五行参数（裁剪）
            paramPanel.Controls.Add(new Label() { Text = "起始时间:", Location = new Point(10, y + 3), Size = new Size(60, 20) });
            txtStartTime = new TextBox() { Location = new Point(80, y), Size = new Size(100, 22) };
            paramPanel.Controls.Add(txtStartTime);
            paramPanel.Controls.Add(new Label() { Text = "持续时长:", Location = new Point(190, y + 3), Size = new Size(60, 20) });
            txtDuration = new TextBox() { Location = new Point(260, y), Size = new Size(100, 22) };
            paramPanel.Controls.Add(txtDuration);

            // 底部状态栏
            Panel bottomPanel = new Panel();
            bottomPanel.Dock = DockStyle.Bottom;
            bottomPanel.Height = 60;
            this.Controls.Add(bottomPanel);

            btnConvert = new Button() { Text = "开始转换", Location = new Point(10, 15), Size = new Size(100, 30) };
            lblStatus = new Label() { Text = "状态：就绪", Location = new Point(120, 20), Size = new Size(400, 20) };
            progressBar = new ProgressBar() { Location = new Point(530, 15), Size = new Size(200, 25), Maximum = 100, Anchor = AnchorStyles.Right | AnchorStyles.Top };

            bottomPanel.Controls.Add(btnConvert);
            bottomPanel.Controls.Add(lblStatus);
            bottomPanel.Controls.Add(progressBar);

            // 绑定事件
            btnAddFiles.Click += BtnAddFiles_Click;
            btnAddFolder.Click += BtnAddFolder_Click;
            btnRemove.Click += BtnRemove_Click;
            btnClear.Click += BtnClear_Click;
            btnFilter.Click += BtnFilter_Click;
            btnConvert.Click += BtnConvert_Click;
            btnSelectOut.Click += BtnSelectOut_Click;
            listFiles.SelectedIndexChanged += ListFiles_SelectedIndexChanged;
            comboFormat.SelectedIndexChanged += (sender, e) => UpdateControlStates(); // 格式改变时刷新置灰状态

            // 初始状态：没有文件时禁用格式下拉框
            comboFormat.Enabled = false;
        }

        // ================== 动态置灰逻辑 ==================
        private void UpdateControlStates()
        {
            if (comboFormat.SelectedItem == null) return;
            string fmtStr = comboFormat.SelectedItem.ToString();
            bool isAudio = fmtStr.Contains("音频");

            // 视频参数置灰
            comboPreset.Enabled = !isAudio;
            comboCRF.Enabled = !isAudio;
            comboRes.Enabled = !isAudio;
            comboFPS.Enabled = !isAudio;
            txtBrightness.Enabled = !isAudio;
            txtContrast.Enabled = !isAudio;
            txtSaturation.Enabled = !isAudio;
            txtSharpen.Enabled = !isAudio;

            // 音频参数始终可用（视频也有音频轨）
            comboAudioBitrate.Enabled = true;
            comboAudioRate.Enabled = true;
            comboChannels.Enabled = true;
            txtVolume.Enabled = true;
        }

        // ================== 扫描文件夹 ==================
        private void ScanFolder(string folder)
        {
            try
            {
                string[] files = Directory.GetFiles(folder, "*.*", SearchOption.AllDirectories);
                string[] validExts = { ".mp4", ".avi", ".mkv", ".mov", ".webm", ".mp3", ".wav", ".aac", ".flac", ".m4a" };
                int count = 0;
                foreach (string f in files)
                {
                    string ext = Path.GetExtension(f).ToLower();
                    if (validExts.Contains(ext))
                    {
                        fileList.Add(f);
                        count++;
                    }
                }
                RefreshListBox();
                MessageBox.Show(string.Format("扫描完成！\n共发现并添加 {0} 个文件到队列。", count), "提示", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show("扫描文件夹出错: " + ex.Message, "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
            var exts = fileList.Select(f => Path.GetExtension(f).ToLower()).Distinct().OrderBy(e => e);
            foreach (var ext in exts)
            {
                comboFilter.Items.Add(ext);
            }
            if (comboFilter.Items.Count > 0) comboFilter.SelectedIndex = 0;
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
            string ext = Path.GetExtension(selectedFile).ToLower();

            comboFormat.Items.Clear();
            bool isVideo = (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".mov" || ext == ".webm");

            if (isVideo)
            {
                if (ext != ".mp4") comboFormat.Items.Add("MP4 (视频)");
                if (ext != ".mkv") comboFormat.Items.Add("MKV (视频)");
                if (ext != ".avi") comboFormat.Items.Add("AVI (视频)");
                comboFormat.Items.Add("MP3 (音频)");
                comboFormat.Items.Add("WAV (音频)");
                comboFormat.Items.Add("AAC (音频)");
                comboFormat.Items.Add("FLAC (音频)");
            }
            else
            {
                if (ext != ".mp3") comboFormat.Items.Add("MP3 (音频)");
                if (ext != ".wav") comboFormat.Items.Add("WAV (音频)");
                if (ext != ".aac") comboFormat.Items.Add("AAC (音频)");
                if (ext != ".flac") comboFormat.Items.Add("FLAC (音频)");
            }
            if (comboFormat.Items.Count > 0) comboFormat.SelectedIndex = 0;

            UpdateControlStates(); // 刷新后立即检查置灰状态
        }

        // ================== 事件处理 ==================
        private void BtnAddFiles_Click(object sender, EventArgs e)
        {
            OpenFileDialog ofd = new OpenFileDialog();
            ofd.Multiselect = true;
            ofd.Filter = "所有文件|*.*|视频文件|*.mp4;*.avi;*.mkv;*.mov;*.webm|音频文件|*.mp3;*.wav;*.aac;*.flac;*.m4a";
            if (ofd.ShowDialog() == DialogResult.OK)
            {
                fileList.AddRange(ofd.FileNames);
                RefreshListBox();
                lblStatus.Text = "状态：添加了 " + ofd.FileNames.Length + " 个文件";
            }
        }

        private void BtnAddFolder_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog fbd = new FolderBrowserDialog();
            if (fbd.ShowDialog() == DialogResult.OK)
            {
                lblStatus.Text = "状态：正在扫描文件夹...";
                Application.DoEvents();
                ScanFolder(fbd.SelectedPath);
            }
        }

        private void BtnRemove_Click(object sender, EventArgs e)
        {
            if (listFiles.SelectedItems.Count == 0) return;
            for (int i = listFiles.SelectedIndices.Count - 1; i >= 0; i--)
            {
                fileList.RemoveAt(listFiles.SelectedIndices[i]);
            }
            RefreshListBox();
        }

        private void BtnClear_Click(object sender, EventArgs e)
        {
            fileList.Clear();
            RefreshListBox();
            lblStatus.Text = "状态：列表已清空";
            comboFormat.Items.Clear();
            comboFormat.Enabled = false;
        }

        private void BtnFilter_Click(object sender, EventArgs e)
        {
            if (comboFilter.SelectedItem == null) return;
            string targetExt = comboFilter.SelectedItem.ToString();
            
            List<string> newList = new List<string>();
            foreach (string f in fileList)
            {
                if (Path.GetExtension(f).ToLower() == targetExt)
                {
                    newList.Add(f);
                }
            }

            if (newList.Count == 0)
            {
                MessageBox.Show("过滤后没有剩余文件！", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            fileList = newList;
            RefreshListBox();

            for (int i = 0; i < listFiles.Items.Count; i++)
            {
                listFiles.SetSelected(i, true);
            }
            lblStatus.Text = "状态：已过滤出 " + newList.Count + " 个 " + targetExt + " 文件";
        }

        private void BtnSelectOut_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog fbd = new FolderBrowserDialog();
            if (fbd.ShowDialog() == DialogResult.OK)
            {
                txtOutputDir.Text = fbd.SelectedPath;
            }
        }

        // ================== 核心：开始转换 ==================
        private void BtnConvert_Click(object sender, EventArgs e)
        {
            if (fileList.Count == 0) { MessageBox.Show("队列为空！"); return; }
            if (string.IsNullOrEmpty(txtOutputDir.Text)) { MessageBox.Show("请选择输出目录！"); return; }
            if (comboFormat.SelectedItem == null) { MessageBox.Show("请选择输出格式！"); return; }

            string targetExt = ".mp4";
            string fmtStr = comboFormat.SelectedItem.ToString();
            if (fmtStr.Contains("MP4")) targetExt = ".mp4";
            else if (fmtStr.Contains("MKV")) targetExt = ".mkv";
            else if (fmtStr.Contains("AVI")) targetExt = ".avi";
            else if (fmtStr.Contains("MP3")) targetExt = ".mp3";
            else if (fmtStr.Contains("WAV")) targetExt = ".wav";
            else if (fmtStr.Contains("AAC")) targetExt = ".aac";
            else if (fmtStr.Contains("FLAC")) targetExt = ".flac";

            btnConvert.Enabled = false;
            progressBar.Value = 0;
            progressBar.Maximum = fileList.Count;

            string outDir = txtOutputDir.Text;
            List<string> filesToConvert = new List<string>(fileList);

            // 抓取 UI 参数
            string startTime = txtStartTime.Text;
            string duration = txtDuration.Text;
            string speedStr = txtSpeed.Text;
            string brightStr = txtBrightness.Text;
            string contrastStr = txtContrast.Text;
            string satStr = txtSaturation.Text;
            string sharpenStr = txtSharpen.Text;
            string volumeStr = txtVolume.Text;
            int presetIdx = comboPreset.SelectedIndex;
            int crfIdx = comboCRF.SelectedIndex;
            int resIdx = comboRes.SelectedIndex;
            int fpsIdx = comboFPS.SelectedIndex;
            int audioBitrateIdx = comboAudioBitrate.SelectedIndex;
            int audioRateIdx = comboAudioRate.SelectedIndex;
            int channelsIdx = comboChannels.SelectedIndex;

            Thread t = new Thread(delegate() {
                int success = 0;
                int skipped = 0;
                bool isAudioOutput = (targetExt == ".mp3" || targetExt == ".wav" || targetExt == ".aac" || targetExt == ".flac");

                for (int i = 0; i < filesToConvert.Count; i++)
                {
                    string srcPath = filesToConvert[i];
                    string srcExt = Path.GetExtension(srcPath).ToLower();
                    bool isSrcVideo = (srcExt == ".mp4" || srcExt == ".avi" || srcExt == ".mkv" || srcExt == ".mov" || srcExt == ".webm");

                    if (!isSrcVideo && !isAudioOutput) { skipped++; continue; }

                    this.Invoke((MethodInvoker)delegate {
                        lblStatus.Text = string.Format("状态：正在转换 ({0}/{1})...", i + 1, filesToConvert.Count);
                        progressBar.Value = i;
                    });

                    string fileName = Path.GetFileNameWithoutExtension(srcPath);
                    string targetPath = Path.Combine(outDir, fileName + targetExt);

                    int counter = 1;
                    while (File.Exists(targetPath))
                    {
                        targetPath = Path.Combine(outDir, fileName + "_" + counter + targetExt);
                        counter++;
                    }

                    // ========== 核心：拼接 FFmpeg 参数 ==========
                    string args = string.Format("-i \"{0}\" ", srcPath);
                    
                    if (!string.IsNullOrEmpty(startTime)) args += string.Format("-ss {0} ", startTime);
                    if (!string.IsNullOrEmpty(duration)) args += string.Format("-t {0} ", duration);

                    if (isAudioOutput)
                    {
                        args += "-vn ";
                        if (targetExt == ".mp3") args += "-acodec libmp3lame ";
                        else if (targetExt == ".wav") args += "-acodec pcm_s16le ";
                        else if (targetExt == ".aac") args += "-acodec aac ";
                        else if (targetExt == ".flac") args += "-acodec flac ";

                        if (audioBitrateIdx == 1) args += "-b:a 320k ";
                        else if (audioBitrateIdx == 2) args += "-b:a 192k ";
                        else if (audioBitrateIdx == 3) args += "-b:a 128k ";
                        
                        if (audioRateIdx == 1) args += "-ar 48000 ";
                        else if (audioRateIdx == 2) args += "-ar 44100 ";
                        
                        if (channelsIdx == 1) args += "-ac 2 ";
                        else if (channelsIdx == 2) args += "-ac 1 ";

                        string audioFilters = "";
                        if (!string.IsNullOrEmpty(volumeStr) && volumeStr != "1.0" && volumeStr != "1")
                            audioFilters += "volume=" + volumeStr;

                        if (!string.IsNullOrEmpty(speedStr))
                        {
                            double speed = 0;
                            double.TryParse(speedStr, out speed);
                            if (speed >= 0.5 && speed <= 2.0)
                            {
                                if (audioFilters != "") audioFilters += ",";
                                audioFilters += "atempo=" + speedStr;
                            }
                            else if (speed > 2.0 && speed <= 4.0)
                            {
                                if (audioFilters != "") audioFilters += ",";
                                audioFilters += "atempo=2.0,atempo=" + (speed / 2.0).ToString("0.0");
                            }
                        }
                        if (audioFilters != "") args += string.Format("-af \"{0}\" ", audioFilters);
                    }
                    else
                    {
                        args += "-c:v libx264 ";
                        
                        string preset = "medium";
                        if (presetIdx == 1) preset = "fast";
                        else if (presetIdx == 2) preset = "slow";
                        args += string.Format("-preset {0} ", preset);

                        string crf = "23";
                        if (crfIdx == 0) crf = "18";
                        else if (crfIdx == 2) crf = "28";
                        args += string.Format("-crf {0} ", crf);

                        if (resIdx == 1) args += "-s 1920x1080 ";
                        else if (resIdx == 2) args += "-s 1280x720 ";
                        else if (resIdx == 3) args += "-s 854x480 ";

                        if (fpsIdx == 1) args += "-r 60 ";
                        else if (fpsIdx == 2) args += "-r 30 ";
                        else if (fpsIdx == 3) args += "-r 24 ";

                        string videoFilters = "";
                        if (!string.IsNullOrEmpty(brightStr) && brightStr != "0")
                            videoFilters += "eq=brightness=" + brightStr + ":";
                        
                        if (!string.IsNullOrEmpty(contrastStr) && contrastStr != "1.0" && contrastStr != "1")
                        {
                            if (videoFilters == "") videoFilters += "eq=contrast=" + contrastStr + ":";
                            else videoFilters += "contrast=" + contrastStr + ":";
                        }
                        
                        if (!string.IsNullOrEmpty(satStr) && satStr != "1.0" && satStr != "1")
                        {
                            if (videoFilters == "") videoFilters += "eq=saturation=" + satStr;
                            else videoFilters += "saturation=" + satStr;
                        }
                        
                        if (videoFilters != "" && videoFilters.EndsWith(":"))
                            videoFilters = videoFilters.Substring(0, videoFilters.Length - 1);
                        
                        if (!string.IsNullOrEmpty(sharpenStr))
                        {
                            if (videoFilters != "") videoFilters += ",";
                            videoFilters += "unsharp=5:5:" + sharpenStr;
                        }

                        bool hasSpeed = !string.IsNullOrEmpty(speedStr);
                        if (hasSpeed)
                        {
                            double speed = 0;
                            double.TryParse(speedStr, out speed);
                            if (speed >= 0.5 && speed <= 2.0)
                            {
                                if (videoFilters != "")
                                    args += string.Format("-filter_complex \"[0:v]{0},setpts=1/{1}*PTS[v];[0:a]atempo={1}[a]\" -map \"[v]\" -map \"[a]\" ", videoFilters, speedStr);
                                else
                                    args += string.Format("-filter_complex \"[0:v]setpts=1/{0}*PTS[v];[0:a]atempo={0}[a]\" -map \"[v]\" -map \"[a]\" ", speedStr);
                            }
                        }
                        else
                        {
                            if (videoFilters != "") args += string.Format("-vf \"{0}\" ", videoFilters);
                        }

                        args += "-c:a aac -b:a 192k ";
                        if (targetExt == ".mp4") args += "-movflags +faststart ";
                    }
                    
                    args += string.Format("\"{0}\" -y", targetPath);
                    // ============================================

                    ProcessStartInfo psi = new ProcessStartInfo();
                    psi.FileName = ffmpegPath;
                    psi.Arguments = args;
                    psi.UseShellExecute = false;
                    psi.CreateNoWindow = true;

                    try
                    {
                        using (Process p = Process.Start(psi))
                        {
                            p.WaitForExit();
                            if (p.ExitCode == 0) success++;
                        }
                    }
                    catch { }
                }

                this.Invoke((MethodInvoker)delegate {
                    btnConvert.Enabled = true;
                    progressBar.Value = progressBar.Maximum;
                    string finalMsg = string.Format("批量转换完成！\n成功: {0} / {1}", success, filesToConvert.Count);
                    if (skipped > 0) finalMsg += "\n跳过（音频无法转视频）: " + skipped;
                    MessageBox.Show(finalMsg, "完成", MessageBoxButtons.OK, MessageBoxIcon.Information);
                    lblStatus.Text = "状态：批量转换完成！";
                });
            });
            t.IsBackground = true;
            t.Start();
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