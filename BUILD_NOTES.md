# 🦞 QtModbusTool - GitHub Actions 编译问题总结

> 创建时间：2026-03-19  
> 更新时间：2026-03-19 (v1.1.0)  
> 目的：记录编译过程中遇到的问题和解决方案，供后续参考

---

## 📋 问题总览

| 序号 | 问题类型 | 状态 |
|------|----------|------|
| 1 | Qt 版本与架构不匹配 | ✅ 已解决 |
| 2 | 编译器运行时缺失 | ✅ 已解决 |
| 3 | 依赖模块缺失 (SerialPort) | ✅ 已解决 |
| 4 | windeployqt 部署失败 | ✅ 已解决 |
| 5 | 代码编译错误 (枚举类型) | ✅ 已解决 |
| 6 | 权限问题 | ✅ 已解决 |

---

## 🔧 问题详情与解决方案

### 问题 1：Qt 版本与架构不匹配

**❌ 错误表现：**
```
Error: Qt version not found for arch win64_msvc2022_64
CMake failed: Qt6 package not found
```

**🔍 原因分析：**
- Qt 6.5.0 没有 `win64_msvc2022_64` 架构
- GitHub Actions 的 `windows-latest` 默认使用 VS 2022
- Qt 官方只提供 `win64_msvc2019_64` 架构

**✅ 解决方案：**
```yaml
uses: jurplel/install-qt-action@v4
with:
  version: '6.5.0'
  arch: 'win64_msvc2019_64'  # 使用 VS2019 架构
  modules: 'qtserialbus qtserialport'
```

**📝 教训：**
> Qt 的 Windows 架构命名与 Visual Studio 版本绑定，不是与 GitHub Actions 的 runner 版本绑定。

---

### 问题 2：编译器运行时缺失 (MSVC Runtime)

**❌ 错误表现：**
```
The code execution cannot proceed because MSVCP140.dll was not found
VCRUNTIME140_1.dll missing
```

**🔍 原因分析：**
- `windeployqt` 默认不复制 MSVC 运行时文件
- 目标机器没有安装 Visual C++ Redistributable

**✅ 解决方案（两种）：**

**方案 A：使用 windeployqt 参数**
```yaml
windeployqt QtModbusTool.exe --release --no-compiler-runtime
# 然后手动复制运行时或要求用户安装 VC++ Redist
```

**方案 B：手动复制必要 DLL（最终采用）**
```yaml
Copy-Item $env:QT_ROOT_DIR\bin\Qt6Core.dll publish\
Copy-Item $env:QT_ROOT_DIR\bin\Qt6Gui.dll publish\
Copy-Item $env:QT_ROOT_DIR\bin\Qt6Widgets.dll publish\
Copy-Item $env:QT_ROOT_DIR\bin\Qt6SerialBus.dll publish\
Copy-Item $env:QT_ROOT_DIR\bin\Qt6SerialPort.dll publish\
```

**📝 教训：**
> 简化部署流程，手动复制核心文件比依赖 windeployqt 更可控。

---

### 问题 3：依赖模块缺失 (QtSerialPort)

**❌ 错误表现：**
```
error: undefined reference to `Qt6SerialPort'
CMake Error: Could not find Qt6SerialPort
```

**🔍 原因分析：**
- `QtSerialBus` 依赖 `QtSerialPort`
- 但 `install-qt-action` 默认只安装显式声明的模块

**✅ 解决方案：**
```yaml
modules: 'qtserialbus qtserialport'  # 显式声明两个模块
```

**📝 教训：**
> Qt 模块的依赖关系需要手动处理，不能假设自动包含。

---

### 问题 4：windeployqt 部署失败

**❌ 错误表现：**
```
windeployqt: Error: Cannot find module 'Qt6SerialBus'
windeployqt: Error: Cannot find module 'Qt6SerialPort'
```

**🔍 原因分析：**
- `windeployqt` 在 Actions 环境中路径识别有问题
- 某些 Qt 模块的部署元数据不完整

**✅ 解决方案：**
```yaml
# 放弃 windeployqt，改用手动复制
- name: 📁 打包程序
  run: |
    New-Item -ItemType Directory -Force -Path publish
    Copy-Item build\Release\QtModbusTool.exe publish\
    Copy-Item $env:QT_ROOT_DIR\bin\Qt6Core.dll publish\
    Copy-Item $env:QT_ROOT_DIR\bin\Qt6Gui.dll publish\
    Copy-Item $env:QT_ROOT_DIR\bin\Qt6Widgets.dll publish\
    Copy-Item $env:QT_ROOT_DIR\bin\Qt6SerialBus.dll publish\
    Copy-Item $env:QT_ROOT_DIR\bin\Qt6SerialPort.dll publish\
