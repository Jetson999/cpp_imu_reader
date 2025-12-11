#include "config_parser.h"
#include <iostream>
#include <iomanip>

int main(int argc, char* argv[]) {
    std::string config_file = "config.ini";
    if (argc > 1) {
        config_file = argv[1];
    }

    std::cout << "=== 配置验证程序 ===" << std::endl;
    std::cout << "配置文件: " << config_file << std::endl;
    std::cout << std::endl;

    ConfigParser config;
    if (!config.load(config_file)) {
        std::cerr << "错误: 无法加载配置文件 " << config_file << std::endl;
        return 1;
    }

    std::cout << "配置读取结果:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 读取 report_rate
    int report_rate = config.getInt("IMU", "report_rate", 60);
    std::cout << "  [IMU] report_rate = " << report_rate << " Hz" << std::endl;
    
    // 读取其他相关配置作为对比
    int device_address = config.getInt("IMU", "device_address", 255);
    std::cout << "  [IMU] device_address = " << device_address << std::endl;
    
    std::string port = config.getString("Serial", "port", "/dev/ttyUSB0");
    std::cout << "  [Serial] port = " << port << std::endl;
    
    std::cout << "----------------------------------------" << std::endl;
    
    // 验证结果
    if (report_rate == 60) {
        std::cout << "✓ 配置验证成功: report_rate 已正确读取为 60 Hz" << std::endl;
        return 0;
    } else {
        std::cout << "✗ 配置验证失败: report_rate 读取为 " << report_rate 
                  << " Hz，期望值为 60 Hz" << std::endl;
        return 1;
    }
}

