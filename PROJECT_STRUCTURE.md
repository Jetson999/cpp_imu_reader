# 项目结构说明

本项目是一个独立的C++工程，所有依赖都已包含在项目内。

## 目录结构

```
cpp_imu_reader/
├── CMakeLists.txt              # CMake构建配置
├── config.ini                  # 配置文件（串口、IMU参数等）
├── build.sh / build.bat        # 构建脚本
├── .gitignore                  # Git忽略文件
│
├── include/                    # 头文件目录
│   ├── config_parser.h         # 配置文件解析器
│   ├── imu_parser.h           # IMU数据包解析器
│   └── imu_reader.h           # IMU读取器（主类，支持热拔插）
│
├── src/                        # 源文件目录
│   ├── config_parser.cpp       # 配置文件解析实现
│   ├── imu_parser.cpp         # IMU数据包解析实现
│   └── imu_reader.cpp         # IMU读取器实现
│
├── example/                    # 示例程序
│   └── main.cpp               # 主程序示例
│
├── third_party/                # 第三方库（已包含所有依赖）
│   ├── README.md              # 第三方库说明
│   └── serial/                # 串口通信库
│       ├── include/serial/    # 头文件
│       └── src/               # 源文件
│
├── README.md                   # 项目说明文档
├── QUICKSTART.md              # 快速开始指南
└── PROJECT_STRUCTURE.md       # 本文件
```

## 依赖说明

### 内置依赖

- **serial库**：位于 `third_party/serial/`
  - 跨平台串口通信库
  - 支持 Linux/Windows/macOS
  - MIT许可证
  - 无需额外安装，已包含在项目中

### 系统依赖

- CMake 3.10+
- C++17编译器
- pthread库（Linux/macOS）
- setupapi库（Windows）

## 独立工程特性

✅ **完全独立**：所有代码和依赖都在项目内  
✅ **无需外部路径**：不依赖项目外的任何目录  
✅ **可直接提交**：整个目录可以作为独立仓库提交  
✅ **跨平台支持**：支持Linux/Windows/macOS编译

## 编译说明

项目使用CMake构建系统，serial库已集成到CMakeLists.txt中。

```bash
# Linux/macOS
mkdir build && cd build
cmake ..
make

# Windows
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## 注意事项

1. `third_party/` 目录包含项目依赖，**需要提交到仓库**
2. `build/` 目录是编译产物，已在 `.gitignore` 中忽略
3. 配置文件 `config.ini` 可根据实际环境修改，无需重新编译

