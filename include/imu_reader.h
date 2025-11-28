/*
    * @file imu_reader.h
    * @brief IMU 读取器头文件
    *
    * Author : Jetson LV <ljhao1994@163.com>
    * Created: 2025-11-27
*/
#ifndef IMU_READER_H
#define IMU_READER_H

#include "imu_parser.h"
#include "config_parser.h"
#include <serial/serial.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>

// IMU读取器（支持热拔插）
class IMUReader {
public:
    IMUReader();
    ~IMUReader();

    // 加载配置并初始化
    bool initialize(const std::string& config_file);

    // 启动读取线程
    bool start();

    // 停止读取线程
    void stop();

    // 是否正在运行
    bool isRunning() const { return running_; }

    // 是否已连接
    bool isConnected() const { return connected_; }

    // 设置数据回调函数
    void setDataCallback(IMUDataCallback callback);

    // 发送命令
    bool sendCommand(const U8* cmd, size_t len);

    // 配置IMU参数
    bool configureIMU();

    // 唤醒传感器
    bool wakeupSensor();

    // 启用主动上报
    bool enableAutoReport();

private:
    // 读取线程函数
    void readThread();

    // 热拔插检测线程
    void hotplugThread();

    // 打开串口
    bool openSerial();

    // 关闭串口
    void closeSerial();

    // 重连串口
    bool reconnect();

    // 发送数据包
    int sendPacket(const U8* data, size_t len);

    ConfigParser config_;
    std::unique_ptr<serial::Serial> serial_;
    std::unique_ptr<IMUParser> parser_;

    std::thread read_thread_;
    std::thread hotplug_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::mutex serial_mutex_;

    // 配置参数
    std::string port_;
    int baudrate_;
    int timeout_;
    U8 device_address_;
    int report_rate_;
    uint16_t subscribe_tag_;
    bool compass_on_;
    int barometer_filter_;
    int gyro_filter_;
    int acc_filter_;
    int compass_filter_;

    // 热拔插参数
    int check_interval_;
    int reconnect_interval_;
    int max_reconnect_;
    int reconnect_count_;

    // 调试参数
    bool debug_enabled_;
};

#endif // IMU_READER_H

