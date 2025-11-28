/**
 * @file imu_parser.cpp
 * @brief IMU 数据解析器实现
 *
 * Author : Jetson LV <ljhao1994@163.com>
 * Created: 2025-11-27
 * description: imu Data Parser
 */
#include "imu_parser.h"
#include <cstring>
#include <iostream>

IMUParser::IMUParser() 
    : rx_state_(RX_STATE_WAIT_BEGIN)
    , rx_index_(0)
    , rx_cmd_len_(0)
    , rx_checksum_(0)
    , target_device_addr_(255) {
}

void IMUParser::setDataCallback(IMUDataCallback callback) {
    data_callback_ = callback;
}

bool IMUParser::processByte(U8 byte) {
    rx_checksum_ += byte;

    switch (rx_state_) {
        case RX_STATE_WAIT_BEGIN:
            if (byte == CMD_PACKET_BEGIN) {
                rx_index_ = 0;
                rx_buffer_[rx_index_++] = CMD_PACKET_BEGIN;
                rx_checksum_ = 0;  // 从下一个字节开始计算校验和
                rx_state_ = RX_STATE_ADDRESS;
            }
            break;

        case RX_STATE_ADDRESS:
            rx_buffer_[rx_index_++] = byte;
            if (byte == 255) {
                // 广播地址，重置
                rx_state_ = RX_STATE_WAIT_BEGIN;
            } else {
                rx_state_ = RX_STATE_LENGTH;
            }
            break;

        case RX_STATE_LENGTH:
            rx_buffer_[rx_index_++] = byte;
            if (byte > CMD_PACKET_MAX_DAT_SIZE_RX || byte == 0) {
                // 无效长度，重置
                rx_state_ = RX_STATE_WAIT_BEGIN;
            } else {
                rx_cmd_len_ = byte;
                rx_state_ = RX_STATE_DATA;
            }
            break;

        case RX_STATE_DATA:
            rx_buffer_[rx_index_++] = byte;
            if (rx_index_ >= rx_cmd_len_ + 3) {
                // 数据体接收完成
                rx_state_ = RX_STATE_CHECKSUM;
            }
            break;

        case RX_STATE_CHECKSUM:
            rx_checksum_ -= byte;  // 减去校验和字节本身
            if ((rx_checksum_ & 0xFF) == byte) {
                // 校验通过
                rx_buffer_[rx_index_++] = byte;
                rx_state_ = RX_STATE_END;
            } else {
                // 校验失败，重置
                std::cerr << "[调试] 校验失败: 期望=" << (int)byte << " 计算=" << (int)(rx_checksum_ & 0xFF) << std::endl;
                rx_state_ = RX_STATE_WAIT_BEGIN;
            }
            break;

        case RX_STATE_END:
            rx_state_ = RX_STATE_WAIT_BEGIN;
            if (byte == CMD_PACKET_END) {
                rx_buffer_[rx_index_++] = byte;
                // 检查地址匹配
                U8 addr = rx_buffer_[1];
                U8 data_len = rx_index_ - 5;
                if (target_device_addr_ == 255 || target_device_addr_ == addr) {
                    std::cout << "[调试] 收到完整数据包: 地址=" << (int)addr 
                              << " 长度=" << (int)data_len 
                              << " 命令=0x" << std::hex << (int)rx_buffer_[3] << std::dec << std::endl;
                    unpackData(&rx_buffer_[3], data_len);
                    return true;
                } else {
                    std::cerr << "[调试] 地址不匹配: 期望=" << (int)target_device_addr_ 
                              << " 收到=" << (int)addr << std::endl;
                }
            } else {
                std::cerr << "[调试] 结束字节错误: 期望=0x4D 收到=0x" 
                          << std::hex << (int)byte << std::dec << std::endl;
            }
            break;
    }

    return false;
}

void IMUParser::unpackData(U8* buf, U8 dLen) {
    if (dLen == 0) return;

    switch (buf[0]) {
        case 0x11:  // 传感器数据
            parseSensorData(buf, dLen);
            break;
        default:
            // 其他命令响应，可在此扩展
            std::cout << "[调试] 收到未知命令: 0x" << std::hex << (int)buf[0] << std::dec << std::endl;
            break;
    }
}

