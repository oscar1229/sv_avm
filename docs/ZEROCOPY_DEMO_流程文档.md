# SV AVM 零拷贝渲染 Demo 流程文档

> 本文档以「NV12 dma_buf 零拷贝」路径为主线，讲解 `sv_avm_render_test`
> 这个环视拼接（AVM, Around View Monitor）测试程序的完整流程：
> 每个模块做了什么、数据如何在 CPU 与 GPU 之间流转、主函数每一行的含义。
> 运行平台：RISC-V SpaceMIT + Imagination PowerVR B-Series BXM-4-64 GPU。

---

## 一、这个 Demo 在做什么

把 4 路摄像头图像（左/右/前/后，1280×720）投影到一个「碗状」3D 网格上，
拼接成环视效果，叠加 3D 车模，同时输出两个视图：

```
+-------------------+-------------+
|   3D 视角         |  2D 鸟瞰    |
|   (正方形)        |  (剩余宽度) |
|   环视拼接+车模   |  环视拼接+车模 |
+-------------------+-------------+
```

核心计算量在于：每帧都要把摄像头图像送进 GPU 当纹理，再用碗状网格做投影采样。
**「把图像送进 GPU」这一步就是本次零拷贝优化的对象。**

---

## 二、运行配置（config.json）

所有可调参数统一放在可执行文件同级的 `config.json`，程序启动时读取。
默认走零拷贝。改参数后无需重新编译，直接重跑即可（构建时 CMake 会把
`src/sv_avm_test/config.json` 自动拷到可执行目录）。

```json
{
  // 是否走 NV12 dma_buf 零拷贝路径（true=零拷贝[默认], false=普通RGB拷贝上传）
  "zero_copy": true,
  // 渲染帧数（跑满后自动停止并输出统计）
  "frames": 10,
  // 每帧节流睡眠(微秒)。20000=20ms; 设 0 关闭节流测帧率上限
  "sleep_us": 20000,
  // 碗面网格细分数（越大越平滑但 GPU 越慢；可选 90/180/360/720）
  "grid_subdiv": 180
}
```

| 字段 | 含义 | 默认 |
|---|---|---|
| `zero_copy` | true=NV12 dma_buf 零拷贝；false=普通 RGB 拷贝上传 | true |
| `frames` | 渲染帧数，跑满后停止并输出统计 | 10 |
| `sleep_us` | 每帧节流睡眠微秒数，0=关闭（测帧率上限）| 20000 |
| `grid_subdiv` | 碗面网格细分数，越大越平滑但 GPU 越慢 | 180 |

程序内部由 `SvRunConfig` 结构体承载这些值，`LoadConfigJson` 负责解析。
为了不给工程引入第三方 JSON 库，解析器是手写的极简版：只认本配置这几个
扁平的 `"key": value`，并能容忍 `//` 行注释；找不到文件时回落到结构体里的
内置默认值，程序照常运行。所有运行行为只由这个文件决定，不再读任何环境变量。

---

## 三、两条纹理上传路径的对比（优化的本质）

| | 普通 RGB 路径（baseline） | NV12 dma_buf 零拷贝路径 |
|---|---|---|
| 图像格式 | BGR/RGB 三通道 | NV12（Y 平面 + UV 交织平面）|
| 上传方式 | `glTexSubImage2D` 每帧把 ~10MB 像素从 CPU 内存拷进 GPU | dma_buf fd 直接被 GPU 映射，**无拷贝** |
| YUV→RGB | 不涉及（本就是 RGB）| GPU 采样时由驱动硬件自动完成 |
| 纹理类型 | `GL_TEXTURE_2D` + `sampler2D` | `GL_TEXTURE_EXTERNAL_OES` + `samplerExternalOES` |
| 每帧纹理上传耗时 | **~25 ms** | **~0.04 ms**（稳态，首帧 ~5ms 建 EGLImage）|
| 配置开关 | `"zero_copy": false` | `"zero_copy": true`（默认）|

