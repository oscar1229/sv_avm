# GenCameraTextrue 函数讲解

> 文件：`src/svrender/camera/camerarender.cpp`
> 作用：把 4 路摄像头图像「送进 GPU」变成可采样的纹理。
> 这是每帧渲染循环里被调用的第一步（主程序里的 `(1) 纹理上传`），
> 也是本次零拷贝优化的核心落点。

---

## 一、函数定位

`GenCameraTextrue` 是一个**分发器（dispatcher）**：它本身不直接干上传，
而是先判断本帧能不能走零拷贝，然后二选一：

```
GenCameraTextrue(img)
        │
        ├── 4 路都带有效 dma_fd  →  GenCameraTextrueZeroCopy(img)  (零拷贝路径)
        │                            成功 → bUseOES=TRUE, 返回
        │                            失败 ↓ 回退
        └── 否则 / 回退          →  普通 glTexImage2D/glTexSubImage2D 拷贝上传
                                     bUseOES=FALSE
```

`bUseOES` 这个成员变量是关键：它记录「本帧最终走的是哪条路」，
后面 `Render` 会据此选择对应的着色器程序和纹理类型。

---

## 二、GenCameraTextrue 逐段讲解

```cpp
SV_VOID InnerSv_CameraRenderClass::GenCameraTextrue(const std::vector<SV_IMAGE_S> &img) {
  if(img.size() < u32VAOVect.size())
    return;
```
**入参校验**：传入的图像数量必须 >= 网格（VAO）数量。每个 VAO 对应一路相机的
碗状网格，图像不够就没法给每个网格配纹理，直接返回不渲染。

```cpp
  //若所有通道都带有效dma_fd,优先走零拷贝路径
  SV_BOOL bAllHaveDmaFd = (img.size()>0) ? SV_TRUE : SV_FALSE;
  for(size_t i=0;i<img.size();++i) {
    if(img[i].s32DmaFd <= 0) { bAllHaveDmaFd = SV_FALSE; break; }
  }
```
**判断能否零拷贝**：逐路检查 `s32DmaFd`。只有 4 路**全部**带有效 dma_buf fd
（>0）时才走零拷贝。只要有一路是普通 RGB（fd=-1），就整体回退普通路径——
因为 `Render` 一帧只能用一套着色器程序，不能混用。

```cpp
  if(SV_TRUE == bAllHaveDmaFd && SV_TRUE == GenCameraTextrueZeroCopy(img)) {
    bUseOES = SV_TRUE;   //本帧用external纹理,Render走OES program
    return;
  }
  bUseOES = SV_FALSE;    //回退普通拷贝上传路径
```
**尝试零拷贝**：条件满足就调 `GenCameraTextrueZeroCopy`。注意这里用了
短路逻辑——零拷贝函数内部任一步失败会返回 `SV_FALSE`，此时不 return，
继续往下走普通路径。这就是「优雅降级」：零拷贝不可用（缺扩展、导入失败等）
也不会崩，自动退回能跑的老路。

```cpp
  //首次进入时初始化各纹理的"已分配尺寸"记录为0(表示尚未分配存储)
  if(stTexAllocSize.size() < img.size()) {
    stTexAllocSize.resize(img.size(), SV_SIZE_S{0, 0});
  }
```
**普通路径的尺寸缓存**：`stTexAllocSize` 记录每个纹理「上一次分配的宽高」。
这是为了区分「首次分配」和「后续更新」——见下面。

```cpp
  for(SV_S32 i=0;i<img.size();++i) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, u32TexObjVect[i]);  //绑定第 i 路的普通2D纹理
```
**逐路上传**：激活纹理单元 0，绑定第 i 路对应的 `GL_TEXTURE_2D` 纹理对象。

```cpp
  #ifdef CAMERA_USED
    glTexDirectVIVMap(...GL_VIV_YV12...);   //Vivante GPU专用的直接映射扩展
    glTexDirectInvalidateVIV(GL_TEXTURE_2D);
  #else
```
**编译期分支**：`CAMERA_USED` 宏下走 Vivante GPU 的 `glTexDirectVIVMap`
（另一种零拷贝/直接映射机制，针对 YV12）。当前 PowerVR 平台不定义这个宏，
走 `#else` 的标准 GLES 上传。

```cpp
    const SV_S32 s32W = img[i].stImageSize.s32Width;
    const SV_S32 s32H = img[i].stImageSize.s32Height;
    //RGB(3字节/像素)行无4字节对齐,设为1避免驱动走对齐慢路径
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
```
**对齐设置**：GLES 默认按 4 字节对齐读取每行像素。RGB 是 3 字节/像素，
行字节数（W×3）通常不是 4 的倍数，不设 `UNPACK_ALIGNMENT=1` 驱动会读错或
走慢路径。这是 RGB 上传的常见坑。