void IMUParser::parseSensorData(U8* buf, U8 dLen) {
    if (dLen < 7) {
        std::cerr << "[调试] 数据长度不足: " << (int)dLen << std::endl;
        return;
    }

    IMUData data;
    
    // 解析订阅标签和时间戳
    data.subscribe_tag = ((U16)buf[2] << 8) | buf[1];
    data.timestamp = ((U32)buf[6] << 24) | ((U32)buf[5] << 16) | 
                    ((U32)buf[4] << 8) | buf[3];

    U8 L = 7;  // 数据起始位置

    // 解析加速度（不含重力）
    if ((data.subscribe_tag & 0x0001) != 0 && L + 6 <= dLen) {
        data.accel_x = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ACCEL;
        L += 2;
        data.accel_y = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ACCEL;
        L += 2;
        data.accel_z = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ACCEL;
        L += 2;
    }

    // 解析加速度（含重力）
    if ((data.subscribe_tag & 0x0002) != 0 && L + 6 <= dLen) {
        data.accel_with_gravity_x = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ACCEL;
        L += 2;
        data.accel_with_gravity_y = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ACCEL;
        L += 2;
        data.accel_with_gravity_z = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ACCEL;
        L += 2;
    }

    // 解析角速度
    if ((data.subscribe_tag & 0x0004) != 0 && L + 6 <= dLen) {
        data.gyro_x = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ANGLE_SPEED;
        L += 2;
        data.gyro_y = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ANGLE_SPEED;
        L += 2;
        data.gyro_z = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ANGLE_SPEED;
        L += 2;
    }

    // 解析磁力计
    if ((data.subscribe_tag & 0x0008) != 0 && L + 6 <= dLen) {
        data.mag_x = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_MAG;
        L += 2;
        data.mag_y = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_MAG;
        L += 2;
        data.mag_z = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_MAG;
        L += 2;
    }

    // 解析温度、气压、高度
    if ((data.subscribe_tag & 0x0010) != 0 && L + 8 <= dLen) {
        // 温度
        data.temperature = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_TEMPERATURE;
        L += 2;

        // 气压 (24位有符号整数)
        U32 tmpU32 = ((U32)buf[L+2] << 16) | ((U32)buf[L+1] << 8) | buf[L];
        if ((tmpU32 & 0x800000) == 0x800000) {
            tmpU32 |= 0xff000000;  // 符号扩展
        }
        data.pressure = (S32)tmpU32 * SCALE_AIR_PRESSURE;
        L += 3;

        // 高度 (24位有符号整数)
        tmpU32 = ((U32)buf[L+2] << 16) | ((U32)buf[L+1] << 8) | buf[L];
        if ((tmpU32 & 0x800000) == 0x800000) {
            tmpU32 |= 0xff000000;  // 符号扩展
        }
        data.height = (S32)tmpU32 * SCALE_HEIGHT;
        L += 3;
    }

    // 解析四元数
    if ((data.subscribe_tag & 0x0020) != 0 && L + 8 <= dLen) {
        data.quat_w = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_QUAT;
        L += 2;
        data.quat_x = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_QUAT;
        L += 2;
        data.quat_y = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_QUAT;
        L += 2;
        data.quat_z = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_QUAT;
        L += 2;
    }

    // 解析欧拉角
    if ((data.subscribe_tag & 0x0040) != 0 && L + 6 <= dLen) {
        data.euler_x = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ANGLE;
        L += 2;
        data.euler_y = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ANGLE;
        L += 2;
        data.euler_z = (S16)(((S16)buf[L+1] << 8) | buf[L]) * SCALE_ANGLE;
        L += 2;
    }

    // 调用回调函数
    if (data_callback_) {
        data_callback_(data);
    }
}

int IMUParser::packAndSend(U8* pDat, U8 dLen, U8 deviceAddr, 
                           std::function<int(const U8*, size_t)> sendFunc) {
    if (dLen == 0 || dLen > CMD_PACKET_MAX_DAT_SIZE_TX || pDat == nullptr) {
        return -1;
    }

    // 构建数据包: 前导码(50字节) + 数据包(5字节) + 数据体
    U8 buf[50 + 5 + CMD_PACKET_MAX_DAT_SIZE_TX];
    
    // 填充前导码
    memset(buf, 0x00, 46);
    buf[46] = 0x00;
    buf[47] = 0xFF;
    buf[48] = 0x00;
    buf[49] = 0xFF;

    // 填充数据包
    buf[50] = CMD_PACKET_BEGIN;
    buf[51] = deviceAddr;
    buf[52] = dLen;
    memcpy(&buf[53], pDat, dLen);

    // 计算校验和（从地址码到数据体结束）
    U8 checksum = 0;
    for (int i = 51; i < 53 + dLen; i++) {
        checksum += buf[i];
    }
    buf[53 + dLen] = checksum;
    buf[54 + dLen] = CMD_PACKET_END;

    // 发送数据
    return sendFunc(buf, 55 + dLen);
}

void IMUParser::reset() {
    rx_state_ = RX_STATE_WAIT_BEGIN;
    rx_index_ = 0;
    rx_cmd_len_ = 0;
    rx_checksum_ = 0;
    memset(rx_buffer_, 0, sizeof(rx_buffer_));
}

