#include "Temperature_Sys.h"

#include <fstream>

double ReadCpuTemperature(const std::string& temp_path) {
    long temp_value = 0;

    std::ifstream file(temp_path.c_str());
    if (!file.is_open()) {
        return -1.0;
    }

    if (!(file >> temp_value)) {
        return -1.0;
    }

    return temp_value / 1000.0;
}
