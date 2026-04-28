#include "FanControl_Sys.h"

#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/mman.h>
#include <unistd.h>

namespace {

const std::size_t MAP_SIZE = 0x30000;

const std::size_t IO_BANK0_OFFSET = 0x00000;

const std::size_t PADS_BANK0_OFFSET = 0x20000;

bool WriteNumberToFile(const std::string& path, int value) {
    if (path.empty()) {
        return false;
    }

    std::ofstream file(path.c_str());
    if (!file.is_open()) {
        return false;
    }

    file << value;
    return !file.fail();
}

bool WriteNumberToFileWithSudo(const std::string& path, int value) {
    if (path.empty()) {
        return false;
    }

    std::stringstream command;
    command << "printf '" << value << "\\n' | sudo tee '" << path << "' >/dev/null";

    return std::system(command.str().c_str()) == 0;
}

bool WriteNumberToFileSmart(const std::string& path, int value) {
    if (WriteNumberToFile(path, value)) {
        return true;
    }

    return WriteNumberToFileWithSudo(path, value);
}

std::size_t GetCtrlIndex(int gpio_pin) {
    std::size_t byte_offset = IO_BANK0_OFFSET + static_cast<std::size_t>(gpio_pin) * 8 + 0x04;
    return byte_offset / 4;
}

std::size_t GetPadIndex(int gpio_pin) {
    std::size_t byte_offset = PADS_BANK0_OFFSET + 0x04 + static_cast<std::size_t>(gpio_pin) * 4;
    return byte_offset / 4;
}

int ClampPwmValue(int value) {
    if (value < 0) {
        return 0;
    }

    if (value > 255) {
        return 255;
    }

    return value;
}

bool SetFanManualModeInternal(const Settings& settings) {
    if (settings.fan_pwm_enable_path.empty()) {
        return true;
    }

    return WriteNumberToFileSmart(settings.fan_pwm_enable_path, 1);
}

}

bool SetFanEnableValue(const Settings& settings, int value, std::string& message) {
    if (settings.fan_pwm_enable_path.empty()) {
        message = "Path pwm1_enable is empty. Set it in Settings.";
        return false;
    }

    if (WriteNumberToFileSmart(settings.fan_pwm_enable_path, value)) {
        message = "Value was written to pwm1_enable.";
        return true;
    }

    message = "Unable to write pwm1_enable. If sudo asked for password, check it.";
    return false;
}

bool SetFanPwmValue(const Settings& settings, int value, std::string& message) {
    if (settings.fan_pwm_path.empty()) {
        message = "Path pwm1 is empty. Set it in Settings.";
        return false;
    }

    if (!SetFanManualModeInternal(settings)) {
        message = "Unable to switch fan to MANUAL mode.";
        return false;
    }

    value = ClampPwmValue(value);

    if (!WriteNumberToFileSmart(settings.fan_pwm_path, value)) {
        message = "Unable to write PWM value. If sudo asked for password, check it.";
        return false;
    }

    if (!SetFanManualModeInternal(settings)) {
        message = "PWM was written, but MANUAL mode was not fixed.";
        return false;
    }

    message = "PWM value was written and MANUAL mode was fixed.";
    return true;
}

bool HoldFanPwmValue(const Settings& settings, int value) {
    if (settings.fan_pwm_path.empty()) {
        return false;
    }

    if (!SetFanManualModeInternal(settings)) {
        return false;
    }

    value = ClampPwmValue(value);

    if (!WriteNumberToFileSmart(settings.fan_pwm_path, value)) {
        return false;
    }

    return SetFanManualModeInternal(settings);
}

bool TurnFanOn(const Settings& settings, std::string& message) {
    if (HoldFanPwmValue(settings, 255)) {
        message = "Fan is ON. PWM = 255.";
        return true;
    }

    message = "Unable to turn fan ON. If sudo asked for password, check it.";
    return false;
}

bool TurnFanOff(const Settings& settings, std::string& message) {
    if (HoldFanPwmValue(settings, 0)) {
        message = "Fan is OFF. PWM = 0.";
        return true;
    }

    message = "Unable to turn fan OFF. If sudo asked for password, check it.";
    return false;
}

bool SetFanAutoMode(const Settings& settings, std::string& message) {
    if (settings.fan_pwm_enable_path.empty()) {
        message = "Path pwm1_enable is empty. Set it in Settings.";
        return false;
    }

    if (!settings.fan_pwm_path.empty()) {
        if (!WriteNumberToFileSmart(settings.fan_pwm_path, 0)) {
            message = "Unable to release old MANUAL PWM value.";
            return false;
        }
    }

    if (!WriteNumberToFileSmart(settings.fan_pwm_enable_path, 2)) {
        message = "Unable to enable auto mode. If sudo asked for password, check it.";
        return false;
    }

    if (!settings.fan_pwm_path.empty()) {
        WriteNumberToFileSmart(settings.fan_pwm_path, 0);
    }

    message = "Fan auto mode was enabled and manual PWM was released.";
    return true;
}

bool SetFanMemoryState(const Settings& settings, bool turn_on, std::string& message) {
    if (settings.fan_gpio_pin < 0 || settings.fan_gpio_pin > 27) {
        message = "GPIO pin must be from 0 to 27.";
        return false;
    }

    int fd = open(settings.gpiomem_path.c_str(), O_RDWR | O_SYNC);
    if (fd < 0) {
        message = "Unable to open gpiomem. Check permissions.";
        return false;
    }

    void* map = mmap(nullptr, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        message = "Unable to map GPIO memory.";
        return false;
    }

    volatile unsigned int* regs = static_cast<volatile unsigned int*>(map);

    std::size_t ctrl_index = GetCtrlIndex(settings.fan_gpio_pin);
    std::size_t pad_index = GetPadIndex(settings.fan_gpio_pin);

    unsigned int ctrl_value = regs[ctrl_index];
    unsigned int pad_value = regs[pad_index];

    pad_value &= ~(1u << 7);
    regs[pad_index] = pad_value;

    ctrl_value &= ~0x1Fu;
    ctrl_value |= 31u;

    ctrl_value &= ~(0x3u << 14);
    ctrl_value |= (0x3u << 14);

    ctrl_value &= ~(0x3u << 12);
    if (turn_on) {
        ctrl_value |= (0x3u << 12);
    } else {
        ctrl_value |= (0x2u << 12);
    }

    regs[ctrl_index] = ctrl_value;

    munmap(map, MAP_SIZE);
    close(fd);

    std::stringstream text;
    text << "GPIO" << settings.fan_gpio_pin
         << (turn_on ? " -> HIGH" : " -> LOW")
         << ". Use this only through a transistor/MOSFET.";
    message = text.str();
    return true;
}