零拷贝的关键思想：图像数据所在的那块物理内存（dma_buf），
让 CPU 和 GPU **共享同一份**，而不是各存一份、每帧来回拷贝。

---

## 四、零拷贝路径的数据流（端到端）

```
JPG 文件
  │  cv::imread → BGR
  │  cvtColor(BGR→I420)            [CPU, OpenCV]
  ▼
I420 (YUV420P: Y + U + V 三平面)
  │  重排 U/V 交织 → NV12          [CPU, 主程序 LoadJPGImageNV12Dma]
  ▼
NV12 数据写入 dma_buf
  │  DmaHeapAlloc 从 /dev/dma_heap 分配连续物理内存
  │  mmap 映射到用户空间 → memcpy/交织写入 → munmap
  ▼
dma_buf fd  (存入 SV_IMAGE_S.s32DmaFd)
  │  ───────── 以下每帧执行，但 fd 不变则只做一次 ─────────
  │  eglCreateImageKHR(EGL_LINUX_DMA_BUF_EXT, NV12 双 plane 属性)
  ▼
EGLImage
  │  glEGLImageTargetTexture2DOES → 绑定到 GL_TEXTURE_EXTERNAL_OES
  ▼
external 纹理（GPU 直接读 dma_buf，零拷贝）
  │  片元着色器 samplerExternalOES 采样
  │  驱动硬件自动 YUV(NV12) → RGB
  ▼
碗状网格投影渲染 → 屏幕
```

对比普通路径，省掉的就是「NV12 数据 → GPU 纹理存储」这一次每帧 10MB 的拷贝，
以及 CPU 端的 YUV→RGB 转换（改由 GPU 免费完成）。

---

## 五、各模块职责

| 模块 | 文件 | 职责 |
|---|---|---|
| display | `src/svrender/display/` | 创建 EGL/X11 显示与 GLES 上下文；暴露 `InnerSV_GetEglDisplay()` 供零拷贝建 EGLImage |
| camera | `src/svrender/camera/camerarender.cpp` | 生成碗状网格 VAO；纹理上传（两条路径）；渲染环视拼接 |
| vehicle | `src/svrender/vehicle/` | 加载 DAE 车模并渲染 |
| mvp | `src/svrender/common/viewtransoform/` | 视点/投影矩阵（3D 视角 vs 2D 鸟瞰）|
| shader | `src/svrender/common/shader/shader.hpp` | GLSL：`s_f_shader_ec`（普通 2D 纹理）/ `s_f_shader_ec_oes`（external 纹理）|
| svmparam | `src/svmparam/` | 从 XML 读标定参数（车体尺寸、各路相机内外参）|
| svtype | `common/svtype.hpp` | 公共数据结构，含 `SV_IMAGE_S`（新增 `s32DmaFd`）和 `SV_IMAGE_TYPE_NV12` |

### 零拷贝相关的关键改动点

1. `common/svtype.hpp`
   - 新增枚举 `SV_IMAGE_TYPE_NV12`
   - `SV_IMAGE_S` 新增字段 `SV_S32 s32DmaFd`（>0 表示该图带 dma_buf，走零拷贝）

2. `display.{hpp,cpp}`
   - 新增 `InnerSV_GetEglDisplay()`，把内部 `EGLDisplay` 句柄以 `void*` 暴露出来
     （零拷贝建 `EGLImage` 时需要它）

3. `shader.hpp`
   - 新增片元着色器 `s_f_shader_ec_oes`，与原 `s_f_shader_ec` 结构一致，
     但 `uniform sampler2D` 改为 `samplerExternalOES`，并加
     `#extension GL_OES_EGL_image_external_essl3 : require`

