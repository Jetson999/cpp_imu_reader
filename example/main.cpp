#include "imu_reader.h"
#include <iostream>
#include <iomanip>
#include <csignal>
#include <atomic>
#include <chrono>
#include <mutex>

std::atomic<bool> g_running(true);

// 频率统计变量（线程安全）
struct FrequencyStats {
    std::mutex mutex;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point window_start_time;
    uint64_t total_count = 0;
    uint64_t window_count = 0;
    uint64_t last_window_count = 0;
    bool initialized = false;
    
    void init() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) {
            auto now = std::chrono::steady_clock::now();
            start_time = now;
            window_start_time = now;
            total_count = 0;
            window_count = 0;
            last_window_count = 0;
            initialized = true;
        }
    }
    
    void update() {
        std::lock_guard<std::mutex> lock(mutex);
        total_count++;
        window_count++;
    }
    
    void getFrequencies(double& avg_freq, double& instant_freq) {
        std::lock_guard<std::mutex> lock(mutex);
        auto now = std::chrono::steady_clock::now();
        
        // 计算平均频率（从开始到现在）
        avg_freq = 0.0;
        if (initialized && total_count > 0) {
            auto total_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            if (total_elapsed_ms > 0) {
                avg_freq = (total_count * 1000.0) / total_elapsed_ms;
            }
        }
        
        // 计算瞬时频率（最近1秒窗口）
        instant_freq = 0.0;
        auto window_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - window_start_time).count();
        if (window_elapsed_ms >= 1000) {  // 至少1秒
            uint64_t count_in_window = window_count - last_window_count;
            instant_freq = (count_in_window * 1000.0) / window_elapsed_ms;
            // 重置窗口
            window_start_time = now;
            last_window_count = window_count;
        } else if (window_elapsed_ms > 100) {  // 至少100ms才计算瞬时频率
            uint64_t count_in_window = window_count - last_window_count;
            instant_freq = (count_in_window * 1000.0) / window_elapsed_ms;
        }
    }
};

FrequencyStats freq_stats;

// 信号处理
void signalHandler(int signum) {
    std::cout << "\n接收到退出信号，正在关闭..." << std::endl;
    g_running = false;
}

// 数据回调函数
void onIMUData(const IMUData& data) {
    // 初始化统计（只在第一次调用时）
    freq_stats.init();
    
    // 更新计数
    freq_stats.update();
    
    // 获取频率
    double avg_freq = 0.0;
    double instant_freq = 0.0;
    freq_stats.getFrequencies(avg_freq, instant_freq);
    
    std::cout << "\r" << std::fixed << std::setprecision(3);
    
    // 显示频率信息
    std::cout << "[频率: 平均=" << std::setw(6) << std::setprecision(2) << avg_freq 
              << " Hz";
    if (instant_freq > 0) {
        std::cout << ", 瞬时=" << std::setw(6) << std::setprecision(2) << instant_freq << " Hz";
    }
    std::cout << "] ";
    
    // 显示欧拉角
    if (data.subscribe_tag & 0x0040) {
        std::cout << "欧拉角: X=" << std::setw(7) << std::setprecision(3) << data.euler_x
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

