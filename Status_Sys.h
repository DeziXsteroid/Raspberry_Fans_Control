#pragma once

#include <string>
#include <vector>

#include "Paths_Sys.h"

struct CpuCoreInfo {
    std::string name;
    double usage_percent;
};

struct SystemInfo {
    bool cpu_temp_ok = false;
    double cpu_temp = 0.0;

    bool cpu_freq_ok = false;
    double cpu_freq_ghz = 0.0;

    bool cpu_total_ok = false;
    double cpu_total_usage = 0.0;

    bool gpu_ok = false;
    double gpu_usage = 0.0;

    bool memory_ok = false;
    long memory_total_mb = 0;
    long memory_used_mb = 0;
    long memory_free_mb = 0;
    double memory_usage = 0.0;

    bool load_ok = false;
    double load1 = 0.0;
    double load5 = 0.0;
    double load15 = 0.0;

    bool fan_rpm_ok = false;
    int fan_rpm = 0;

    bool fan_pwm_ok = false;
    int fan_pwm = 0;

    bool fan_mode_ok = false;
    int fan_mode = 0;

    std::vector<CpuCoreInfo> cores;
};

SystemInfo ReadSystemInfo(const Settings& settings);