4. `camerarender.{hpp,cpp}`
   - `GenCameraTextrue` 检测到 4 路都带 `s32DmaFd` 时，调用 `GenCameraTextrueZeroCopy`
   - `GenCameraTextrueZeroCopy`：dma_buf fd → EGLImage → external 纹理；
     fd 未变化则复用已有 EGLImage，不每帧重建
   - `Render` 按本帧是否零拷贝（`bUseOES`）选用对应的着色器程序与纹理类型

5. `sv_avm_render_main_test.cpp`（测试主程序）
   - 新增 `SvRunConfig` 结构体 + `LoadConfigJson`，从 `config.json` 读全部运行参数
   - `LoadCameraFrames(bZeroCopy)` 按配置决定走 NV12 dma_buf 还是普通 RGB
   - 碗面细分数、帧数、节流均改为读配置，不再硬编码、不再读环境变量

---

## 六、主函数 `main()` 逐行讲解

### 6.1 初始化日志与信号

```cpp
google::InitGoogleLogging(argv[0]);   // 初始化 glog 日志系统
FLAGS_logtostderr = 1;                // 日志直接打到 stderr（终端可见）
signal(SIGINT,  signal_handler);      // 捕获 Ctrl+C，置退出标志 g_bExit
signal(SIGTERM, signal_handler);      // 捕获 kill 信号，同上
```

### 6.2 资源路径

```cpp
const char* s8XmlFile = "./_aParam.xml";              // 标定参数文件
const char* s8DaeFile = "./res/concept_BUS cycles.dae"; // 3D 车模文件
SV_F32 f32Translucency = 0.8;                          // 车模半透明度
```

### 6.3 加载运行配置 ★参数总入口

```cpp
SvRunConfig stCfg;
LoadConfigJson("./config.json", &stCfg);   // 读 config.json，失败用内置默认
LOG(INFO) << "Config: zero_copy=" << (stCfg.bZeroCopy ? "true" : "false")
          << ", frames=" << stCfg.s32Frames
          << ", sleep_us=" << stCfg.s32SleepUs
          << ", grid_subdiv=" << stCfg.s32GridSubdiv;

// 碗面网格参数,细分数来自配置(其余四项为固定的碗体几何参数)
SV_BOWL_GRID_PARAM_S stGridParam = {1.7, (SV_S32)stCfg.s32GridSubdiv, 100, 0.05, 0.7};
//                                  ↑    ↑
//                          碗底半径  网格细分数(来自 config.grid_subdiv)
```
`grid_subdiv` 越大碗面越平滑但 GPU 越慢；当前实测 GPU 渲染约 21ms/帧，
主要就消耗在网格投影上。`zero_copy / frames / sleep_us` 也都来自这里。

### 6.4 读标定参数

```cpp
SV_SIZE_S stVehicleSize;                          // 车体尺寸
std::vector<SV_CAMERA_PARAMS_S> stCameraParamsVector; // 4 路相机参数
LoadParamsFromXml(s8XmlFile, &stVehicleSize, &stCameraParamsVector);
// 从 XML 读车体尺寸 + 各路相机内外参；读失败则用默认参数
```

### 6.5 创建显示与 GLES 上下文 ★零拷贝前提

```cpp
svrender::display::InnerSV_CreateDisplay(NULL, NULL);
// 打开 X11/EGL 窗口，创建 EGLDisplay + EGLContext + EGLSurface 并 makeCurrent。
// 这一步之后才有 GL 上下文，纹理/shader/EGLImage 等才能创建。
// 零拷贝路径稍后会通过 InnerSV_GetEglDisplay() 取到这里创建的 EGLDisplay。

SV_SIZE_S stSize = svrender::display::InnerSV_GetDisplayFrameSize(); // 取屏幕分辨率
LOG(INFO) << "Display size: " << stSize.s32Width << "x" << stSize.s32Height;
```

### 6.6 初始化 MVP / 车模 / 相机渲染器

