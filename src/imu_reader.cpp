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
#include <sys/stat.h>
#include <fstream>

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

    // 唤醒传感器
    if (!wakeupSensor()) {
        std::cerr << "唤醒传感器失败" << std::endl;
        return false;
    }

    // 启用主动上报
    if (!enableAutoReport()) {
        std::cerr << "启用主动上报失败" << std::endl;
        return false;
    }

    std::cout << "IMU配置完成，等待数据..." << std::endl;

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

    std::cout << "发送IMU配置命令..." << std::endl;
    if (!sendCommand(params, 11)) {
        std::cerr << "发送配置命令失败" << std::endl;
        return false;
    }

    usleep(200000);  // 等待200ms
    std::cout << "IMU配置命令已发送" << std::endl;
    return true;
}

bool IMUReader::wakeupSensor() {
    U8 cmd[1] = {0x03};
    std::cout << "唤醒传感器..." << std::endl;
    if (!sendCommand(cmd, 1)) {
        std::cerr << "唤醒传感器命令发送失败" << std::endl;
        return false;
    }
    usleep(200000);  // 等待200ms
    std::cout << "传感器已唤醒" << std::endl;
    return true;
}

bool IMUReader::enableAutoReport() {
    U8 cmd[1] = {0x19};
    std::cout << "启用主动上报..." << std::endl;
    if (!sendCommand(cmd, 1)) {
        std::cerr << "启用主动上报命令发送失败" << std::endl;
        return false;
    }
    std::cout << "主动上报已启用" << std::endl;
    return true;
}

bool IMUReader::openSerial() {
    std::lock_guard<std::mutex> lock(serial_mutex_);

    // 先检查设备文件是否存在
    struct stat file_stat;
    if (stat(port_.c_str(), &file_stat) != 0) {
        // 设备文件不存在
        std::cerr << "设备文件不存在: " << port_ << std::endl;
        connected_ = false;
        return false;
    }

    try {
        // 如果串口已经打开，先关闭并释放
        if (serial_) {
            try {
                if (serial_->isOpen()) {
                    serial_->close();
                }
            } catch (...) {
                // 忽略关闭时的异常
            }
            serial_.reset();
        }

        // 等待一小段时间，确保设备完全就绪
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        serial_ = std::make_unique<serial::Serial>(
            port_,
            baudrate_,
            serial::Timeout::simpleTimeout(timeout_)
        );

        if (serial_->isOpen()) {
            connected_ = true;
            std::cout << "串口打开成功: " << port_ << std::endl;
            return true;
        } else {
            std::cerr << "串口打开失败: " << port_ << " (isOpen()返回false)" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "打开串口异常: " << e.what() << std::endl;
    }

    connected_ = false;
    if (serial_) {
        serial_.reset();
    }
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

    // 等待设备就绪（检查设备文件是否存在）
    int wait_count = 0;
    const int max_wait = 50;  // 最多等待5秒（50 * 100ms）
    while (wait_count < max_wait && running_) {
        struct stat file_stat;
        if (stat(port_.c_str(), &file_stat) == 0) {
            // 设备文件存在，等待一小段时间确保设备完全就绪
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }

    if (wait_count >= max_wait) {
        // 设备文件不存在，可能设备未插入
        return false;
    }

    if (openSerial()) {
        reconnect_count_ = 0;
        parser_->reset();  // 重置解析器状态

        // 等待串口稳定
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 重新配置
        if (configureIMU() && wakeupSensor() && enableAutoReport()) {
            std::cout << "重连成功并重新配置" << std::endl;
            return true;
        } else {
            // 配置失败，关闭串口
            std::cerr << "重连后配置失败" << std::endl;
            closeSerial();
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
    size_t total_bytes = 0;
    auto last_print_time = std::chrono::steady_clock::now();

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
                // 读取异常，关闭串口并标记为断开，让热插拔线程处理重连
                std::cerr << "读取串口异常: " << e.what() << std::endl;
                try {
                    if (serial_ && serial_->isOpen()) {
                        serial_->close();
                    }
                } catch (...) {
                    // 忽略关闭时的异常
                }
                connected_ = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        if (bytes_read > 0) {
            total_bytes += bytes_read;
            parser_->processByte(byte);
            
            // 每5秒打印一次接收统计（仅用于调试）
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_print_time).count() >= 5) {
                std::cout << "\n[调试] 已接收 " << total_bytes << " 字节 (速率: " 
                          << (total_bytes * 8 / 5) << " 字节/秒)" << std::endl;
                last_print_time = now;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void IMUReader::hotplugThread() {
    bool last_device_state = false;
    
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_));

        bool need_reconnect = false;
        bool device_exists = false;

        // 检查设备文件是否存在
        struct stat file_stat;
        if (stat(port_.c_str(), &file_stat) == 0) {
            device_exists = true;
        }

        {
            std::lock_guard<std::mutex> lock(serial_mutex_);
            
            // 检查连接状态
            bool is_connected = connected_ && serial_ && serial_->isOpen();
            
            if (!is_connected) {
                // 未连接状态
                if (device_exists) {
                    // 设备存在但未连接，需要重连
                    need_reconnect = true;
                }
            } else {
                // 已连接状态
                if (!device_exists) {
                    // 设备文件不存在，说明设备已拔出
                    connected_ = false;
                    if (last_device_state) {
                        std::cout << "检测到设备拔出: " << port_ << std::endl;
                    }
                    // 关闭串口
                    try {
                        if (serial_ && serial_->isOpen()) {
                            serial_->close();
                        }
                    } catch (...) {
                        // 忽略关闭时的异常
                    }
                } else {
                    // 设备存在，检查串口是否仍然可用
                    try {
                        // 尝试读取可用字节数来检测连接
                        size_t available = serial_->available();
                        // 如果读取失败，说明连接断开
                    } catch (...) {
                        need_reconnect = true;
                        connected_ = false;
                        std::cout << "检测到串口异常，尝试重连..." << std::endl;
                        // 关闭串口
                        try {
                            if (serial_ && serial_->isOpen()) {
                                serial_->close();
                            }
                        } catch (...) {
                            // 忽略关闭时的异常
                        }
                    }
                }
            }
        }

        // 检测设备重新插入（从不存在变为存在）
        if (!last_device_state && device_exists && !connected_) {
            std::cout << "检测到设备重新插入: " << port_ << std::endl;
            need_reconnect = true;
        }

        last_device_state = device_exists;

        if (need_reconnect && running_) {
            if (!device_exists) {
                // 设备不存在，等待设备插入
                continue;
            }
            
            std::cout << "尝试重连..." << std::endl;
            
            int retry_count = 0;
            while (running_ && !reconnect()) {
                retry_count++;
                // 在重连过程中，检查设备是否仍然存在
                struct stat file_stat;
                if (stat(port_.c_str(), &file_stat) != 0) {
                    // 设备又拔出了，停止重连尝试
                    std::cout << "重连过程中设备拔出，停止重连" << std::endl;
                    break;
                }
                
                // 每5次重连尝试输出一次提示
                if (retry_count % 5 == 0) {
                    std::cout << "重连中... (已尝试 " << retry_count << " 次)" << std::endl;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_interval_));
            }
        }
    }
}