```

**📝 教训：**
> 在 CI/CD 环境中，手动控制比自动化工具更可靠。

---

### 问题 5：代码编译错误 (枚举类型)

**❌ 错误表现：**
```cpp
error: 'QModbusDevice::Error' is not a member of 'QModbusDevice'
error: 'QModbusDevice::UnknownError' was not declared
```

**🔍 原因分析：**
- Qt6 中 `QModbusDevice` 的错误枚举已更改
- 旧代码使用了 Qt5 的枚举值

**✅ 解决方案：**
```cpp
// 错误写法 (Qt5)
if (modbusDevice->error() != QModbusDevice::NoError)

// 正确写法 (Qt6)
if (modbusDevice->error() != QModbusDevice::DeviceError)
```

**📝 教训：**
> Qt5 到 Qt6 有 breaking changes，需要查阅官方迁移文档。

---

### 问题 6：权限问题 (PowerShell)

**❌ 错误表现：**
```
Access to the path 'publish' is denied
Cannot create file 'publish\QtModbusTool.exe': Access denied
```

**🔍 原因分析：**
- Windows runner 的目录权限设置严格
- 某些 PowerShell 命令需要显式创建目录

**✅ 解决方案：**
```yaml
# 使用 -Force 参数强制创建目录
New-Item -ItemType Directory -Force -Path publish

# 使用 Copy-Item 而不是 cp
Copy-Item build\Release\QtModbusTool.exe publish\
```

**📝 教训：**
> Windows Actions 中使用 PowerShell 原生命令比 Unix 命令更可靠。

---

## 🎯 最终稳定配置

### GitHub Actions (windows-build.yml)

```yaml
name: 🪟 Windows Build

on:
  push:
    branches: [ "main", "master" ]

jobs:
  build-windows:
    runs-on: windows-latest
    
    steps:
    - uses: actions/checkout@v4
      
    - uses: jurplel/install-qt-action@v4
      with:
        version: '6.5.0'
        host: 'windows'
        target: 'desktop'
        arch: 'win64_msvc2019_64'
        modules: 'qtserialbus qtserialport'
        cache: 'false'
        
    - run: cmake -S . -B build
      
    - run: cmake --build build --config Release
      
    - run: |
        New-Item -ItemType Directory -Force -Path publish
        Copy-Item build\Release\QtModbusTool.exe publish\
        Copy-Item $env:QT_ROOT_DIR\bin\Qt6Core.dll publish\
        Copy-Item $env:QT_ROOT_DIR\bin\Qt6Gui.dll publish\
        Copy-Item $env:QT_ROOT_DIR\bin\Qt6Widgets.dll publish\
        Copy-Item $env:QT_ROOT_DIR\bin\Qt6SerialBus.dll publish\
        Copy-Item $env:QT_ROOT_DIR\bin\Qt6SerialPort.dll publish\
        
    - run: Compress-Archive -Path publish\* -DestinationPath QtModbusTool-Windows-x64.zip
      
    - uses: actions/upload-artifact@v4
      with:
        name: QtModbusTool-Windows-x64
        path: QtModbusTool-Windows-x64.zip
```

### CMakeLists.txt 关键点

```cmake
cmake_minimum_required(VERSION 3.16)
project(QtModbusTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 只使用 Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets SerialBus)

qt_add_executable(QtModbusTool
    MANUAL_FINALIZATION
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
)

target_link_libraries(QtModbusTool PRIVATE
    Qt6::Widgets
    Qt6::SerialBus
)
```

---

## 📚 经验总结

### ✅ 最佳实践

1. **Qt 版本选择**
   - 使用 Qt 6.5.0 LTS 版本
   - 架构固定为 `win64_msvc2019_64`
   - 显式声明所有依赖模块

2. **部署策略**
   - 手动复制核心 DLL，不依赖 windeployqt
   - 使用 PowerShell 原生命令
   - 创建压缩包而非直接上传目录

3. **代码规范**
   - 遵循 Qt6 API，不混用 Qt5 语法
   - 错误枚举使用 Qt6 的新命名
   - 头文件包含完整

4. **CI/CD 配置**
   - 禁用缓存保证纯净编译
   - 明确指定 Release 模式
   - 使用 `windows-latest` runner

### ⚠️ 避坑指南

| 坑 | 正确做法 |
|----|----------|
| 使用 win64_msvc2022_64 | → 使用 win64_msvc2019_64 |
| 依赖 windeployqt | → 手动复制 DLL |
| 只声明 qtserialbus | → 同时声明 qtserialbus + qtserialport |
| 使用 Unix 命令 (cp, mkdir) | → 使用 PowerShell 命令 |
| Qt5 枚举语法 | → 查阅 Qt6 迁移文档 |
| 启用缓存 | → 禁用缓存保证一致性 |

---

## 🔗 相关资源

- **GitHub 仓库**: https://github.com/dgutwgf/QtModbusTool
- **Qt6 迁移指南**: https://doc.qt.io/qt-6/qtportingguide.html
- **install-qt-action**: https://github.com/jurplel/install-qt-action
- **GitHub Actions Windows Runner**: https://github.com/actions/runner-images

---

_🦞 小龙虾整理 · 2026-03-19_
