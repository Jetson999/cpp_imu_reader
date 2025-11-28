/*
    * @file imu_parser.h
    * @brief IMU 数据解析器头文件
    *
    * Author : Jetson LV <ljhao1994@163.com>
    * Created: 2025-11-27
*/
#ifndef IMU_PARSER_H
#define IMU_PARSER_H

#include <cstdint>
#include <functional>
#include <cmath>

// 数据类型定义
typedef int8_t   S8;
typedef uint8_t  U8;
typedef int16_t  S16;
typedef uint16_t U16;
typedef int32_t  S32;
typedef uint32_t U32;
typedef float    F32;

// 数据缩放因子
constexpr F32 SCALE_ACCEL       = 0.00478515625f;      // 加速度 m/s²
constexpr F32 SCALE_QUAT        = 0.000030517578125f;  // 四元数
constexpr F32 SCALE_ANGLE       = 0.0054931640625f;   // 欧拉角 度
constexpr F32 SCALE_ANGLE_SPEED = 0.06103515625f;     // 角速度 dps
constexpr F32 SCALE_MAG         = 0.15106201171875f;   // 磁力计 uT
constexpr F32 SCALE_TEMPERATURE = 0.01f;              // 温度 ℃
constexpr F32 SCALE_AIR_PRESSURE = 0.0002384185791f;   // 气压 hPa
constexpr F32 SCALE_HEIGHT      = 0.0010728836f;      // 高度 m

// 数据包常量
constexpr U8 CMD_PACKET_BEGIN = 0x49;
constexpr U8 CMD_PACKET_END   = 0x4D;
constexpr U8 CMD_PACKET_MAX_DAT_SIZE_RX = 73;
constexpr U8 CMD_PACKET_MAX_DAT_SIZE_TX = 31;

// IMU数据结构
struct IMUData {
    // 加速度 (不含重力) m/s²
    float accel_x = 0.0f, accel_y = 0.0f, accel_z = 0.0f;
    
    // 加速度 (含重力) m/s²
    float accel_with_gravity_x = 0.0f, accel_with_gravity_y = 0.0f, accel_with_gravity_z = 0.0f;
    
    // 角速度 dps
    float gyro_x = 0.0f, gyro_y = 0.0f, gyro_z = 0.0f;
    
    // 磁力计 uT
    float mag_x = 0.0f, mag_y = 0.0f, mag_z = 0.0f;
    
    // 温度、气压、高度
    float temperature = 0.0f;  // ℃
    float pressure = 0.0f;     // hPa
    float height = 0.0f;       // m
    
    // 四元数
    float quat_w = 0.0f, quat_x = 0.0f, quat_y = 0.0f, quat_z = 0.0f;
    
    // 欧拉角 度
    float euler_x = 0.0f, euler_y = 0.0f, euler_z = 0.0f;
    
    // 时间戳 ms
    uint32_t timestamp = 0;
    
    // 订阅标签
    uint16_t subscribe_tag = 0;
};

// 数据回调函数类型
using IMUDataCallback = std::function<void(const IMUData&)>;

// IMU数据包解析器
class IMUParser {
public:
    IMUParser();
    ~IMUParser() = default;

    // 设置数据回调函数
    void setDataCallback(IMUDataCallback callback);

    // 处理接收到的字节
    bool processByte(U8 byte);

    // 打包并发送命令
    static int packAndSend(U8* pDat, U8 dLen, U8 deviceAddr, std::function<int(const U8*, size_t)> sendFunc);

    // 重置解析状态（用于热拔插恢复）
    void reset();

private:
    // 解析数据包
    void unpackData(U8* buf, U8 dLen);

    // 解析传感器数据 (0x11命令)
    void parseSensorData(U8* buf, U8 dLen);

    // 状态机状态
    enum RxState {
        RX_STATE_WAIT_BEGIN = 0,  // 等待起始码
        RX_STATE_ADDRESS,         // 接收地址码
        RX_STATE_LENGTH,          // 接收长度
        RX_STATE_DATA,            // 接收数据体
        RX_STATE_CHECKSUM,        // 接收校验和
        RX_STATE_END              // 接收结束码
    };

    RxState rx_state_;
    U8 rx_buffer_[5 + CMD_PACKET_MAX_DAT_SIZE_RX];
    U8 rx_index_;
    U8 rx_cmd_len_;
    U8 rx_checksum_;
    U8 target_device_addr_;

    IMUDataCallback data_callback_;
};

#endif // IMU_PARSER_H

