#include "Status_Sys.h"

#include "Temperature_Sys.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

namespace {

struct CpuStatLine {
    std::string name;
    unsigned long long user = 0;
    unsigned long long nice = 0;
    unsigned long long system = 0;
    unsigned long long idle = 0;
    unsigned long long iowait = 0;
    unsigned long long irq = 0;
    unsigned long long softirq = 0;
    unsigned long long steal = 0;
};

struct GpuStatsSnapshot {
    bool ok = false;
    unsigned long long timestamp = 0;
    unsigned long long total_runtime = 0;
};

struct PreviousLoadData {
    bool ready = false;
    std::vector<CpuStatLine> cpu;
    GpuStatsSnapshot gpu;
};

double LimitPercent(double value) {
    if (value < 0.0) {
        return 0.0;
    }

    if (value > 100.0) {
        return 100.0;
    }

    return value;
}

bool ReadLongLongFromFile(const std::string& path, long long& value) {
    if (path.empty()) {
        return false;
    }

    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        return false;
    }

    file >> value;
    return !file.fail();
}

bool ReadCpuFrequencyGHz(const std::string& path, double& ghz) {
    long long freq_khz = 0;
    if (!ReadLongLongFromFile(path, freq_khz)) {
        return false;
    }

    ghz = static_cast<double>(freq_khz) / 1000000.0;
    return true;
}

std::vector<CpuStatLine> ReadCpuStat(const std::string& path) {
    std::vector<CpuStatLine> list;

    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        return list;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("cpu") != 0) {
            break;
        }

        std::stringstream parser(line);
        CpuStatLine cpu;
        parser >> cpu.name
               >> cpu.user
               >> cpu.nice
               >> cpu.system
               >> cpu.idle
               >> cpu.iowait
               >> cpu.irq
               >> cpu.softirq
               >> cpu.steal;

        if (!parser.fail()) {
            list.push_back(cpu);
        }
    }

    return list;
}

unsigned long long GetTotalCpuTime(const CpuStatLine& cpu) {
    return cpu.user + cpu.nice + cpu.system + cpu.idle +
           cpu.iowait + cpu.irq + cpu.softirq + cpu.steal;
}

unsigned long long GetIdleCpuTime(const CpuStatLine& cpu) {
    return cpu.idle + cpu.iowait;
}

double CalculateCpuUsage(const CpuStatLine& first, const CpuStatLine& second) {
    unsigned long long total1 = GetTotalCpuTime(first);
    unsigned long long total2 = GetTotalCpuTime(second);

    unsigned long long idle1 = GetIdleCpuTime(first);
    unsigned long long idle2 = GetIdleCpuTime(second);

    unsigned long long total_delta = total2 - total1;
    unsigned long long idle_delta = idle2 - idle1;

    if (total_delta == 0) {
        return 0.0;
    }

    double busy = static_cast<double>(total_delta - idle_delta);
    return LimitPercent((busy * 100.0) / static_cast<double>(total_delta));
}

bool ReadMemory(const std::string& path, long& total_mb, long& used_mb, long& free_mb, double& usage) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        return false;
    }

    long total_kb = 0;
    long free_kb = 0;
    long available_kb = 0;

    std::string key;
    long value = 0;
    std::string unit;

    while (file >> key >> value >> unit) {
        if (key == "MemTotal:") {
            total_kb = value;
        } else if (key == "MemFree:") {
            free_kb = value;
        } else if (key == "MemAvailable:") {
            available_kb = value;
        }
    }

    if (total_kb <= 0) {
        return false;
    }

    if (available_kb <= 0) {
        available_kb = free_kb;
    }

    long used_kb = total_kb - available_kb;

    total_mb = total_kb / 1024;
    used_mb = used_kb / 1024;
    free_mb = available_kb / 1024;
    usage = LimitPercent((static_cast<double>(used_kb) * 100.0) / static_cast<double>(total_kb));
    return true;
}

bool ReadLoadAverage(const std::string& path, double& load1, double& load5, double& load15) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        return false;
    }

    file >> load1 >> load5 >> load15;
    return !file.fail();
}

GpuStatsSnapshot ReadGpuStats(const std::string& path) {
    GpuStatsSnapshot snapshot;

    if (path.empty()) {
        return snapshot;
    }

    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        return snapshot;
    }

    std::string header;
    std::getline(file, header);

    std::string queue_name;
    unsigned long long timestamp = 0;
    unsigned long long jobs = 0;
    unsigned long long runtime = 0;

    while (file >> queue_name >> timestamp >> jobs >> runtime) {
        snapshot.ok = true;
        snapshot.timestamp = timestamp;

        if (queue_name != "cpu") {
            snapshot.total_runtime += runtime;
        }
    }

    return snapshot;
}

