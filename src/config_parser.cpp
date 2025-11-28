/**
 * @file config_parser.cpp
 * @brief IMU 配置文件解析器实现
 *
 * Author : Jetson LV <ljhao1994@163.com>
 * Created: 2025-11-27
 * description: imu配置文件解析器实现  
 *
 * 简要说明：
 *   实现对 INI 风格配置文件的解析
 *   支持节 [Section]、键=值、注释行（# 或 ;）、空行跳过；支持十六进制整数、布尔值识别等
 */
#include "config_parser.h"
#include <algorithm>
#include <cctype>

bool ConfigParser::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "无法打开配置文件: " << filename << std::endl;
        return false;
    }

    std::string current_section;
    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);
        
        // 跳过空行和注释
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // 解析节名 [Section]
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }

        // 解析键值对 key=value
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            config_data_[current_section][key] = value;
        }
    }

    file.close();
    return true;
}

std::string ConfigParser::getString(const std::string& section, const std::string& key, const std::string& default_value) {
    auto sec_it = config_data_.find(section);
    if (sec_it == config_data_.end()) {
        return default_value;
    }

    auto key_it = sec_it->second.find(key);
    if (key_it == sec_it->second.end()) {
        return default_value;
    }

    return key_it->second;
}

int ConfigParser::getInt(const std::string& section, const std::string& key, int default_value) {
    std::string value = getString(section, key);
    if (value.empty()) {
        return default_value;
    }

    // 支持十六进制
    if (value.length() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        return static_cast<int>(std::stoul(value, nullptr, 16));
    }

    return std::stoi(value);
}

float ConfigParser::getFloat(const std::string& section, const std::string& key, float default_value) {
    std::string value = getString(section, key);
    if (value.empty()) {
        return default_value;
    }
    return std::stof(value);
}

bool ConfigParser::getBool(const std::string& section, const std::string& key, bool default_value) {
    std::string value = getString(section, key);
    if (value.empty()) {
        return default_value;
    }

    // 转换为小写
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    
    return (value == "1" || value == "true" || value == "yes" || value == "on");
}

std::string ConfigParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