```cpp
    if(stTexAllocSize[i].s32Width != s32W || stTexAllocSize[i].s32Height != s32H) {
      //尺寸首次确定或发生变化:分配纹理存储
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, s32W, s32H, 0, GL_RGB, GL_UNSIGNED_BYTE, img[i].dataPtr);
      stTexAllocSize[i].s32Width  = s32W;
      stTexAllocSize[i].s32Height = s32H;
    } else {
      //尺寸不变:复用已分配存储,仅更新像素数据,避免每帧重分配
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, s32W, s32H, GL_RGB, GL_UNSIGNED_BYTE, img[i].dataPtr);
    }
```
**这是普通路径的核心优化**，区分两种调用：
- `glTexImage2D`：**分配并上传**。会重新申请 GPU 纹理显存，开销大。只在首帧
  或分辨率变化时调用。
- `glTexSubImage2D`：**只更新像素**，复用已有显存。后续帧都走这条，省掉每帧
  重分配。

即便如此，每帧仍要把 W×H×3 ≈ 2.6MB（单路）的像素从 CPU 拷进 GPU，
4 路约 10MB，实测约 25ms——这正是零拷贝要消灭的开销。

```cpp
  #endif
    glBindTexture(GL_TEXTURE_2D, 0);   //解绑
  }
}
```
解绑收尾。

---

## 三、GenCameraTextrueZeroCopy 逐段讲解（零拷贝路径）

```cpp
SV_BOOL InnerSv_CameraRenderClass::GenCameraTextrueZeroCopy(const std::vector<SV_IMAGE_S> &img) {
  if(SV_FALSE == _local::InnerSV_LoadZeroCopyProcs())
    return SV_FALSE;
```
**加载扩展函数指针**：`eglCreateImageKHR` / `eglDestroyImageKHR` /
`glEGLImageTargetTexture2DOES` 这些是 EGL/GLES 扩展，不在核心 API 里，
要用 `eglGetProcAddress` 动态取。取不到（驱动不支持）→ 返回 FALSE 回退。

```cpp
  EGLDisplay dpy = reinterpret_cast<EGLDisplay>(display::InnerSV_GetEglDisplay());
  if(EGL_NO_DISPLAY == dpy)
    return SV_FALSE;
```
**取 EGLDisplay**：建 EGLImage 需要 display 句柄。这正是 display 模块专门
新增 `InnerSV_GetEglDisplay()` 把内部句柄暴露出来的原因。

```cpp
  const size_t n = img.size();
  if(u32TexObjOESVect.size() < n || s32EglImageFdVect.size() < n || pEglImageVect.size() < n)
    return SV_FALSE;
```
**容器校验**：external 纹理对象、上次 fd 记录、EGLImage 句柄三个数组都要够 n 路。

```cpp
  for(size_t i=0;i<n;++i) {
    const SV_S32 s32W = img[i].stImageSize.s32Width;
    const SV_S32 s32H = img[i].stImageSize.s32Height;
    //dma_fd未变则复用已导入的EGLImage,无需重建
    if(s32EglImageFdVect[i] == img[i].s32DmaFd && pEglImageVect[i] != NULL)
      continue;
```
**★复用判断（最关键的性能点）**：如果这一路的 dma_fd 跟上次一样、且 EGLImage
已经建好，就**直接跳过**。这意味着——EGLImage 只在 fd 第一次出现时建一次，
后续帧零开销。这也是为什么零拷贝稳态能到 ~0.04ms：绝大多数帧这个循环里
4 路全部 `continue`，啥都不做。

> 静态测试图场景下 fd 永远不变，所以只有首帧真正建 EGLImage。
> 真实摄像头若用固定缓冲池循环（同几个 fd 轮转），同样命中复用。

```cpp
    //fd变化:先销毁旧EGLImage
    if(pEglImageVect[i] != NULL) {
      _local::pEglDestroyImageKHR(dpy, (EGLImageKHR)pEglImageVect[i]);
      pEglImageVect[i] = NULL;
    }
```
**fd 变了才走到这**：先销毁旧的 EGLImage 防泄漏，再重建。

