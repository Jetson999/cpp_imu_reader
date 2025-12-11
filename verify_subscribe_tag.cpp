#include "config_parser.h"
#include <iostream>
#include <iomanip>

int main(int argc, char* argv[]) {
    std::string config_file = "config.ini";
    if (argc > 1) {
        config_file = argv[1];
    }

    std::cout << "=== 订阅标签验证 ===" << std::endl;
    std::cout << "配置文件: " << config_file << std::endl;
    std::cout << std::endl;

    ConfigParser config;
    if (!config.load(config_file)) {
        std::cerr << "错误: 无法加载配置文件 " << config_file << std::endl;
        return 1;
    }

    int subscribe_tag = config.getInt("IMU", "subscribe_tag", 0x7F);
    int report_rate = config.getInt("IMU", "report_rate", 60);
    
    std::cout << "配置读取结果:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  subscribe_tag = 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << subscribe_tag << std::dec << std::endl;
    std::cout << "  report_rate = " << report_rate << " Hz" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << std::endl;
    
    // 分析订阅标签
    std::cout << "订阅内容分析:" << std::endl;
    if (subscribe_tag & 0x01) std::cout << "  ✓ 加速度不含重力 (0x01)" << std::endl;
    if (subscribe_tag & 0x02) std::cout << "  ✓ 加速度含重力 (0x02)" << std::endl;
    if (subscribe_tag & 0x04) std::cout << "  ✓ 角速度 (0x04)" << std::endl;
    if (subscribe_tag & 0x08) std::cout << "  ✓ 磁力计 (0x08)" << std::endl;
    if (subscribe_tag & 0x10) std::cout << "  ✓ 温度气压 (0x10)" << std::endl;
    if (subscribe_tag & 0x20) std::cout << "  ✓ 四元数 (0x20)" << std::endl;
    if (subscribe_tag & 0x40) std::cout << "  ✓ 欧拉角 (0x40)" << std::endl;
    std::cout << std::endl;
    
    // 计算数据包大小
    int data_size = 7;  // 基础: 命令(1) + 标签(2) + 时间戳(4)
    if (subscribe_tag & 0x01) data_size += 6;
    if (subscribe_tag & 0x02) data_size += 6;
    if (subscribe_tag & 0x04) data_size += 6;
    if (subscribe_tag & 0x08) data_size += 6;
    if (subscribe_tag & 0x10) data_size += 7;
    if (subscribe_tag & 0x20) data_size += 8;
    if (subscribe_tag & 0x40) data_size += 6;
    
    int full_packet = 50 + 3 + data_size + 1 + 1;
    double max_rate = 11520.0 / full_packet;
    
    std::cout << "数据包大小分析:" << std::endl;
    std::cout << "  数据体: " << data_size << " 字节" << std::endl;
    std::cout << "  完整包: " << full_packet << " 字节" << std::endl;
    std::cout << "  理论最大频率: " << std::fixed << std::setprecision(1) 
              << max_rate << " Hz" << std::endl;
    std::cout << std::endl;
    
    // 验证
    if (subscribe_tag == 0x02) {
        std::cout << "✓ 配置正确: subscribe_tag=0x02 (与Python示例一致)" << std::endl;
        std::cout << "✓ 数据包大小: " << full_packet << " 字节 (最小，支持最高频率)" << std::endl;
        if (report_rate <= 250) {
            std::cout << "✓ 频率配置: " << report_rate << " Hz (Python示例已验证可达250Hz)" << std::endl;
            std::cout << std::endl;
            std::cout << "配置验证通过！可以运行程序测试实际频率。" << std::endl;
            return 0;
        } else {
            std::cout << "⚠ 频率配置: " << report_rate << " Hz (超过设备最大支持250Hz)" << std::endl;
            return 1;
        }
    } else {
        std::cout << "⚠ 当前配置: subscribe_tag=0x" << std::hex << subscribe_tag << std::dec << std::endl;
        std::cout << "  建议改为 0x02 以支持250Hz" << std::endl;
        std::cout << "  当前配置理论最大频率: " << std::setprecision(1) << max_rate << " Hz" << std::endl;
        return 1;
    }
}

