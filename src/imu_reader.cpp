/**
 * @file imu_reader.cpp
 * @brief IMU 串口读取器与管理实现
 *
 * Author : Jetson LV <ljhao1994@163.com>
 * Created: 2025-11-27
 * Description:
 *   实现对 IMU 设备的串口管理、配置下发、自动重连与数据读取线程。
 *   主要职责：
 *     - 打开/关闭串口并保证线程安全的读/写操作
 *     - 构建并发送 IMU 配置命令（参数配置、唤醒、自动上报等）
 *     - 持续读取串口数据并交给解析器处理
 *     - 热插拔检测与自动重连策略（可配置重连间隔与次数）
 *     - 提供回调接口将解析后的传感器数据发布给上层
 *
 * 设计要点：
 *   - 互斥保护：所有对 serial_ 对象的访问由 serial_mutex_ 保护
 *   - 线程模型：独立的 read_thread_ 负责字节读取，hotplug_thread_ 负责检测与重连
 *   - 错误恢复：遇到串口异常时尝试重连并在重连成功后重新配置 IMU
 *   - 非阻塞安全：发送命令与数据写入使用封装的发送函数保证原子性
 *
 * 依赖：
 *   - serial::Serial (https://github.com/wjwwood/serial)
 *   - IMUParser 类用于帧解析与数据回调
 *
 *
 * ChangeLog:
 *   2025-11-27  初始实现（Jetson LV）
 *
 */

#include "imu_reader.h"
#include <iostream>
#include <unistd.h>

IMUReader::IMUReader()
    : running_(false)
    , connected_(false)
    , baudrate_(115200)
    , timeout_(1000)
    , device_address_(255)
    , report_rate_(60)
    , subscribe_tag_(0x7F)
    , compass_on_(false)
    , barometer_filter_(2)
    , gyro_filter_(1)
    , acc_filter_(3)
    , compass_filter_(5)
    , check_interval_(1000)
    , reconnect_interval_(2000)
    , max_reconnect_(0)
    , reconnect_count_(0) {
    parser_ = std::make_unique<IMUParser>();
}

IMUReader::~IMUReader() {
    stop();
}

bool IMUReader::initialize(const std::string& config_file) {
    // 加载配置文件
    if (!config_.load(config_file)) {
        std::cerr << "加载配置文件失败: " << config_file << std::endl;
        return false;
    }

    // 读取串口配置
    port_ = config_.getString("Serial", "port", "/dev/ttyUSB0");
    baudrate_ = config_.getInt("Serial", "baudrate", 115200);
    timeout_ = config_.getInt("Serial", "timeout", 1000);

    // 读取IMU配置
    device_address_ = config_.getInt("IMU", "device_address", 255);
    report_rate_ = config_.getInt("IMU", "report_rate", 60);
    subscribe_tag_ = config_.getInt("IMU", "subscribe_tag", 0x7F);
    compass_on_ = config_.getBool("IMU", "compass_on", false);
    barometer_filter_ = config_.getInt("IMU", "barometer_filter", 2);
    gyro_filter_ = config_.getInt("IMU", "gyro_filter", 1);
    acc_filter_ = config_.getInt("IMU", "acc_filter", 3);
    compass_filter_ = config_.getInt("IMU", "compass_filter", 5);

    // 读取热拔插配置
    check_interval_ = config_.getInt("HotPlug", "check_interval", 1000);
    reconnect_interval_ = config_.getInt("HotPlug", "reconnect_interval", 2000);
    max_reconnect_ = config_.getInt("HotPlug", "max_reconnect", 0);

    std::cout << "配置加载成功:" << std::endl;
    std::cout << "  串口: " << port_ << " @ " << baudrate_ << " baud" << std::endl;
    std::cout << "  设备地址: " << (int)device_address_ << std::endl;
    std::cout << "  上报频率: " << report_rate_ << " Hz" << std::endl;

    return true;
}

bool IMUReader::start() {
    if (running_) {
        return true;
    }

    // 打开串口
    if (!openSerial()) {
        std::cerr << "打开串口失败" << std::endl;
        return false;
    }

    // 配置IMU
    if (!configureIMU()) {
        std::cerr << "配置IMU失败" << std::endl;
        return false;
    }

    running_ = true;
    reconnect_count_ = 0;

    // 启动读取线程
    read_thread_ = std::thread(&IMUReader::readThread, this);

    // 启动热拔插检测线程
    hotplug_thread_ = std::thread(&IMUReader::hotplugThread, this);

    return true;
}

void IMUReader::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // 等待线程结束
    if (read_thread_.joinable()) {
        read_thread_.join();
    }
    if (hotplug_thread_.joinable()) {
        hotplug_thread_.join();
    }

    closeSerial();
}

