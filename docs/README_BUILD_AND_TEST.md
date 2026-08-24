# SV AVM 编译与运行指南

本文档说明如何使用 CMake 编译 SV AVM 库和渲染测试程序，以及如何运行测试。

## 目录

- [环境要求](#环境要求)
- [编译](#编译)
- [运行测试](#运行测试)
- [常见问题](#常见问题)
- [项目结构](#项目结构)

---

## 环境要求

### 系统环境
- **操作系统**: Ubuntu 20.04+ / Linux
- **编译器**: GCC 7.0+ (支持 C++14)
- **构建工具**: CMake 3.10+
- **显示环境**: X11 (用于 OpenGL 渲染窗口)

### Conda 环境

本项目使用 conda 环境 `py251_cu124` 管理依赖（OpenCV 4.x、glog 等）：

```bash
conda activate py251_cu124
```

环境路径：`/home/tianchi/miniforge3/envs/py251_cu124`

### 系统依赖

OpenGL ES / EGL、Assimp、X11 等需通过系统包管理器安装：

```bash
sudo apt install libgles2-mesa-dev libegl1-mesa-dev libx11-dev libassimp-dev
```

---

## 编译

项目使用 CMake 构建，根目录下的 `CMakeLists.txt` 会编译共享库 `libsv_avm.so` 和测试程序 `sv_avm_render_test`。

```bash
conda activate py251_cu124
cd /data/home2/tianchi/WorkSpace/code/sv_avm

mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

编译产物：

| 产物 | 路径 |
|------|------|
| 共享库 | `build/libsv_avm.so` |
| 测试程序 | `build/sv_avm_render_test` |

构建时会自动把运行所需的资源文件（`res/` 与 `_aParam.xml`）拷贝到可执行文件同级目录，无需手动准备。

### 重新编译

```bash
cd build
make -j$(nproc)
```

如需完全清理后重建：

```bash
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)
```

---

## 运行测试

### 资源文件

测试程序运行时需要以下资源（构建时已自动拷贝到可执行文件同级目录）：

- `res/imagech0.jpg` ~ `res/imagech3.jpg` — 4 张摄像头图像（左/右/前/后，1280x720）
- `res/concept_BUS cycles.dae` — 3D 车模
- `_aParam.xml` — 标定参数（可选，缺失时使用默认参数）

资源原件位于源码目录 `src/sv_avm_test/`，如需替换图像或车模，修改该目录下的文件后重新编译即可。

### 配置 X11 显示

程序需要 X11 显示环境来弹出 OpenGL 窗口。通过 MobaXterm 或 `ssh -X` 连接时，`DISPLAY` 会自动设置，确认其有值即可：

```bash
echo $DISPLAY     # 例如 localhost:10.0
```

### 运行

```bash
conda activate py251_cu124
cd /data/home2/tianchi/WorkSpace/code/sv_avm/build

export LD_LIBRARY_PATH=/data/home2/tianchi/WorkSpace/code/sv_avm/build:/home/tianchi/miniforge3/envs/py251_cu124/lib:$LD_LIBRARY_PATH

./sv_avm_render_test
```

### 预期输出

```
I20260609 11:02:51 display.cpp:94] CreateDisplay With X11
I20260609 11:02:52 sv_avm_render_main_test.cpp:128] Display size: 1707x960
I20260609 11:02:52 sv_avm_render_main_test.cpp:139] Loading camera images...
I20260609 11:02:52 sv_avm_render_main_test.cpp:92] Loaded image 0: ./res/imagech0.jpg (1280x720)
...
I20260609 11:02:52 sv_avm_render_main_test.cpp:147] Render loop started, press Ctrl+C to exit
```

程序会弹出 OpenGL 窗口，视口按实际显示分辨率自适应划分：左侧为正方形区域显示 3D 视角的环视拼接 + 车模，右侧为剩余宽度显示 2D 鸟瞰视角。约 50 FPS。

> 说明：视口尺寸由运行时实际显示分辨率动态计算（左侧 3D 取屏幕高度的正方形，右侧 2D 占剩余宽度），不再使用固定像素，避免在非 1920x1080 窗口下内容越界偏移。

按 `Ctrl+C` 退出。

---

## 常见问题

### 1. X11 连接失败

错误：`XOpenDisplay Failed` 或 `Authorization required`

`DISPLAY` 未设置或 X11 认证失败。通过 MobaXterm / `ssh -X` 连接以自动启用 X11 转发，并确认 `echo $DISPLAY` 有值。

### 2. 找不到 libsv_avm.so

错误：`error while loading shared libraries: libsv_avm.so`

运行前设置库路径：

```bash
export LD_LIBRARY_PATH=/data/home2/tianchi/WorkSpace/code/sv_avm/build:/home/tianchi/miniforge3/envs/py251_cu124/lib:$LD_LIBRARY_PATH
```

### 3. 编译找不到头文件

错误：`fatal error: opencv2/opencv.hpp: No such file or directory`

确认已激活 conda 环境：`conda activate py251_cu124`。

### 4. 图像文件缺失

警告：`Expected 4 camera images, got 0`

程序仍能运行，但只显示车模没有环视拼接背景。确认 `res/` 下有 4 张 `imagech0~3.jpg`。

---

## 项目结构

```
sv_avm/
├── CMakeLists.txt              # 根构建配置（编译 libsv_avm.so）
├── README_BUILD_AND_TEST.md
├── build/                      # 构建产物（执行 cmake/make 后生成）
├── include/                    # 公共头文件
└── src/
    ├── svrender/               # 渲染模块（display/camera/vehicle/common）
    ├── svmparam/               # 参数管理
    ├── svmcalibrate/           # 标定模块
    └── sv_avm_test/            # 测试程序
        ├── sv_avm_render_main_test.cpp
        ├── _aParam.xml         # 标定参数
        └── res/                # 资源文件（图像 + 车模）
```

### 测试程序功能

`sv_avm_render_test` 加载 4 张摄像头图像，初始化 OpenGL ES / EGL 显示，创建 MVP 视点变换，加载 3D 车模（DAE），渲染碗状投影的环视拼接效果，同时显示 3D 视角和 2D 鸟瞰视角。视口按实际显示分辨率自适应划分：

```
+-------------------+-------------+
|   3D 视角         |  2D 鸟瞰    |
|   (正方形)        |  (剩余宽度) |
|   环视拼接+车模   |  环视拼接+车模 |
+-------------------+-------------+
```

