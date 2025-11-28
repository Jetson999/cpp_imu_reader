# 快速开始指南

## 1. 编译项目

### Linux/macOS

```bash
# 使用构建脚本
./build.sh

# 或手动编译
mkdir build && cd build
cmake ..
make
```

### Windows

```cmd
# 使用构建脚本
build.bat

# 或手动编译
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## 2. 配置串口

编辑 `config.ini` 文件，修改串口设备路径：

```ini
[Serial]
port=/dev/ttyUSB0    # Linux: /dev/ttyUSB0, /dev/ttyS0
                     # Windows: COM3, COM4
baudrate=115200
```

### Linux串口权限

如果遇到权限问题：

```bash
# 方法1: 添加用户到dialout组
sudo usermod -a -G dialout $USER
# 重新登录后生效

# 方法2: 临时设置权限
sudo chmod 666 /dev/ttyUSB0

# 方法3: 使用udev规则（永久）
sudo nano /etc/udev/rules.d/99-serial.rules
# 添加: KERNEL=="ttyUSB*", MODE="0666"
sudo udevadm control --reload-rules
```

## 3. 运行程序

```bash
# Linux/macOS
cd build
./imu_reader_example

# Windows
cd build\Release
imu_reader_example.exe

# 指定配置文件
./imu_reader_example /path/to/config.ini
```

## 4. 查看数据

程序运行后会实时显示IMU数据：
- 欧拉角（度）
- 角速度（dps）
- 加速度（m/s²）

按 `Ctrl+C` 退出程序。

## 5. 热拔插测试

1. 运行程序
2. 拔掉USB串口线
3. 程序会自动检测并显示"检测到串口断开"
4. 重新插入USB串口线
5. 程序会自动重连并恢复数据读取

## 常见问题

### Q: 编译时找不到serial库？

A: 确保项目目录结构正确，serial库应该在 `../ros_example_code/serial`

### Q: 运行时提示"无法打开配置文件"？

A: 确保 `config.ini` 文件在可执行文件同目录，或使用绝对路径

### Q: 收不到数据？

A: 
1. 检查串口设备路径是否正确
2. 检查波特率是否匹配（默认115200）
3. 检查串口权限
4. 检查IMU设备是否正常工作

### Q: 数据值异常？

A:
1. 检查订阅标签是否正确
2. 检查IMU是否需要校准
3. 查看程序输出的错误信息

## 自定义配置

### 修改上报频率

```ini
[IMU]
report_rate=100  # 100Hz
```

### 只订阅特定数据

```ini
[IMU]
subscribe_tag=0x46  # 只订阅角速度(0x04)和欧拉角(0x40)
```

### 调整热拔插参数

```ini
[HotPlug]
check_interval=500        # 检测间隔500ms
reconnect_interval=1000   # 重连间隔1秒
max_reconnect=10         # 最多重连10次
```