```cpp
svrender::mvp::InnerSV_MvCalss stMvClass;
stMvClass.Initialized();                 // 初始化视点/投影矩阵（3D、2D 两套）

svrender::vehicle::InnerSV_VehicleRenderClass stVehicleRenderClass(&stMvClass);
stVehicleRenderClass.Init(s8DaeFile, stVehicleSize, f32Translucency); // 加载 DAE 车模

svrender::camera::InnerSv_CameraRenderClass stCameraRenderClass(&stMvClass);
stCameraRenderClass.Init(stCameraParamsVector, stVehicleSize, stGridParam);
// 相机渲染器 Init 内部：
//   - 编译两套着色器程序：clProgram(普通) + clProgramOES(零拷贝 external)
//   - 按各路相机参数生成碗状网格 VAO
//   - 创建普通 2D 纹理对象 + external(OES) 纹理对象各 4 个
```

### 6.7 加载 4 路图像 ★零拷贝核心

```cpp
std::vector<SV_IMAGE_S> stImageVect = LoadCameraFrames(stCfg.bZeroCopy);
// config.zero_copy=true → 每张 JPG 走 LoadJPGImageNV12Dma：
//   imread→BGR→I420→重排NV12→dma_heap分配→mmap写入→返回 dma_fd
//   结果 SV_IMAGE_S.s32DmaFd 持有 fd，dataPtr=NULL，type=NV12
// 否则走 LoadJPGImage：imread→BGR，dataPtr 持有像素，s32DmaFd=-1

if (stImageVect.size() != 4) {    // 不足 4 路则告警（拼接会不完整）
    LOG(WARNING) << "Expected 4 camera images, got " << stImageVect.size() ...;
}
```

### 6.8 计算视口划分

```cpp
const SV_S32 s32ScreenW = stSize.s32Width;            // 屏幕宽
const SV_S32 s32ScreenH = stSize.s32Height;           // 屏幕高
const SV_S32 s32View3DW = s32ScreenH;                 // 3D 视口=正方形，边长=屏幕高
const SV_S32 s32View2DX = s32View3DW;                 // 2D 视口起点 x（接在 3D 右侧）
const SV_S32 s32View2DW = s32ScreenW - s32View3DW;    // 2D 视口宽=剩余宽度
const SV_RECT_S stView3D = {{0, 0}, {s32View3DW, s32ScreenH}};        // 左侧 3D 视口
const SV_RECT_S stView2D = {{s32View2DX, 0}, {s32View2DW, s32ScreenH}};// 右侧 2D 视口
// 按实际分辨率动态划分，避免硬编码在非 1920x1080 下越界。
```

### 6.9 取测试规模与节流（来自配置）

```cpp
const SV_S32 s32TargetFrames = stCfg.s32Frames;   // 渲染帧数 = config.frames
const SV_S32 s32SleepUs = stCfg.s32SleepUs;        // 每帧节流 = config.sleep_us
LOG(INFO) << "Benchmark: frames=" << s32TargetFrames << ", sleep_us=" << s32SleepUs;
// 两个值都已在 6.3 从 config.json 读好，这里直接取用。
```

### 6.10 计时容器与宏

```cpp
std::vector<SV_F64> vGenTexMs, vSubmitMs, vGpuMs, vSwapMs, vSleepMs, vFrameMs;
// 分别记录每帧 5 个阶段 + 整帧的耗时
#define SV_NOW(tv) gettimeofday(&(tv), NULL)                 // 取当前时刻
#define SV_MS(a,b) (((b).tv_sec-(a).tv_sec)*1000.0 + ((b).tv_usec-(a).tv_usec)/1000.0) // 两时刻差(ms)
gettimeofday(&stTvStart, NULL);   // 整体起始时刻（算总 FPS 用）
```

### 6.11 渲染主循环（核心）

