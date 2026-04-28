#pragma once

#include <string>

#include "Paths_Sys.h"

bool SetFanEnableValue(const Settings& settings, int value, std::string& message);

bool SetFanPwmValue(const Settings& settings, int value, std::string& message);

bool HoldFanPwmValue(const Settings& settings, int value);

bool TurnFanOn(const Settings& settings, std::string& message);

bool TurnFanOff(const Settings& settings, std::string& message);

bool SetFanAutoMode(const Settings& settings, std::string& message);

bool SetFanMemoryState(const Settings& settings, bool turn_on, std::string& message);
