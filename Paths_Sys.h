#pragma once

#include <string>

struct Settings {
    std::string temp_path;
    std::string cpu_stat_path;
    std::string meminfo_path;
    std::string loadavg_path;
    std::string gpu_busy_path;
    std::string gpu_stats_path;
    std::string cpu_freq_path;
    std::string fan_rpm_path;
    std::string fan_pwm_path;
    std::string fan_pwm_enable_path;
    std::string gpiomem_path;
    int auto_check_seconds;
    int fan_gpio_pin;
};

inline Settings MakeStartSettings() {
    Settings settings;

    settings.temp_path = "/sys/class/thermal/thermal_zone0/temp";
    settings.cpu_stat_path = "/proc/stat";
    settings.meminfo_path = "/proc/meminfo";
    settings.loadavg_path = "/proc/loadavg";

    settings.gpu_busy_path = "";

    settings.gpu_stats_path = "/sys/devices/platform/axi/1002000000.v3d/gpu_stats";

    settings.cpu_freq_path = "/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq";

    settings.fan_rpm_path = "/sys/devices/platform/cooling_fan/hwmon/hwmon2/fan1_input";
    settings.fan_pwm_path = "/sys/devices/platform/cooling_fan/hwmon/hwmon2/pwm1";
    settings.fan_pwm_enable_path = "/sys/devices/platform/cooling_fan/hwmon/hwmon2/pwm1_enable";

    settings.gpiomem_path = "/dev/gpiomem0";

    settings.auto_check_seconds = 2;

    settings.fan_gpio_pin = 18;

    return settings;
}