double CalculateGpuUsage(const GpuStatsSnapshot& first, const GpuStatsSnapshot& second) {
    if (!first.ok || !second.ok) {
        return -1.0;
    }

    if (second.timestamp <= first.timestamp) {
        return 0.0;
    }

    if (second.total_runtime < first.total_runtime) {
        return 0.0;
    }

    unsigned long long time_delta = second.timestamp - first.timestamp;
    unsigned long long runtime_delta = second.total_runtime - first.total_runtime;

    double usage = static_cast<double>(runtime_delta) * 100.0 / static_cast<double>(time_delta);
    return LimitPercent(usage);
}

PreviousLoadData& GetPreviousLoadData() {
    static PreviousLoadData data;
    return data;
}

}

SystemInfo ReadSystemInfo(const Settings& settings) {
    SystemInfo info;

    PreviousLoadData& previous = GetPreviousLoadData();

    std::vector<CpuStatLine> first_cpu;
    std::vector<CpuStatLine> second_cpu;
    GpuStatsSnapshot first_gpu_stats;
    GpuStatsSnapshot second_gpu_stats;

    if (previous.ready) {
        first_cpu = previous.cpu;
        first_gpu_stats = previous.gpu;
        second_cpu = ReadCpuStat(settings.cpu_stat_path);
        second_gpu_stats = ReadGpuStats(settings.gpu_stats_path);

        if (first_cpu.empty() || first_cpu.size() != second_cpu.size()) {
            first_cpu = ReadCpuStat(settings.cpu_stat_path);
            first_gpu_stats = ReadGpuStats(settings.gpu_stats_path);
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
            second_cpu = ReadCpuStat(settings.cpu_stat_path);
            second_gpu_stats = ReadGpuStats(settings.gpu_stats_path);
        }
    } else {
        first_cpu = ReadCpuStat(settings.cpu_stat_path);
        first_gpu_stats = ReadGpuStats(settings.gpu_stats_path);

        std::this_thread::sleep_for(std::chrono::milliseconds(600));

        second_cpu = ReadCpuStat(settings.cpu_stat_path);
        second_gpu_stats = ReadGpuStats(settings.gpu_stats_path);
    }

    previous.ready = true;
    previous.cpu = second_cpu;
    previous.gpu = second_gpu_stats;

    double temp = ReadCpuTemperature(settings.temp_path);
    if (temp >= 0.0) {
        info.cpu_temp_ok = true;
        info.cpu_temp = temp;
    }

    double freq_ghz = 0.0;
    if (ReadCpuFrequencyGHz(settings.cpu_freq_path, freq_ghz)) {
        info.cpu_freq_ok = true;
        info.cpu_freq_ghz = freq_ghz;
    }

    if (!first_cpu.empty() && first_cpu.size() == second_cpu.size()) {
        info.cpu_total_ok = true;
        info.cpu_total_usage = CalculateCpuUsage(first_cpu[0], second_cpu[0]);

        for (std::size_t i = 1; i < first_cpu.size(); ++i) {
            CpuCoreInfo core;
            core.name = "C" + std::to_string(i - 1);
            core.usage_percent = CalculateCpuUsage(first_cpu[i], second_cpu[i]);
            info.cores.push_back(core);
        }
    }

    long long gpu_value = 0;
    if (ReadLongLongFromFile(settings.gpu_busy_path, gpu_value)) {
        info.gpu_ok = true;
        info.gpu_usage = LimitPercent(static_cast<double>(gpu_value));
    } else {
        double gpu_usage = CalculateGpuUsage(first_gpu_stats, second_gpu_stats);
        if (gpu_usage >= 0.0) {
            info.gpu_ok = true;
            info.gpu_usage = gpu_usage;
        }
    }

    if (ReadMemory(settings.meminfo_path,
                   info.memory_total_mb,
                   info.memory_used_mb,
                   info.memory_free_mb,
                   info.memory_usage)) {
        info.memory_ok = true;
    }

    if (ReadLoadAverage(settings.loadavg_path, info.load1, info.load5, info.load15)) {
        info.load_ok = true;
    }

    long long fan_rpm_value = 0;
    if (ReadLongLongFromFile(settings.fan_rpm_path, fan_rpm_value)) {
        info.fan_rpm_ok = true;
        info.fan_rpm = static_cast<int>(fan_rpm_value);
    }

    long long fan_pwm_value = 0;
    if (ReadLongLongFromFile(settings.fan_pwm_path, fan_pwm_value)) {
        info.fan_pwm_ok = true;
        info.fan_pwm = static_cast<int>(fan_pwm_value);
    }

    long long fan_mode_value = 0;
    if (ReadLongLongFromFile(settings.fan_pwm_enable_path, fan_mode_value)) {
        info.fan_mode_ok = true;
        info.fan_mode = static_cast<int>(fan_mode_value);
    }

    return info;
}
