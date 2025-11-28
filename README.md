# IMU数据读取器 (C++实现)

基于C++的10 DOF IMU数据读取程序，支持串口通信、热拔插恢复和配置文件管理。

## 特性

- ✅ 标准C++项目结构
- ✅ 完整依赖包含（serial库已内置，无需外部依赖）
- ✅ 支持热拔插（自动检测断开并重连）
- ✅ 配置文件管理（无需重新编译）
- ✅ 高性能多线程设计
- ✅ 完整的数据解析和回调机制
- ✅ 独立工程（所有依赖都在项目内，可直接提交）

## 项目结构

```
cpp_imu_reader/
├── CMakeLists.txt          # CMake构建文件
├── config.ini              # 配置文件
├── include/                # 头文件目录
│   ├── config_parser.h     # 配置文件解析器
│   ├── imu_parser.h        # IMU数据包解析器
│   └── imu_reader.h        # IMU读取器（主类）
├── src/                    # 源文件目录
│   ├── config_parser.cpp
│   ├── imu_parser.cpp
│   └── imu_reader.cpp
├── example/                # 示例程序
│   └── main.cpp
├── third_party/            # 第三方库（已包含所有依赖）
│   └── serial/            # 串口通信库
└── README.md
```

## 编译

### 依赖要求

- CMake 3.10+
- C++17编译器 (GCC 7+, Clang 5+)
- Linux系统（支持Windows/macOS）

**注意**：所有第三方依赖（serial库）已包含在 `third_party/` 目录中，无需额外安装。

### 编译步骤

```bash
# 创建构建目录
mkdir build
cd build

# 配置CMake
cmake ..

# 编译
make

# 运行示例
./imu_reader_example
```

## 配置文件说明

编辑 `config.ini` 文件来配置程序参数：

### [Serial] 串口配置
- `port`: 串口设备路径
  - Linux: `/dev/ttyUSB0`, `/dev/ttyS0` 等
  - Windows: `COM3`, `COM4` 等
- `baudrate`: 波特率（默认115200）
- `timeout`: 超时时间（毫秒）

### [IMU] IMU配置
- `device_address`: 设备地址（0-254, 255=广播）
- `report_rate`: 上报帧率（0-250Hz, 0=0.5Hz）
- `subscribe_tag`: 订阅标签（位掩码）
  - `0x01`: 加速度（不含重力）
  - `0x02`: 加速度（含重力）
  - `0x04`: 角速度
  - `0x08`: 磁力计
  - `0x10`: 温度、气压、高度
  - `0x20`: 四元数
  - `0x40`: 欧拉角
  - `0x7F`: 全部数据
- `compass_on`: 是否使用磁力计融合（0/1）
- `barometer_filter`: 气压计滤波等级（0-3）
- `gyro_filter`: 陀螺仪滤波系数（0-2）
- `acc_filter`: 加速度计滤波系数（0-4）
- `compass_filter`: 磁力计滤波系数（0-9）

### [HotPlug] 热拔插配置
- `check_interval`: 检测间隔（毫秒）
- `reconnect_interval`: 重连尝试间隔（毫秒）
- `max_reconnect`: 最大重连次数（0=无限）

## 使用方法

### 基本使用

```bash
# 使用默认配置文件
./imu_reader_example

# 指定配置文件
./imu_reader_example /path/to/config.ini
```

### 代码示例

```cpp
#include "imu_reader.h"

// 创建读取器
IMUReader reader;

// 初始化
reader.initialize("config.ini");

// 设置数据回调
reader.setDataCallback([](const IMUData& data) {
    std::cout << "欧拉角: " << data.euler_x << ", " 
              << data.euler_y << ", " << data.euler_z << std::endl;
});

// 启动
reader.start();

// ... 运行 ...

// 停止
reader.stop();
```

## 热拔插支持

程序自动检测串口连接状态：
- 定期检查串口连接
- 检测到断开时自动尝试重连
- 重连成功后自动恢复数据读取
- 解析器状态自动重置，确保数据完整性

## 性能优化

- 多线程设计：读取线程和热拔插检测线程分离
- 无锁设计：使用原子变量和互斥锁保证线程安全
- 高效解析：状态机逐字节解析，最小化内存拷贝
- 回调机制：异步数据处理，不阻塞读取线程

## 故障排除

### 串口权限问题（Linux）

```bash
# 添加用户到dialout组
sudo usermod -a -G dialout $USER
# 重新登录后生效

# 或临时设置权限
sudo chmod 666 /dev/ttyUSB0
```

### 找不到串口设备

```bash
# 列出可用串口
ls -l /dev/ttyUSB* /dev/ttyS*

# 或使用dmesg查看
dmesg | grep tty
```

### 数据接收异常

1. 检查波特率是否匹配
2. 检查订阅标签是否正确
3. 检查串口线是否正常
4. 查看程序输出的错误信息

## 作者
JetsonLV
基于10 DOF ROS IMU项目Python代码移植。