```cpp
while (!g_bExit && s32FrameCount < s32TargetFrames) {
    struct timeval t0,t1,t2,t3,t4,t5;
    SV_NOW(t0);                                   // ── 帧开始
    svrender::display::InnerSV_DisplayClear();    // 清空颜色/深度缓冲

    // (1) 纹理上传 ★零拷贝发生处
    stCameraRenderClass.GenCameraTextrue(stImageVect);
    SV_NOW(t1);
    // 零拷贝：4 路 dma_fd 各导入/复用 EGLImage 并绑到 external 纹理(~0ms)
    // 普通：4 路各 glTexSubImage2D 拷贝上传(~25ms)

    // (2) 渲染提交：发 draw call（异步，不等 GPU 执行完）
    stCameraRenderClass.Render(...SV_ENUM_VIEWMODE_3D, stView3D);  // 3D 环视拼接
    stVehicleRenderClass.Render(...SV_ENUM_VIEWMODE_3D, stView3D); // 3D 车模
    stCameraRenderClass.Render(...SV_ENUM_VIEWMODE_2D, stView2D);  // 2D 鸟瞰拼接
    stVehicleRenderClass.Render(...SV_ENUM_VIEWMODE_2D, stView2D); // 2D 车模
    SV_NOW(t2);
    // 相机 Render 内部依 bUseOES 选 clProgramOES + external 纹理（零拷贝）
    // 或 clProgram + 2D 纹理（普通）

    // (3) GPU 完成：glFinish 阻塞直到 GPU 真正画完 → 反映 GPU 实际渲染耗时
    glFinish();
    SV_NOW(t3);

    // (4) SwapBuffers：把后台缓冲提交合成器显示（含 VSync/合成器节流）
    svrender::display::InnerSV_DisplaySwap();
    SV_NOW(t4);

    // (5) 节流睡眠：主动限速，降低 GPU/CPU 占用（sleep_us=0 时跳过）
    if (s32SleepUs > 0) usleep(s32SleepUs);
    SV_NOW(t5);

    // 汇总本帧各段耗时并打印
    SV_F64 f64Gen=SV_MS(t0,t1), f64Sub=SV_MS(t1,t2), f64Gpu=SV_MS(t2,t3),
           f64Swap=SV_MS(t3,t4), f64Sleep=SV_MS(t4,t5), f64Frame=SV_MS(t0,t5);
    vGenTexMs.push_back(f64Gen); ... vFrameMs.push_back(f64Frame);
    LOG(INFO) << "Frame " << s32FrameCount
              << " | tex=" << f64Gen << " submit=" << f64Sub
              << " gpu=" << f64Gpu << " swap=" << f64Swap
              << " sleep=" << f64Sleep << " | total=" << f64Frame << " ms";
    ++s32FrameCount;
}
gettimeofday(&stTvEnd, NULL);   // 整体结束时刻
```

5 个阶段含义速查：
- **tex**：纹理上传（零拷贝 vs 普通拷贝，优化对象）
- **submit**：CPU 发出绘制命令，异步，不代表 GPU 画完
- **gpu**：`glFinish` 等到 GPU 真画完，反映真实渲染开销
- **swap**：交换缓冲提交显示
- **sleep**：人为节流

### 6.12 统计输出

```cpp
SV_F64 f64ElapsedSec = (stTvEnd - stTvStart);              // 总耗时(秒)
SV_F64 f64Fps = s32FrameCount / f64ElapsedSec;            // 平均 FPS
LOG(INFO) << "Rendered " << s32FrameCount << " frames in " << f64ElapsedSec
          << " s, FPS = " << f64Fps;

// 对 6 个指标各算 avg/min/max（含首帧）
// 再算「剔除首帧」的稳态均值——首帧含一次性建 EGLImage 开销，剔除后更代表持续运行
```
之所以分「含首帧」和「稳态」两组：首帧要建 EGLImage、首次分配纹理存储，
比后续帧慢；稳态均值才是持续运行时的真实水平。

### 6.13 资源释放

