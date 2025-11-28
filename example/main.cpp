#include "imu_reader.h"
#include <iostream>
#include <iomanip>
#include <csignal>
#include <atomic>

std::atomic<bool> g_running(true);

// 信号处理
void signalHandler(int signum) {
    std::cout << "\n接收到退出信号，正在关闭..." << std::endl;
    g_running = false;
}

// 数据回调函数
void onIMUData(const IMUData& data) {
    std::cout << "\r" << std::fixed << std::setprecision(3);
    
    // 显示欧拉角
    if (data.subscribe_tag & 0x0040) {
        std::cout << "欧拉角: X=" << std::setw(7) << data.euler_x
                  << " Y=" << std::setw(7) << data.euler_y
                  << " Z=" << std::setw(7) << data.euler_z << "°";
    }
    
    // 显示角速度
    if (data.subscribe_tag & 0x0004) {
        std::cout << " | 角速度: X=" << std::setw(7) << data.gyro_x
                  << " Y=" << std::setw(7) << data.gyro_y
                  << " Z=" << std::setw(7) << data.gyro_z << " dps";
    }
    
    // 显示加速度
    if (data.subscribe_tag & 0x0002) {
        std::cout << " | 加速度: X=" << std::setw(7) << data.accel_with_gravity_x
                  << " Y=" << std::setw(7) << data.accel_with_gravity_y
                  << " Z=" << std::setw(7) << data.accel_with_gravity_z << " m/s²";
    }
    
    std::cout << std::flush;
}

int main(int argc, char* argv[]) {
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 配置文件路径
    std::string config_file = "config.ini";
    if (argc > 1) {
        config_file = argv[1];
    }

    std::cout << "=== IMU数据读取程序 ===" << std::endl;
    std::cout << "配置文件: " << config_file << std::endl;

    // 创建IMU读取器
    IMUReader reader;

    // 初始化
    if (!reader.initialize(config_file)) {
        std::cerr << "初始化失败" << std::endl;
        return 1;
    }

    // 设置数据回调
    reader.setDataCallback(onIMUData);

    // 启动
    if (!reader.start()) {
        std::cerr << "启动失败" << std::endl;
        return 1;
    }

    std::cout << "IMU读取器已启动，按Ctrl+C退出" << std::endl;
    std::cout << "等待数据..." << std::endl;

    // 主循环
    while (g_running && reader.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 显示连接状态
        if (!reader.isConnected()) {
            std::cout << "\r等待连接..." << std::flush;
        }
    }

    // 停止
    reader.stop();
    std::cout << "\n程序已退出" << std::endl;

    return 0;
}

