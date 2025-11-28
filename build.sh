#!/bin/bash

# IMU读取器构建脚本

echo "=== IMU读取器构建脚本 ==="

# 创建构建目录
if [ ! -d "build" ]; then
    mkdir build
    echo "创建构建目录: build"
fi

cd build

# 运行CMake
echo "配置CMake..."
cmake ..

if [ $? -ne 0 ]; then
    echo "CMake配置失败！"
    exit 1
fi

# 编译
echo "开始编译..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "编译失败！"
    exit 1
fi

echo ""
echo "=== 编译完成 ==="
echo "可执行文件位置: build/imu_reader_example"
echo ""
echo "运行示例:"
echo "  cd build"
echo "  ./imu_reader_example"
echo ""