```cpp
ReleaseCameraFrames(stImageVect);
// 普通路径 free(dataPtr)；零拷贝路径 close(s32DmaFd) 关闭 dma_buf fd
svrender::display::InnerSV_DeleteDisplay(0);  // 销毁 EGL 上下文/窗口
google::ShutdownGoogleLogging();
return 0;
```

---

## 七、相机渲染器内部（零拷贝关键函数）

### `GenCameraTextrueZeroCopy`（camerarender.cpp）

```cpp
1. eglGetProcAddress 取扩展函数指针：
   eglCreateImageKHR / eglDestroyImageKHR / glEGLImageTargetTexture2DOES
2. 取 EGLDisplay：display::InnerSV_GetEglDisplay()
3. 逐路相机：
   - 若 dma_fd 与上次相同且 EGLImage 已存在 → 复用，跳过（关键优化：不每帧重建）
   - 否则销毁旧 EGLImage，按 NV12 双 plane 属性 eglCreateImageKHR：
       plane0 = Y  (offset=0,     pitch=W)
       plane1 = UV (offset=W*H,   pitch=W)
       色彩空间 BT.601 窄范围
   - glEGLImageTargetTexture2DOES 把 EGLImage 绑到 external 纹理
4. 任一路失败 → 返回 SV_FALSE，调用方回退普通拷贝路径
```

### `Render` 的程序选择

```cpp
bOES = (bUseOES && clProgramOES 可用);
glUseProgram(bOES ? clProgramOES : clProgram);      // 选着色器程序
// 渲染时 bOES ? 绑 GL_TEXTURE_EXTERNAL_OES : 绑 GL_TEXTURE_2D
```

---

## 八、性能实测（PowerVR BXM-4-64，稳态/剔除首帧）

| 阶段 | 普通 RGB 路径 | NV12 零拷贝 | 说明 |
|---|---|---|---|
| tex 纹理上传 | ~25 ms | **~1.2 ms**（第3帧起 ~0.04ms）| 零拷贝核心收益 |
| submit 渲染提交 | ~0.4 ms | ~0.4 ms | 一致 |
| gpu GPU 渲染 | ~21 ms | ~21 ms | 一致，当前真正瓶颈 |
| swap SwapBuffers | ~0.2 ms | ~0.2 ms | 一致 |
| sleep 节流 | 20 ms | 20 ms | 人为限速，可关 |
| **total 整帧** | **~46 ms** | **~43 ms** | — |

结论：
1. 零拷贝把纹理上传从 25ms 降到接近 0，CPU 端拷贝与 YUV→RGB 转换全部消除。
2. 当前帧率瓶颈已转移到 **GPU 渲染本身（~21ms）** 和 **节流（20ms）**。
3. 想进一步提帧率：在 `config.json` 里把 `sleep_us` 设 0（关节流，上限约 43 FPS），
   或把 `grid_subdiv` 从 180 降到 90（减小 GPU 负载，画质换帧率）。

---

## 九、运行方式

所有行为都由 `config.json` 决定，命令行不带任何参数：

```bash
cd /home/szl/code/sv_avm/build
export LD_LIBRARY_PATH=/home/szl/c_envs/opencv/opencv413_install/lib/:$PWD

# 直接运行(读同级 config.json, 默认零拷贝)
./sv_avm_render_test
```

要改测试行为，编辑可执行目录同级的 `config.json` 后重跑即可，无需重新编译：
- 测对照（普通 RGB 路径）：把 `"zero_copy"` 改为 `false`
- 测帧率上限：把 `"sleep_us"` 改为 `0`、`"frames"` 调大（如 60）
- 画质换帧率：把 `"grid_subdiv"` 从 180 降到 90

> 源码里的 `src/sv_avm_test/config.json` 是模板，CMake 构建时会自动拷到
> 可执行目录。要持久化默认值，改源码模板；要临时试参数，改 build 目录下那份。
