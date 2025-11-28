cmake_minimum_required(VERSION 3.10)
project(imu_reader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用项目内的serial库
set(SERIAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/serial)
include_directories(${SERIAL_DIR}/include)

# 源文件
set(SOURCES
    src/config_parser.cpp
    src/imu_parser.cpp
    src/imu_reader.cpp
)

# 头文件
set(HEADERS
    include/config_parser.h
    include/imu_parser.h
    include/imu_reader.h
)

# 创建库
add_library(imu_reader_lib STATIC ${SOURCES} ${HEADERS})
target_include_directories(imu_reader_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SERIAL_DIR}/include
)

# 链接serial库（使用项目内的serial库）
if(APPLE)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(FOUNDATION_LIBRARY Foundation)
    add_library(serial_lib STATIC
        ${SERIAL_DIR}/src/serial.cc
        ${SERIAL_DIR}/src/impl/unix.cc
        ${SERIAL_DIR}/src/impl/list_ports/list_ports_osx.cc
    )
    target_link_libraries(serial_lib ${FOUNDATION_LIBRARY} ${IOKIT_LIBRARY})
elseif(UNIX)
    add_library(serial_lib STATIC
        ${SERIAL_DIR}/src/serial.cc
        ${SERIAL_DIR}/src/impl/unix.cc
        ${SERIAL_DIR}/src/impl/list_ports/list_ports_linux.cc
    )
    target_link_libraries(serial_lib rt pthread)
elseif(WIN32)
    add_library(serial_lib STATIC
        ${SERIAL_DIR}/src/serial.cc
        ${SERIAL_DIR}/src/impl/win.cc
        ${SERIAL_DIR}/src/impl/list_ports/list_ports_win.cc
    )
    target_link_libraries(serial_lib setupapi)
endif()

target_include_directories(serial_lib PUBLIC ${SERIAL_DIR}/include)
target_link_libraries(imu_reader_lib serial_lib)

# 示例程序
add_executable(imu_reader_example example/main.cpp)
target_link_libraries(imu_reader_example imu_reader_lib pthread)

# 安装
install(TARGETS imu_reader_example DESTINATION bin)
install(FILES config.ini DESTINATION etc)

