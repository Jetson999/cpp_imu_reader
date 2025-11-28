/*
    * @file config_parser.h
    * @brief 配置文件解析器头文件
    *
    * Author : Jetson LV <ljhao1994@163.com>
    * Created: 2025-11-27
*/
#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>

// 配置文件解析器
class ConfigParser {
public:
    ConfigParser() = default;
    ~ConfigParser() = default;

    // 加载配置文件
    bool load(const std::string& filename);

    // 获取字符串值
    std::string getString(const std::string& section, const std::string& key, const std::string& default_value = "");

    // 获取整数值
    int getInt(const std::string& section, const std::string& key, int default_value = 0);

    // 获取浮点数值
    float getFloat(const std::string& section, const std::string& key, float default_value = 0.0f);

    // 获取布尔值
    bool getBool(const std::string& section, const std::string& key, bool default_value = false);

private:
    std::map<std::string, std::map<std::string, std::string>> config_data_;

    // 去除字符串首尾空白
    std::string trim(const std::string& str);
};

#endif // CONFIG_PARSER_H

