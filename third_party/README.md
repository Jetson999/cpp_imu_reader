# 第三方库说明

本目录包含项目依赖的第三方库。

## serial库

`serial/` 目录包含串口通信库，来自项目自带的serial库。

### 许可证

MIT License

Copyright (c) 2012 William Woodall

### 说明

该库提供了跨平台的串口通信接口，支持：
- Linux (使用termios)
- Windows (使用Win32 API)
- macOS (使用IOKit)

### 文件结构

```
serial/
├── include/serial/     # 头文件
│   ├── serial.h        # 主头文件
│   ├── v8stdint.h      # 类型定义
│   └── impl/           # 平台特定实现头文件
│       ├── unix.h      # Unix/Linux实现
│       └── win.h       # Windows实现
└── src/                # 源文件
    ├── serial.cc       # 主实现
    └── impl/           # 平台特定实现
        ├── unix.cc     # Unix/Linux实现
        ├── win.cc      # Windows实现
        └── list_ports/ # 串口列表功能
            ├── list_ports_linux.cc
            ├── list_ports_osx.cc
            └── list_ports_win.cc
```

### 使用

该库已集成到CMakeLists.txt中，无需额外配置。

