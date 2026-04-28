#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "AppInfo.h"
#include "FanControl_Sys.h"
#include "Paths_Sys.h"
#include "Status_Sys.h"

const std::string COLOR_RESET = "\033[0m";
const std::string COLOR_BOLD = "\033[1m";

const int BAR_WIDTH = 30;
const int METRIC_NAME_WIDTH = 5;
const int LEFT_VALUE_WIDTH = 12;
const int HISTORY_WIDTH = 28;
const int RIGHT_VALUE_WIDTH = 18;
const int FOOTER_LOAD_COLUMN = 50;
const std::size_t HISTORY_LIMIT = 48;
const int HISTORY_SLOT_WIDTH = 1;
const long long LOG_FILE_LIMIT_BYTES = 1024 * 1024;
const char* LOG_FILE_NAME = "rpi5_fun.log";

struct UiHistory {
    std::vector<double> temp;
    std::vector<double> freq;
    std::vector<double> cpu;
    std::vector<double> gpu;
    std::vector<double> ram;
    std::vector<std::vector<double>> cores;
};

UiHistory& GetUiHistory() {
    static UiHistory history;
    return history;
}

struct FanManualHoldState {
    bool enabled = false;
    int pwm_value = 0;
};

FanManualHoldState& GetFanManualHoldState() {
    static FanManualHoldState state;
    return state;
}

void RememberManualFanHold(int pwm_value) {
    FanManualHoldState& state = GetFanManualHoldState();
    state.enabled = true;
    if (pwm_value < 0) {
        pwm_value = 0;
    }

    if (pwm_value > 255) {
        pwm_value = 255;
    }

    state.pwm_value = pwm_value;
}

void ClearManualFanHold() {
    GetFanManualHoldState().enabled = false;
}

void ApplyManualFanHoldIfNeeded(const Settings& settings) {
    FanManualHoldState& state = GetFanManualHoldState();
    if (!state.enabled) {
        return;
    }

    HoldFanPwmValue(settings, state.pwm_value);
}

std::string Color256(int color_code) {
    return "\033[38;5;" + std::to_string(color_code) + "m";
}

std::string GetFanModeText(const SystemInfo& info);

int GetTerminalColumns() {
    winsize size;
    if (isatty(STDOUT_FILENO) != 0 && ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
        return static_cast<int>(size.ws_col);
    }

    return 100;
}

int GetTerminalRows() {
    winsize size;
    if (isatty(STDOUT_FILENO) != 0 && ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_row > 0) {
        return static_cast<int>(size.ws_row);
    }

    return 30;
}

void ClearScreen() {
    std::cout << "\033[H\033[2J\033[3J";
}

std::string ReadText(const std::string& text) {
    std::cout << text;
    std::string value;
    std::getline(std::cin, value);
    return value;
}

void ClearBadInput() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int ReadNumber(const std::string& text, int min_value, int max_value) {
    while (true) {
        int number = 0;
        std::cout << text;
        std::cin >> number;

        if (!std::cin.fail() && number >= min_value && number <= max_value) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return number;
        }

        ClearBadInput();
        std::cout << "Wrong input. Enter number from "
                  << min_value << " to " << max_value << ".\n";
    }
}

void WaitEnter() {
    ReadText("\nPress Enter to continue...");
}