void IMUReader::setDataCallback(IMUDataCallback callback) {
    parser_->setDataCallback(callback);
}

bool IMUReader::sendCommand(const U8* cmd, size_t len) {
    std::lock_guard<std::mutex> lock(serial_mutex_);
    
    if (!connected_ || !serial_) {
        return false;
    }

    return IMUParser::packAndSend(const_cast<U8*>(cmd), len, device_address_,
        [this](const U8* data, size_t len) -> int {
            try {
                size_t written = serial_->write(data, len);
                return (written == len) ? 0 : -1;
            } catch (...) {
                return -1;
            }
        }) == 0;
}

bool IMUReader::configureIMU() {
    // 构建参数配置命令 (0x12)
    U8 params[11];
    params[0] = 0x12;
    params[1] = 5;  // 静止状态加速度阈值
    params[2] = 255;  // 静态归零速度
    params[3] = 0;  // 动态归零速度
    params[4] = ((barometer_filter_ & 3) << 1) | (compass_on_ ? 1 : 0);
    params[5] = report_rate_;
    params[6] = gyro_filter_;
    params[7] = acc_filter_;
    params[8] = compass_filter_;
    params[9] = subscribe_tag_ & 0xFF;
    params[10] = (subscribe_tag_ >> 8) & 0xFF;

    if (!sendCommand(params, 11)) {
        return false;
    }

    usleep(200000);  // 等待200ms
    return true;
}

bool IMUReader::wakeupSensor() {
    U8 cmd[1] = {0x03};
    if (!sendCommand(cmd, 1)) {
        return false;
    }
    usleep(200000);  // 等待200ms
    return true;
}

bool IMUReader::enableAutoReport() {
    U8 cmd[1] = {0x19};
    return sendCommand(cmd, 1);
}

bool IMUReader::openSerial() {
    std::lock_guard<std::mutex> lock(serial_mutex_);

    try {
        serial_ = std::make_unique<serial::Serial>(
            port_,
            baudrate_,
            serial::Timeout::simpleTimeout(timeout_)
        );

        if (serial_->isOpen()) {
            connected_ = true;
            std::cout << "串口打开成功: " << port_ << std::endl;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "打开串口异常: " << e.what() << std::endl;
    }

    connected_ = false;
    return false;
}

void IMUReader::closeSerial() {
    std::lock_guard<std::mutex> lock(serial_mutex_);
    
    if (serial_ && serial_->isOpen()) {
        serial_->close();
    }
    serial_.reset();
    connected_ = false;
}

bool IMUReader::reconnect() {
    if (max_reconnect_ > 0 && reconnect_count_ >= max_reconnect_) {
        std::cerr << "达到最大重连次数" << std::endl;
        return false;
    }

    closeSerial();
    reconnect_count_++;

    std::cout << "尝试重连 (" << reconnect_count_ << ")..." << std::endl;

    if (openSerial()) {
        reconnect_count_ = 0;
        parser_->reset();  // 重置解析器状态

        // 重新配置
        if (configureIMU() && wakeupSensor() && enableAutoReport()) {
            std::cout << "重连成功并重新配置" << std::endl;
            return true;
        }
    }

    return false;
}

int IMUReader::sendPacket(const U8* data, size_t len) {
    std::lock_guard<std::mutex> lock(serial_mutex_);
    
    if (!connected_ || !serial_) {
        return -1;
    }

    try {
        size_t written = serial_->write(data, len);
        return (written == len) ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

void IMUReader::readThread() {
    U8 byte;
    size_t bytes_read = 0;

    while (running_) {
        {
            std::lock_guard<std::mutex> lock(serial_mutex_);
            
            if (!connected_ || !serial_ || !serial_->isOpen()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            try {
                bytes_read = serial_->read(&byte, 1);
            } catch (const std::exception& e) {
                std::cerr << "读取串口异常: " << e.what() << std::endl;
                connected_ = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        if (bytes_read > 0) {
            parser_->processByte(byte);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void IMUReader::hotplugThread() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_));

        bool need_reconnect = false;

        {
            std::lock_guard<std::mutex> lock(serial_mutex_);
            
            if (!serial_ || !serial_->isOpen()) {
                need_reconnect = true;
            } else {
                // 检查串口是否仍然可用
                try {
                    // 尝试读取可用字节数来检测连接
                    size_t available = serial_->available();
                    // 如果读取失败，说明连接断开
                } catch (...) {
                    need_reconnect = true;
                }
            }
        }

        if (need_reconnect && running_) {
            std::cout << "检测到串口断开，尝试重连..." << std::endl;
            
            while (running_ && !reconnect()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_interval_));
            }
        }
    }
}

