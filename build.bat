@echo off
REM IMU读取器构建脚本 (Windows)

echo === IMU读取器构建脚本 ===

REM 创建构建目录
if not exist build mkdir build
echo 创建构建目录: build

cd build

REM 运行CMake
echo 配置CMake...
cmake ..

if errorlevel 1 (
    echo CMake配置失败！
    exit /b 1
)

REM 编译
echo 开始编译...
cmake --build . --config Release

if errorlevel 1 (
    echo 编译失败！
    exit /b 1
)

echo.
echo === 编译完成 ===
echo 可执行文件位置: build\Release\imu_reader_example.exe
echo.
echo 运行示例:
echo   cd build\Release
echo   imu_reader_example.exe
echo.