std::string ToText(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::string MakeSpaces(int count) {
    if (count <= 0) {
        return "";
    }

    return std::string(static_cast<std::size_t>(count), ' ');
}

std::string RepeatText(const std::string& text, int count) {
    if (count <= 0) {
        return "";
    }

    std::string out;
    for (int i = 0; i < count; ++i) {
        out += text;
    }
    return out;
}

int GetVisibleTextWidth(const std::string& text) {
    int width = 0;

    for (std::size_t i = 0; i < text.size(); ++i) {
        unsigned char symbol = static_cast<unsigned char>(text[i]);

        if (symbol == '\033') {
            while (i < text.size() && text[i] != 'm') {
                ++i;
            }
            continue;
        }

        if ((symbol & 0xC0u) != 0x80u) {
            ++width;
        }
    }

    return width;
}

std::string CenterPlainText(const std::string& text, int width) {
    int text_width = GetVisibleTextWidth(text);
    if (text_width >= width) {
        return text;
    }

    int free_space = width - text_width;
    int left_padding = free_space / 2;
    int right_padding = free_space - left_padding;
    return MakeSpaces(left_padding) + text + MakeSpaces(right_padding);
}

void PrintCenteredLine(const std::string& text) {
    int columns = GetTerminalColumns();
    int visible_width = GetVisibleTextWidth(text);
    int left_padding = 0;

    if (columns > visible_width) {
        left_padding = (columns - visible_width) / 2;
    }

    std::cout << MakeSpaces(left_padding) << text << "\n";
}

int LimitInt(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

double LimitDouble(double value, double min_value, double max_value) {
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

void TrimHistory(std::vector<double>& history, std::size_t limit) {
    if (history.size() > limit) {
        history.erase(history.begin(), history.begin() + static_cast<long>(history.size() - limit));
    }
}

void PushHistoryValue(std::vector<double>& history, bool ok, double value) {
    history.push_back(ok ? value : -1.0);
    TrimHistory(history, HISTORY_LIMIT);
}

int GetFilledBlocks(double percent, int width) {
    percent = LimitDouble(percent, 0.0, 100.0);
    int blocks = static_cast<int>(percent * width / 100.0);

    if (percent > 0.0 && blocks == 0) {
        blocks = 1;
    }

    return LimitInt(blocks, 0, width);
}

int GetCpuRampColor(double position_percent) {
    if (position_percent < 45.0) {
        return 120;
    }

    if (position_percent < 70.0) {
        return 228;
    }

    if (position_percent < 88.0) {
        return 214;
    }

    return 196;
}

int GetGpuRampColor(double position_percent) {
    if (position_percent < 50.0) {
        return 111;
    }

    if (position_percent < 80.0) {
        return 75;
    }

    return 39;
}

int GetMemoryRampColor(double position_percent) {
    if (position_percent < 55.0) {
        return 121;
    }

    if (position_percent < 80.0) {
        return 190;
    }

    return 214;
}

int GetTempRampColor(double position_percent) {
    if (position_percent < 20.0) {
        return 81;
    }

    if (position_percent < 45.0) {
        return 121;
    }

    if (position_percent < 70.0) {
        return 228;
    }

    return 196;
}

int GetFreqRampColor(double position_percent) {
    if (position_percent < 30.0) {
        return 117;
    }

    if (position_percent < 55.0) {
        return 121;
    }

    if (position_percent < 80.0) {
        return 228;
    }

    return 196;
}

int PickBarColor(const std::string& theme, double position_percent) {
    if (theme == "gpu") {
        return GetGpuRampColor(position_percent);
    }

    if (theme == "ram") {
        return GetMemoryRampColor(position_percent);
    }

    if (theme == "temp") {
        return GetTempRampColor(position_percent);
    }

    if (theme == "freq") {
        return GetFreqRampColor(position_percent);
    }

    return GetCpuRampColor(position_percent);
}

std::string MakeSquareBar(double percent, int width, const std::string& theme) {
    int filled = GetFilledBlocks(percent, width);
    std::string bar;

    for (int i = 0; i < width; ++i) {
        if (i < filled) {
            double position_percent = static_cast<double>(i + 1) * 100.0 / static_cast<double>(width);
            bar += Color256(PickBarColor(theme, position_percent)) + u8"■";
        } else {
            bar += Color256(236) + u8"■";
        }
    }

    bar += COLOR_RESET;
    return bar;
}

double GetTemperaturePercent(double temp_c) {
    return LimitDouble((temp_c - 20.0) * 100.0 / 65.0, 0.0, 100.0);
}

double GetFrequencyPercent(double ghz) {
    return LimitDouble(ghz * 100.0 / 2.40, 0.0, 100.0);
}

void UpdateUiHistory(const SystemInfo& info) {
    UiHistory& history = GetUiHistory();

    PushHistoryValue(history.temp, info.cpu_temp_ok, info.cpu_temp);
    PushHistoryValue(history.freq, info.cpu_freq_ok, info.cpu_freq_ghz);
    PushHistoryValue(history.cpu, info.cpu_total_ok, info.cpu_total_usage);
    PushHistoryValue(history.gpu, info.gpu_ok, info.gpu_usage);
    PushHistoryValue(history.ram, info.memory_ok, info.memory_usage);

    history.cores.resize(info.cores.size());
    for (std::size_t i = 0; i < info.cores.size(); ++i) {
        PushHistoryValue(history.cores[i], true, info.cores[i].usage_percent);
    }
}

double ToHistoryPercent(const std::string& theme, double value) {
    if (value < 0.0) {
        return -1.0;
    }

    if (theme == "temp") {
        return GetTemperaturePercent(value);
    }

    if (theme == "freq") {
        return GetFrequencyPercent(value);
    }

    return LimitDouble(value, 0.0, 100.0);
}

struct MetricLayout {
    int left_width = LEFT_VALUE_WIDTH;
    int bar_width = BAR_WIDTH;
    int history_width = HISTORY_WIDTH;
    int right_width = RIGHT_VALUE_WIDTH;
};

MetricLayout GetMetricLayout() {
    MetricLayout layout;
    int columns = GetTerminalColumns();

    auto total_width = [&layout]() {
        return METRIC_NAME_WIDTH + 1 +
               layout.left_width + 1 +
               layout.bar_width + 2 +
               layout.history_width * HISTORY_SLOT_WIDTH + 3 +
               layout.right_width;
    };

    while (total_width() > columns - 2 && layout.history_width > 10) {
        --layout.history_width;
    }

    while (total_width() > columns - 2 && layout.bar_width > 18) {
        --layout.bar_width;
    }

    while (total_width() > columns - 2 && layout.left_width > 8) {
        --layout.left_width;
    }

    while (total_width() > columns - 2 && layout.right_width > 10) {
        --layout.right_width;
    }

    while (total_width() > columns - 2 && layout.history_width > 5) {
        --layout.history_width;
    }

    return layout;
}

std::string FitText(const std::string& text, int width) {
    if (width <= 0) {
        return "";
    }

    if (static_cast<int>(text.size()) <= width) {
        return text + std::string(static_cast<std::size_t>(width - static_cast<int>(text.size())), ' ');
    }

    if (width <= 3) {
        return text.substr(0, static_cast<std::size_t>(width));
    }

    return text.substr(0, static_cast<std::size_t>(width - 1)) + ".";
}

int PickHistoryTrafficColor(double percent) {
    if (percent >= 88.0) {
        return 196;
    }

    if (percent >= 55.0) {
        return 228;
    }

    return 120;
}

std::string PickHistoryTrafficGlyph(double percent) {
    if (percent >= 8.0) {
        return COLOR_RESET + Color256(PickHistoryTrafficColor(percent)) + "|";
    }

    return COLOR_RESET + Color256(238) + ".";
}

std::string MakeTrafficHistoryLine(const std::vector<double>& history,
                                   int width,
                                   const std::string& theme) {
    std::string out;

    int padding = width;
    if (static_cast<int>(history.size()) < width) {
        padding = width - static_cast<int>(history.size());
    } else {
        padding = 0;
    }

    for (int i = 0; i < padding; ++i) {
        out += Color256(238) + ".";
    }

    std::size_t start = 0;
    if (history.size() > static_cast<std::size_t>(width)) {
        start = history.size() - static_cast<std::size_t>(width);
    }

    for (std::size_t i = start; i < history.size(); ++i) {
        out += PickHistoryTrafficGlyph(ToHistoryPercent(theme, history[i]));
    }

    out += COLOR_RESET;
    return out;
}

std::string MakeCompactMetricLine(const std::string& name,
                                  bool ok,
                                  double percent,
                                  const std::string& theme,
                                  const std::vector<double>& history,
                                  const std::string& left_text,
                                  const std::string& right_text,
                                  const MetricLayout& layout) {
    std::ostringstream out;
    out << std::left << std::setw(METRIC_NAME_WIDTH) << name << " "
        << FitText(ok ? left_text : "N/A", layout.left_width) << " "
        << MakeSquareBar(ok ? percent : 0.0, layout.bar_width, theme)
        << " "
        << MakeTrafficHistoryLine(history, layout.history_width, theme)
        << "  "
        << FitText(ok ? right_text : "N/A", layout.right_width);
    return out.str();
}

void AppendCompactMetricLine(std::vector<std::string>& lines,
                             const std::string& name,
                             bool ok,
                             double percent,
                             const std::string& theme,
                             const std::vector<double>& history,
                             const std::string& left_text,
                             const std::string& right_text,
                             const MetricLayout& layout) {
    lines.push_back(MakeCompactMetricLine(name, ok, percent, theme, history, left_text, right_text, layout));
}

std::string PadRightText(const std::string& text, int width) {
    if (static_cast<int>(text.size()) >= width) {
        return text + "   ";
    }

    return text + std::string(static_cast<std::size_t>(width - static_cast<int>(text.size())), ' ');
}

bool ResetLogFileIfNeeded(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    long long size = static_cast<long long>(file.tellg());
    if (size <= LOG_FILE_LIMIT_BYTES) {
        return false;
    }

    std::ofstream clear_file(path.c_str(), std::ios::trunc);
    return clear_file.is_open();
}

std::string BuildLogTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm local_time {};
    localtime_r(&now, &local_time);

    std::ostringstream out;
    out << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string GetLogFilePath() {
    return LOG_FILE_NAME;
}

void AppendLogEntry(const std::string& tag, const std::string& text) {
    std::string path = GetLogFilePath();
    bool was_reset = ResetLogFileIfNeeded(path);

    std::ofstream file(path.c_str(), std::ios::app);
    if (!file.is_open()) {
        return;
    }

    if (was_reset) {
        file << BuildLogTimestamp() << " | SYSTEM | Log file reset after 1 MB limit.\n";
    }

    file << BuildLogTimestamp() << " | " << tag << " | " << text << "\n";
}

std::string BuildSystemLogText(const SystemInfo& info) {
    std::ostringstream out;
    out << "TEMP "
        << (info.cpu_temp_ok ? ToText(info.cpu_temp, 1) + "C" : "N/A")
        << " | SPEED "
        << (info.cpu_freq_ok ? ToText(info.cpu_freq_ghz, 2) + " GHz" : "N/A")
        << " | CPU "
        << (info.cpu_total_ok ? ToText(info.cpu_total_usage, 1) + "%" : "N/A")
        << " | LOAD "
        << (info.load_ok ? ToText(info.load1, 2) + " " + ToText(info.load5, 2) + " " + ToText(info.load15, 2)
                         : "N/A")
        << " | FAN "
        << GetFanModeText(info)
        << " | PWM "
        << (info.fan_pwm_ok ? std::to_string(info.fan_pwm) + "/255" : "N/A")
        << " | RPM "
        << (info.fan_rpm_ok ? std::to_string(info.fan_rpm) : "N/A");
    return out.str();
}

void AppendSystemLogEntry(const std::string& source, const SystemInfo& info) {
    AppendLogEntry(source, BuildSystemLogText(info));
}

std::vector<std::string> ReadLogLines() {
    std::vector<std::string> lines;
    std::ifstream file(GetLogFilePath().c_str());
    if (!file.is_open()) {
        return lines;
    }

    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    return lines;
}

std::vector<std::string> BuildLogViewerLines(const Settings& settings) {
    std::vector<std::string> lines;
    std::vector<std::string> file_lines = ReadLogLines();
    int max_visible = std::max(8, GetTerminalRows() - 14);
    int line_width = std::max(40, GetTerminalColumns() - 2);

    lines.push_back("File : " + GetLogFilePath() + "   Max size : 1 MB   Auto refresh : " +
                    std::to_string(settings.auto_check_seconds) + " sec");
    lines.push_back("");

    if (file_lines.empty()) {
        lines.push_back("No log entries yet.");
        return lines;
    }

    std::size_t start = 0;
    if (file_lines.size() > static_cast<std::size_t>(max_visible)) {
        start = file_lines.size() - static_cast<std::size_t>(max_visible);
    }

    for (std::size_t i = start; i < file_lines.size(); ++i) {
        lines.push_back(FitText(file_lines[i], line_width));
    }

    return lines;
}

std::string GetFanModeText(const SystemInfo& info) {
    if (!info.fan_mode_ok) {
        return "N/A";
    }

    if (info.fan_mode == 1) {
        return "MANUAL";
    }

    if (info.fan_mode == 2) {
        return "AUTO";
    }

    if (info.fan_mode == 0) {
        return "OFF";
    }

    return "MODE " + std::to_string(info.fan_mode);
}

void PrintTitle() {
    std::vector<std::string> title_lines;
    title_lines.push_back(u8"██████╗ ██████╗ ██╗███████╗   ███████╗██╗   ██╗███╗   ██╗    ██████╗████████╗██████╗ ██╗     ");
    title_lines.push_back(u8"██╔══██╗██╔══██╗██║██╔════╝   ██╔════╝██║   ██║████╗  ██║   ██╔════╝╚══██╔══╝██╔══██╗██║     ");
    title_lines.push_back(u8"██████╔╝██████╔╝██║███████╗   █████╗  ██║   ██║██╔██╗ ██║   ██║        ██║   ██████╔╝██║     ");
    title_lines.push_back(u8"██╔══██╗██╔═══╝ ██║╚════██║   ██╔══╝  ██║   ██║██║╚██╗██║   ██║        ██║   ██╔══██╗██║     ");
    title_lines.push_back(u8"██║  ██║██║     ██║███████║   ██║     ╚██████╔╝██║ ╚████║   ╚██████╗   ██║   ██║  ██║███████╗");
    title_lines.push_back(u8"╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝   ╚═╝      ╚═════╝ ╚═╝  ╚═══╝    ╚═════╝   ╚═╝   ╚═╝  ╚═╝╚══════╝");

    for (std::size_t i = 0; i < title_lines.size(); ++i) {
        int color = 51;
        if (i == 1) color = 50;
        if (i == 2) color = 49;
        if (i == 3) color = 48;
        if (i == 4) color = 47;
        if (i == 5) color = 46;
        if (title_lines[i].empty()) {
            std::cout << "\n";
            continue;
        }

        PrintCenteredLine(" " + Color256(color) + title_lines[i] + COLOR_RESET);
    }

    PrintCenteredLine(" " + COLOR_BOLD + getAppName() + COLOR_RESET +
                      " | " + getAppVersion());
    PrintCenteredLine(" " + Color256(240) + RepeatText("=", 85) + COLOR_RESET);
}

void PrintMainMenuPanel() {
    const std::vector<std::string> lines = {
        u8"┌──────────────────────────────────────────────────┐",
        u8"│            Main Menu by DeziXsteroid             │",
        u8"│                                                  │",
        u8"│ 1. Start Auto Check              2. Fans Control │",
        u8"│ 3. Logs                          4. Settings     │",
        u8"│ 5. Help And Info                 6. Exit         │",
        u8"└──────────────────────────────────────────────────┘"
    };

    for (std::size_t i = 0; i < lines.size(); ++i) {
        PrintCenteredLine(lines[i]);
    }
}

std::vector<std::string> BuildStatusLines(const SystemInfo& info) {
    std::vector<std::string> lines;
    UiHistory& history = GetUiHistory();
    MetricLayout layout = GetMetricLayout();

    if (info.cpu_temp_ok) {
        AppendCompactMetricLine(lines, "TEMP", true, GetTemperaturePercent(info.cpu_temp), "temp",
                                history.temp,
                                ToText(info.cpu_temp, 1) + "C",
                                ToText(info.cpu_temp, 1) + " C",
                                layout);
    } else {
        AppendCompactMetricLine(lines, "TEMP", false, 0.0, "temp", history.temp, "N/A", "N/A", layout);
    }

    if (info.cpu_freq_ok) {
        AppendCompactMetricLine(lines, "SPEED", true, GetFrequencyPercent(info.cpu_freq_ghz), "freq",
                                history.freq,
                                ToText(info.cpu_freq_ghz, 2) + "GHz",
                                ToText(info.cpu_freq_ghz, 2) + " GHz",
                                layout);
    } else {
        AppendCompactMetricLine(lines, "SPEED", false, 0.0, "freq", history.freq, "N/A", "N/A", layout);
    }

    AppendCompactMetricLine(lines, "CPU", info.cpu_total_ok, info.cpu_total_usage, "cpu",
                            history.cpu,
                            info.cpu_total_ok ? ToText(info.cpu_total_usage, 1) + "%" : "N/A",
                            info.cpu_total_ok ? ToText(info.cpu_total_usage, 1) + "%" : "N/A",
                            layout);

    for (std::size_t i = 0; i < info.cores.size(); ++i) {
        AppendCompactMetricLine(lines, info.cores[i].name, true, info.cores[i].usage_percent, "cpu",
                                history.cores[i],
                                ToText(info.cores[i].usage_percent, 1) + "%",
                                ToText(info.cores[i].usage_percent, 1) + "%",
                                layout);
    }

    AppendCompactMetricLine(lines, "GPU", info.gpu_ok, info.gpu_usage, "gpu",
                            history.gpu,
                            info.gpu_ok ? ToText(info.gpu_usage, 1) + "%" : "N/A",
                            info.gpu_ok ? ToText(info.gpu_usage, 1) + "%" : "N/A",
                            layout);

    if (info.memory_ok) {
        AppendCompactMetricLine(lines, "RAM", true, info.memory_usage, "ram",
                                history.ram,
                                ToText(info.memory_usage, 1) + "%",
                                std::to_string(info.memory_used_mb) + "/" +
                                std::to_string(info.memory_total_mb) + " MB",
                                layout);
    } else {
        AppendCompactMetricLine(lines, "RAM", false, 0.0, "ram", history.ram, "N/A", "N/A", layout);
    }

    std::string fan_info = "Fan Info : ";
    if (info.fan_rpm_ok) {
        fan_info += std::to_string(info.fan_rpm) + " RPM";
    } else {
        fan_info += "RPM N/A";
    }

    fan_info += "   ";

    if (info.fan_pwm_ok) {
        fan_info += "PWM " + std::to_string(info.fan_pwm) + "/255";
    } else {
        fan_info += "PWM N/A";
    }

    fan_info += "   Mode: " + GetFanModeText(info);

    std::string load_info;
    if (info.load_ok) {
        load_info = "Load AVG : " + ToText(info.load1, 2) + "   " +
                    ToText(info.load5, 2) + "   " +
                    ToText(info.load15, 2);
    } else {
        load_info = "Load AVG : N/A";
    }

    if (static_cast<int>(fan_info.size() + load_info.size() + 3) <= GetTerminalColumns()) {
        lines.push_back(PadRightText(fan_info, FOOTER_LOAD_COLUMN) + load_info);
    } else {
        lines.push_back(fan_info);
        lines.push_back(load_info);
    }
    return lines;
}

void PrintStatusLines(const std::vector<std::string>& lines) {
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::cout << "\033[2K\r" << lines[i] << "\n";
    }

    std::cout << "\033[J";
    std::cout.flush();
}

void MoveCursorUp(int lines_count) {
    if (lines_count > 0) {
        std::cout << "\033[" << lines_count << "A";
    }
}

bool TurnOnRawMode(termios& old_settings) {
    if (isatty(STDIN_FILENO) == 0) {
        return false;
    }

    if (tcgetattr(STDIN_FILENO, &old_settings) != 0) {
        return false;
    }

    termios new_settings = old_settings;

    new_settings.c_lflag &= static_cast<unsigned long>(~(ICANON | ECHO));
    new_settings.c_cc[VMIN] = 0;
    new_settings.c_cc[VTIME] = 0;

    return tcsetattr(STDIN_FILENO, TCSANOW, &new_settings) == 0;
}

void TurnOffRawMode(const termios& old_settings, bool was_enabled) {
    if (was_enabled) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
    }
}

bool KeyWasPressed() {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    return select(STDIN_FILENO + 1, &set, nullptr, nullptr, &tv) > 0;
}

std::string ReadInputChunk() {
    std::string chunk;
    char buffer[64];

    while (true) {
        ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (count <= 0) {
            break;
        }

        chunk.append(buffer, static_cast<std::size_t>(count));

        if (count < static_cast<ssize_t>(sizeof(buffer))) {
            break;
        }

        if (!KeyWasPressed()) {
            break;
        }
    }

    return chunk;
}

bool InputHasStopCommand(const std::string& input_text) {
    if (input_text.find("q") != std::string::npos) {
        return true;
    }

    if (input_text.find("Q") != std::string::npos) {
        return true;
    }

    if (input_text.find(u8"й") != std::string::npos) {
        return true;
    }

    if (input_text.find(u8"Й") != std::string::npos) {
        return true;
    }

    if (input_text.find('\x1B') != std::string::npos) {
        return true;
    }

    return false;
}

int ExtractDigitCommand(const std::string& input_text, int min_value, int max_value) {
    for (std::size_t i = 0; i < input_text.size(); ++i) {
        char symbol = input_text[i];
        if (symbol < '0' || symbol > '9') {
            continue;
        }

        int value = symbol - '0';
        if (value >= min_value && value <= max_value) {
            return value;
        }
    }

    return -1;
}

void ChangePath(std::string& path, const std::string& path_name) {
    std::cout << "Old " << path_name << ": " << path << "\n";
    std::cout << "Enter new " << path_name << ": ";
    std::getline(std::cin, path);
}

void StartAutoCheck(const Settings& settings) {
    ClearScreen();
    PrintTitle();
    std::cout << u8"AUTO CHECK MODE | Press Q or Й to stop\n";
    std::cout << "\n";

    std::cout << "\033[?25l";
    std::cout.flush();

    termios old_settings;
    bool raw_mode = TurnOnRawMode(old_settings);
    std::string input_tail;
    bool first_draw = true;
    int previous_line_count = 0;

    while (true) {
        if (KeyWasPressed()) {
            input_tail += ReadInputChunk();

            if (InputHasStopCommand(input_tail)) {
                TurnOffRawMode(old_settings, raw_mode);
                std::cout << "\033[?25h";
                std::cout.flush();
                return;
            }

            if (input_tail.size() > 8) {
                input_tail = input_tail.substr(input_tail.size() - 8);
            }
        }

        ApplyManualFanHoldIfNeeded(settings);
        SystemInfo info = ReadSystemInfo(settings);
        UpdateUiHistory(info);
        AppendSystemLogEntry("AUTO", info);
        std::vector<std::string> lines = BuildStatusLines(info);

        if (!first_draw) {
            MoveCursorUp(previous_line_count);
        }

        PrintStatusLines(lines);
        previous_line_count = static_cast<int>(lines.size());
        first_draw = false;

        int wait_ms = settings.auto_check_seconds * 1000;

        for (int passed = 0; passed < wait_ms; passed += 50) {
            if (KeyWasPressed()) {
                input_tail += ReadInputChunk();

                if (InputHasStopCommand(input_tail)) {
                    TurnOffRawMode(old_settings, raw_mode);
                    std::cout << "\033[?25h";
                    std::cout.flush();
                    return;
                }

                if (input_tail.size() > 8) {
                    input_tail = input_tail.substr(input_tail.size() - 8);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void ShowLogsMenu(const Settings& settings) {
    termios old_settings;
    bool raw_mode = TurnOnRawMode(old_settings);
    std::string input_tail;
    bool need_full_redraw = true;
    int previous_line_count = 0;

    std::cout << "\033[?25l";
    std::cout.flush();

    while (true) {
        if (need_full_redraw) {
            ClearScreen();
            PrintTitle();
            std::cout << "LOGS | Press 0, Q or Й to back\n\n";
        } else if (previous_line_count > 0) {
            MoveCursorUp(previous_line_count);
        }

        std::vector<std::string> lines = BuildLogViewerLines(settings);
        PrintStatusLines(lines);
        previous_line_count = static_cast<int>(lines.size());
        need_full_redraw = false;

        int wait_ms = settings.auto_check_seconds * 1000;
        if (wait_ms < 300) {
            wait_ms = 300;
        }

        for (int passed = 0; passed < wait_ms; passed += 50) {
            if (KeyWasPressed()) {
                input_tail += ReadInputChunk();

                if (InputHasStopCommand(input_tail)) {
                    TurnOffRawMode(old_settings, raw_mode);
                    std::cout << "\033[?25h";
                    std::cout.flush();
                    return;
                }

                int command = ExtractDigitCommand(input_tail, 0, 0);
                if (command == 0) {
                    TurnOffRawMode(old_settings, raw_mode);
                    std::cout << "\033[?25h";
                    std::cout.flush();
                    return;
                }

                if (input_tail.size() > 8) {
                    input_tail = input_tail.substr(input_tail.size() - 8);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void ShowSettingsMenu(Settings& settings) {
    while (true) {
        ApplyManualFanHoldIfNeeded(settings);
        ClearScreen();
        PrintTitle();
        std::cout << "SETTINGS\n\n";
        std::cout << "Default start paths are stored in Paths_Sys.h\n";
        std::cout << "Here you can change them during current program run.\n";
        std::cout << "Auto refresh timer is used by monitor and live fan screen.\n\n";

        std::cout << "1.  temp_path           = " << settings.temp_path << "\n";
        std::cout << "2.  cpu_stat_path       = " << settings.cpu_stat_path << "\n";
        std::cout << "3.  meminfo_path        = " << settings.meminfo_path << "\n";
        std::cout << "4.  loadavg_path        = " << settings.loadavg_path << "\n";
        std::cout << "5.  gpu_busy_path       = " << settings.gpu_busy_path << "\n";
        std::cout << "6.  gpu_stats_path      = " << settings.gpu_stats_path << "\n";
        std::cout << "7.  cpu_freq_path       = " << settings.cpu_freq_path << "\n";
        std::cout << "8.  fan_rpm_path        = " << settings.fan_rpm_path << "\n";
        std::cout << "9.  fan_pwm_path        = " << settings.fan_pwm_path << "\n";
        std::cout << "10. fan_pwm_enable_path = " << settings.fan_pwm_enable_path << "\n";
        std::cout << "11. auto_refresh_seconds = " << settings.auto_check_seconds << "\n";
        std::cout << "12. Reset default settings\n";
        std::cout << "0. Back\n\n";

        int choice = ReadNumber("Choose a number: ", 0, 12);

        if (choice == 0) {
            return;
        }

        if (choice == 1) {
            ChangePath(settings.temp_path, "temp_path");
        } else if (choice == 2) {
            ChangePath(settings.cpu_stat_path, "cpu_stat_path");
        } else if (choice == 3) {
            ChangePath(settings.meminfo_path, "meminfo_path");
        } else if (choice == 4) {
            ChangePath(settings.loadavg_path, "loadavg_path");
        } else if (choice == 5) {
            ChangePath(settings.gpu_busy_path, "gpu_busy_path");
        } else if (choice == 6) {
            ChangePath(settings.gpu_stats_path, "gpu_stats_path");
        } else if (choice == 7) {
            ChangePath(settings.cpu_freq_path, "cpu_freq_path");
        } else if (choice == 8) {
            ChangePath(settings.fan_rpm_path, "fan_rpm_path");
        } else if (choice == 9) {
            ChangePath(settings.fan_pwm_path, "fan_pwm_path");
        } else if (choice == 10) {
            ChangePath(settings.fan_pwm_enable_path, "fan_pwm_enable_path");
        } else if (choice == 11) {
            settings.auto_check_seconds = ReadNumber("New auto_refresh_seconds (1..60): ", 1, 60);
        } else if (choice == 12) {
            settings = MakeStartSettings();
        }

        std::cout << "\nNew value saved in program memory.\n";
        WaitEnter();
    }
}

std::vector<std::string> BuildFanLines(const Settings& settings,
                                       const SystemInfo& info,
                                       const std::string& action_message) {
    std::vector<std::string> lines;
    UiHistory& history = GetUiHistory();
    MetricLayout layout = GetMetricLayout();

    lines.push_back("Auto refresh : " + std::to_string(settings.auto_check_seconds) + " sec");
    lines.push_back("");

    AppendCompactMetricLine(lines, "TEMP", info.cpu_temp_ok,
                            info.cpu_temp_ok ? GetTemperaturePercent(info.cpu_temp) : 0.0,
                            "temp",
                            history.temp,
                            info.cpu_temp_ok ? ToText(info.cpu_temp, 1) + "C" : "N/A",
                            info.cpu_temp_ok ? ToText(info.cpu_temp, 1) + " C" : "N/A",
                            layout);

    lines.push_back("");
    lines.push_back("Current mode : " + GetFanModeText(info));
    lines.push_back(std::string("Current PWM  : ") +
                    (info.fan_pwm_ok ? std::to_string(info.fan_pwm) + " / 255" : "N/A"));
    lines.push_back(std::string("Current RPM  : ") +
                    (info.fan_rpm_ok ? std::to_string(info.fan_rpm) + " RPM" : "N/A"));

    if (!action_message.empty()) {
        lines.push_back("");
        lines.push_back("Last action  : " + action_message);
    }

    lines.push_back("");
    lines.push_back("Keys: 1 ON | 2 OFF | 3 SET PWM | 4 AUTO | 0 BACK");
    return lines;
}

void ShowFanMenu(const Settings& settings) {
    termios old_settings;
    bool raw_mode = TurnOnRawMode(old_settings);
    std::string input_tail;
    std::string action_message;
    bool need_full_redraw = true;
    int previous_line_count = 0;

    std::cout << "\033[?25l";
    std::cout.flush();

    while (true) {
        ApplyManualFanHoldIfNeeded(settings);
        if (need_full_redraw) {
            ClearScreen();
            PrintTitle();
            std::cout << "FANS CONTROL | Press 0, Q or Й to back\n\n";
        } else if (previous_line_count > 0) {
            MoveCursorUp(previous_line_count);
        }

        SystemInfo info = ReadSystemInfo(settings);
        UpdateUiHistory(info);
        AppendSystemLogEntry("FAN", info);

        std::vector<std::string> lines = BuildFanLines(settings, info, action_message);
        PrintStatusLines(lines);
        previous_line_count = static_cast<int>(lines.size());
        need_full_redraw = false;

        int wait_ms = settings.auto_check_seconds * 1000;
        if (wait_ms < 300) {
            wait_ms = 300;
        }

        bool redraw_now = false;

        for (int passed = 0; passed < wait_ms; passed += 50) {
            if (KeyWasPressed()) {
                input_tail += ReadInputChunk();

                if (InputHasStopCommand(input_tail)) {
                    TurnOffRawMode(old_settings, raw_mode);
                    std::cout << "\033[?25h";
                    std::cout.flush();
                    return;
                }

                int command = ExtractDigitCommand(input_tail, 0, 4);
                if (command == 0) {
                    TurnOffRawMode(old_settings, raw_mode);
                    std::cout << "\033[?25h";
                    std::cout.flush();
                    return;
                }

                if (command != -1) {
                    input_tail.clear();

                    if (command == 1) {
                        if (TurnFanOn(settings, action_message)) {
                            RememberManualFanHold(255);
                            AppendLogEntry("FAN", action_message);
                        }
                    } else if (command == 2) {
                        if (TurnFanOff(settings, action_message)) {
                            RememberManualFanHold(0);
                            AppendLogEntry("FAN", action_message);
                        }
                    } else if (command == 3) {
                        TurnOffRawMode(old_settings, raw_mode);
                        std::cout << "\033[?25h";
                        std::cout.flush();

                        int value = ReadNumber("\nEnter PWM speed (0..255): ", 0, 255);
                        if (SetFanPwmValue(settings, value, action_message)) {
                            RememberManualFanHold(value);
                            AppendLogEntry("FAN", action_message);
                        }

                        raw_mode = TurnOnRawMode(old_settings);
                        std::cout << "\033[?25l";
                        std::cout.flush();
                        need_full_redraw = true;
                    } else if (command == 4) {
                        if (SetFanAutoMode(settings, action_message)) {
                            ClearManualFanHold();
                            AppendLogEntry("FAN", action_message);
                        }
                    }

                    need_full_redraw = true;
                    redraw_now = true;
                    break;
                }

                if (input_tail.size() > 8) {
                    input_tail = input_tail.substr(input_tail.size() - 8);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (redraw_now) {
            continue;
        }
    }
}

void ShowHelp() {
    ClearScreen();
    PrintTitle();
    std::cout << "HELP\n\n";
    std::cout << "1. Auto Check is now the main monitoring mode.\n";
    std::cout << "2. Right history graph uses equal-height lines with color levels and small gaps.\n";
    std::cout << "3. Press Q or Й to stop Auto Check.\n";
    std::cout << "4. Fan screen auto-refreshes with the same timer as monitor.\n";
    std::cout << "5. Logs screen stores fan actions and live telemetry, auto-clears after 1 MB.\n";
    std::cout << "6. GPU load is read from gpu_busy_path, and if it is empty then from gpu_stats_path.\n";
    WaitEnter();
}

int main() {
    Settings settings = MakeStartSettings();

    while (true) {
        ApplyManualFanHoldIfNeeded(settings);
        ClearScreen();
        PrintTitle();
        std::cout << "\n";
        PrintMainMenuPanel();
        std::cout << "\n";

        int choice = ReadNumber("Choose a number: ", 1, 6);

        if (choice == 1) {
            StartAutoCheck(settings);
        } else if (choice == 2) {
            ShowFanMenu(settings);
        } else if (choice == 3) {
            ShowLogsMenu(settings);
        } else if (choice == 4) {
            ShowSettingsMenu(settings);
        } else if (choice == 5) {
            ShowHelp();
        } else if (choice == 6) {
            break;
        }
    }

    return 0;
}