```cpp
    //构造NV12双plane的dma_buf导入属性
    EGLint attribs[] = {
      EGL_WIDTH,  s32W,
      EGL_HEIGHT, s32H,
      EGL_LINUX_DRM_FOURCC_EXT, (EGLint)DRM_FORMAT_NV12,        //格式=NV12
      EGL_DMA_BUF_PLANE0_FD_EXT,     img[i].s32DmaFd,           //Y平面: 同一个fd
      EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,                          //  从0开始
      EGL_DMA_BUF_PLANE0_PITCH_EXT,  s32W,                       //  行距=W
      EGL_DMA_BUF_PLANE1_FD_EXT,     img[i].s32DmaFd,           //UV平面: 同一个fd
      EGL_DMA_BUF_PLANE1_OFFSET_EXT, (EGLint)(s32W*s32H),        //  从W*H偏移开始
      EGL_DMA_BUF_PLANE1_PITCH_EXT,  s32W,                       //  行距=W
      EGL_SAMPLE_RANGE_HINT_EXT,     EGL_YUV_NARROW_RANGE_EXT,   //窄范围(16-235)
      EGL_YUV_COLOR_SPACE_HINT_EXT,  EGL_ITU_REC601_EXT,         //BT.601色彩空间
      EGL_NONE
    };
```
**描述 dma_buf 的内存布局**：告诉 EGL「这块 dma_buf 里是怎么排的」。
NV12 是两个 plane 但都在同一块 buffer（同一个 fd）里：
- plane0 = Y，从 offset 0 开始，每行 W 字节
- plane1 = UV 交织，从 offset W×H（Y 平面之后）开始，每行 W 字节

色彩空间提示 BT.601 窄范围，要和主程序生成 NV12 时的约定一致，否则颜色会偏。
这套属性必须和 `LoadJPGImageNV12Dma` 写入 dma_buf 时的布局严格对应。

```cpp
    EGLImageKHR image = _local::pEglCreateImageKHR(dpy, EGL_NO_CONTEXT,
        EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)0, attribs);
    if(EGL_NO_IMAGE_KHR == image)
      return SV_FALSE;//导入失败,调用方回退普通路径
```
**导入 dma_buf 为 EGLImage**：`EGL_LINUX_DMA_BUF_EXT` 表示数据源是
dma_buf fd。这一步**没有任何像素拷贝**，只是让 EGLImage 引用那块物理内存。
失败就返回 FALSE，回退普通路径。

```cpp
    //绑定到external纹理
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, u32TexObjOESVect[i]);
    _local::pGlImageTargetTex2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)image);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
```
**EGLImage → external 纹理**：`glEGLImageTargetTexture2DOES` 把 EGLImage
绑到 `GL_TEXTURE_EXTERNAL_OES` 纹理对象。之后着色器用 `samplerExternalOES`
采样它，GPU 在采样时**硬件自动完成 NV12→RGB**，CPU 完全不参与。

```cpp
    pEglImageVect[i]     = (void*)image;       //记住句柄,供复用判断/下次销毁
    s32EglImageFdVect[i] = img[i].s32DmaFd;    //记住fd,供复用判断
  }
  return SV_TRUE;
}
```
**记账**：保存这次的 EGLImage 句柄和 fd，下一帧的复用判断和销毁都靠它们。

---

## 四、和 Render 的衔接

`GenCameraTextrue` 设好 `bUseOES` 后，同一帧的 `Render` 据此分流：

```cpp
const SV_BOOL bOES = (SV_TRUE==bUseOES && clProgramOES.GetHandle()!=0);
glUseProgram(bOES ? clProgramOES.GetHandle() : clProgram.GetHandle());   //选程序
glUniformMatrix4fv(bOES ? u32MvpUniformOES : u32MvpUniform, ...);        //对应uniform
...
for(...) {
  _local::InnerSV_RenderProcess(u32VAOVect[i], s32MeshSize[i],
      bOES ? u32TexObjOESVect[i] : u32TexObjVect[i],   //选external纹理 or 2D纹理
      bOES);
}
```
- 零拷贝帧：`clProgramOES`（`samplerExternalOES`）+ `u32TexObjOESVect`（external 纹理）
- 普通帧：`clProgram`（`sampler2D`）+ `u32TexObjVect`（2D 纹理）

上传路径和渲染路径靠 `bUseOES` 一个标志保持一致，互不串台。

---

## 五、一句话总结

`GenCameraTextrue` = 「能零拷贝就零拷贝，不能就回退」的纹理上传分发器。
零拷贝路径把 NV12 dma_buf 通过 EGLImage 直接喂给 GPU external 纹理，
且靠 fd 复用让 EGLImage 只建一次，把每帧约 25ms 的拷贝降到接近 0；
普通路径用 glTexImage2D/glTexSubImage2D 老老实实拷，作为永远可用的兜底。
