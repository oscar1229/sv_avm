/*
 * display.cpp
 *
 */
#include "display.hpp"

#include <string>
#include <vector>
#include <sys/stat.h>
#include <opencv2/opencv.hpp>

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include<sys/ioctl.h>
#include <sys/select.h>
//包含errno所需要的头文件
#include <errno.h>
#include<pthread.h>
#include <linux/input.h> //inputevent
#include<glog/logging.h> //glog

//EGL
#ifdef EGL_USE_X11
#define  XLIB_ILLEGAL_ACCESS
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
//#else
//#define MESA_EGL_NO_X11_HEADERS
#endif
#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
//OpenGL
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

namespace sm {
namespace sv_avm {
namespace svrender {
namespace display {
static const EGLint s_configAttribs[] =
{
        EGL_SAMPLES, 0,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 8,
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,
        EGL_NONE
};
//局部变量，保存获取的窗口系统显示类型的指针
#ifdef EGL_USE_X11
static Display* g_pEglNativeDisplayType = NULL;
#else
static EGLNativeDisplayType g_pEglNativeDisplayType = NULL;
#endif
//局部变量，保存获取的显示窗口的指针
static EGLDisplay g_pEglDisplay = NULL;
//局部变量，保存生成的渲染平面的指针
static EGLSurface g_pEglsurface = NULL;
static EGLContext g_pEglContex = NULL;
//互斥锁，防止多线程调用
static pthread_mutex_t g_stDisplayMutex = {0};
//互斥锁，防止多线程调用swapbuffer操作
static pthread_mutex_t g_stDisplaySwapMutex = {0};
//局部变量，离屏渲染(无显示器)相关配置与状态
static SV_BOOL g_bOffscreenMode = SV_FALSE;      //SV_TRUE表示当前使用pbuffer离屏渲染
static SV_BOOL g_bForceOffscreen = SV_FALSE;     //SV_TRUE表示强制离屏渲染,即使显示器可用
static std::string g_strOffscreenPath = "";      //离屏渲染结果保存路径,空串表示禁用离屏渲染
static SV_S32 g_s32OffscreenWidth = 0;           //离屏渲染宽度
static SV_S32 g_s32OffscreenHeight = 0;          //离屏渲染高度

#ifndef EGL_USE_X11
static SV_S32 g_s32KeyBoadFd = -1; //键盘文件设备
static SV_S32 g_s32MouseFd = -1;//鼠标设备
static fd_set g_FdSets;
static SV_S32 g_s32FdMax = -1;
static struct input_event g_stInevent = {};//输入事件
static SV_BOOL  g_bBtn_mouse_left = SV_FALSE;
static SV_VOID InnerSV_OpenInputDev(const char* s8KeyBoardDev, const char* s8MouseDev);
static SV_VOID InnerSV_CloseInputDev(SV_VOID);
static SV_S32 InnerSV_KeyBoardEventProcessFb(SV_VOID);
static SV_S32 InnerSV_MouseEventProcessFb(SV_VOID);
#else
static SV_F32  g_af32Mouse_pos[2] = {0.0, 0.0};
static SV_S32 InnerSV_KeyBoardEventProcessX11(const XEvent &report);
static SV_S32 InnerSV_MouseEventProcessX11(const XEvent &report);
#endif
static SV_F32  g_af32Mouse_offset[2] = {0.0, 0.0};

#ifdef EGL_USE_X11
static SV_BOOL bX11NativeDisplayCreate(Display **ppNativeType, Window* pWindow)
{
if (getenv("DISPLAY") == NULL) {
    setenv("DISPLAY", ":0.0", 1);
}
LOG(INFO) << "CreateDisplay With X11";
Display* pEglNativeType = XOpenDisplay(NULL);
    if (pEglNativeType == NULL) {
    LOG(WARNING) << "XOpenDisplay Failed\n";
    return SV_FALSE;
    }
    SV_S32 s32Screen = DefaultScreen(pEglNativeType);
    Window rootwindow = RootWindow(pEglNativeType, s32Screen);
    Window window = XCreateSimpleWindow(pEglNativeType, rootwindow, \
        0, 0, pEglNativeType->screens->width, pEglNativeType->screens->height, 0, 0, \
        BlackPixel(pEglNativeType, s32Screen));
    XMapWindow(pEglNativeType, window);
   *pWindow = window;
   *ppNativeType = pEglNativeType;
    return SV_TRUE;
}
#else
static SV_S32 s32FbFd = -1;
static std::string InnerSV_S8GetDbDevName(const SV_S32 &s32FbDev) {
std::string s8NodeStr_temp = "/dev/fb";
    SV_S8 s8dev_temp = '0'+s32FbDev;
    s8NodeStr_temp+=s8dev_temp;
    return s8NodeStr_temp;
}
static SV_S32 InnserSV_s32OpenFbDev(const SV_S32 &s32FbDev) {
std::string s8NodeStr_temp = InnerSV_S8GetDbDevName(s32FbDev);
setenv("FB_MULTI_BUFFER", "4", 1);
s32FbFd = open(s8NodeStr_temp.c_str(), O_RDWR, NULL);
if (s32FbFd < 0){
    LOG(ERROR) << "open " << s8NodeStr_temp.c_str() << "failed with:" << strerror(errno);
    goto err;
}
struct fb_fix_screeninfo stScreenFix;
if (ioctl(s32FbFd, FBIOGET_FSCREENINFO, &stScreenFix) < 0) {
    LOG(ERROR) << "FBIOGET_VSCREENINFO failed with:" << strerror(errno);
    close(s32FbFd);
    s32FbFd = -1;
    goto err;
    }
struct fb_var_screeninfo stScreen_info;
if (ioctl(s32FbFd, FBIOGET_VSCREENINFO, &stScreen_info) < 0) {
    LOG(ERROR) << "FBIOGET_VSCREENINFO failed with:" << strerror(errno);
    close(s32FbFd);
    s32FbFd = -1;
    goto err;
}
stScreen_info.bits_per_pixel = 16;//Set the background to 32-bpp EGL只支持16-bpp及32-bpp
if (ioctl(s32FbFd, FBIOPUT_VSCREENINFO, &stScreen_info) < 0) {
    LOG(ERROR) << "FBIOPUT_VSCREENINFO failed with:" << strerror(errno);
    close(s32FbFd);
    s32FbFd = -1;
    goto err;
}
if (ioctl(s32FbFd, FBIOBLANK, FB_BLANK_UNBLANK) < 0) {
    LOG(ERROR) << "FB_BLANK_UNBLANK failed with:" << strerror(errno);
    close(s32FbFd);
    s32FbFd = -1;
    goto err;
}
close(s32FbFd);
return SV_SUCESSED;
err:
close(s32FbFd);
s32FbFd = -1;
return SV_FAILURED;
}
static SV_S32 InnerSV_s32CloseFbDev(const SV_S32 &s32FbDev) {
std::string s8NodeStr_temp = InnerSV_S8GetDbDevName(s32FbDev);
s32FbFd = open(s8NodeStr_temp.c_str(), O_RDWR, NULL);
if (s32FbFd >= 0) {
    ioctl(s32FbFd, FBIOBLANK, FB_BLANK_POWERDOWN);
    close(s32FbFd);
}
return SV_SUCESSED;
}

static SV_BOOL bFbNativeDisplayCreate(const SV_S32& s32FbDevIdx, EGLNativeDisplayType *ppNativeType, EGLNativeWindowType* pWindow) {
CHECK(InnserSV_s32OpenFbDev(s32FbDevIdx) == SV_SUCESSED) << "Open FBdev failed";
SV_S32 s32W, s32H;
EGLNativeDisplayType pEglNativeType = fbGetDisplayByIndex(s32FbDevIdx);
if (pEglNativeType == NULL) {
    LOG(WARNING) << "fbGetDisplayByIndex Failed\n";
    return SV_FALSE;
    }
EGLNativeWindowType pNativeWindow =  fbCreateWindow(pEglNativeType, 0, 0, 0, 0);
if (pNativeWindow == NULL) {
     //软失败:交由调用方决定是否退化为离屏渲染
    LOG(WARNING) << "fbCreateWindow failed";
    return SV_FALSE;
    }
  *pWindow = pNativeWindow;
  *ppNativeType = pEglNativeType;
return SV_TRUE;
}
#endif
#ifdef EGL_USE_X11
SV_VOID InnerSV_CreateDisplay(const char* s8KeyBoardDev, const char* s8MouseDev) {
#else
SV_VOID InnerSV_CreateDisplay(const SV_S32& s32FbDevIdx, const char* s8KeyBoardDev, const char* s8MouseDev) {
#endif
pthread_mutex_trylock(&g_stDisplayMutex);

if (NULL != g_pEglNativeDisplayType) {
    pthread_mutex_unlock(&g_stDisplayMutex);
    LOG(WARNING) << "Native Display opened;please close it first";
    return ;
}
  //强制离屏时跳过显示器打开;否则先尝试打开显示器,失败则退化为离屏渲染
SV_BOOL bNativeOk = SV_FALSE;
#ifdef EGL_USE_X11
Window window = 0;
if (SV_TRUE != g_bForceOffscreen) {
    bNativeOk = bX11NativeDisplayCreate(&g_pEglNativeDisplayType, &window);
    if (SV_TRUE == bNativeOk) {
        XSelectInput(g_pEglNativeDisplayType, window, KeyPressMask | PointerMotionMask | ButtonPressMask);
    }
}
#else
EGLNativeWindowType window = 0;
if (SV_TRUE != g_bForceOffscreen) {
    InnerSV_OpenInputDev(s8KeyBoardDev, s8MouseDev);
    bNativeOk = bFbNativeDisplayCreate(s32FbDevIdx, &g_pEglNativeDisplayType, &window);
}
#endif
if (SV_TRUE != bNativeOk) {
    //无可用显示器:未配置离屏路径则保持原有的致命错误语义
    CHECK(!g_strOffscreenPath.empty())
        << (SV_TRUE == g_bForceOffscreen
                ? "force_offscreen is on but offscreen_output_path is not configured"
                : "No display available and offscreen_output_path is not configured");
    LOG(WARNING) << (SV_TRUE == g_bForceOffscreen ? "force_offscreen enabled;" : "No display available;")
                << " using offscreen rendering: "
                << g_s32OffscreenWidth << "x" << g_s32OffscreenHeight
                << " -> " << g_strOffscreenPath;
    g_bOffscreenMode = SV_TRUE;
#ifdef EGL_USE_X11
    //X11下若已打开Display需先关闭,避免泄漏
    if (NULL != g_pEglNativeDisplayType) {
        XCloseDisplay(g_pEglNativeDisplayType);
    }
#endif
    g_pEglNativeDisplayType = static_cast<decltype(g_pEglNativeDisplayType)>(EGL_DEFAULT_DISPLAY);
}
    g_pEglDisplay = eglGetDisplay(g_pEglNativeDisplayType);
    CHECK(EGL_NO_DISPLAY != g_pEglDisplay);
    CHECK(eglGetError() == EGL_SUCCESS);
    SV_S32 s32VersionM, s32VersionMinro;
    eglInitialize(g_pEglDisplay, &s32VersionM, &s32VersionMinro);
    CHECK(EGL_SUCCESS == eglGetError());
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLConfig   eglconfig = NULL;
    SV_S32 s32ConfigNumbers;
   //先尝试窗口表面;显示器已关闭时窗口表面会创建失败,此时退化为pbuffer
    if (SV_TRUE != g_bOffscreenMode) {
    eglChooseConfig(g_pEglDisplay, s_configAttribs, &eglconfig, 1, &s32ConfigNumbers);
    if (EGL_SUCCESS != eglGetError() || s32ConfigNumbers <= 0) {
        LOG(WARNING) << "No EGL config for window surface;will try offscreen";
        g_pEglsurface = EGL_NO_SURFACE;
    } else {
        g_pEglsurface = eglCreateWindowSurface(g_pEglDisplay, eglconfig, window, NULL);
        const EGLint s32WinErr = eglGetError();
        if (EGL_SUCCESS != s32WinErr || EGL_NO_SURFACE == g_pEglsurface) {
        LOG(WARNING) << "eglCreateWindowSurface failed (EGL error 0x" << std::hex << s32WinErr
                        << std::dec << ");display may be powered off";
        g_pEglsurface = EGL_NO_SURFACE;
        }
    }
    if (EGL_NO_SURFACE == g_pEglsurface) {
       //窗口表面不可用:已配置离屏路径则退化为离屏,否则保持致命错误语义
        CHECK(!g_strOffscreenPath.empty())
            << "Window surface unavailable and offscreen_output_path is not configured";
        LOG(WARNING) << "Falling back to offscreen rendering: "
                    << g_s32OffscreenWidth << "x" << g_s32OffscreenHeight
                    << " -> " << g_strOffscreenPath;
        g_bOffscreenMode = SV_TRUE;
    }
    }
    if (SV_TRUE == g_bOffscreenMode) {
     //离屏渲染:选择pbuffer配置并创建pbuffer表面
    static const EGLint s_pbufferConfigAttribs[] = {
        EGL_SAMPLES, 0,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_NONE
    };
    eglChooseConfig(g_pEglDisplay, s_pbufferConfigAttribs, &eglconfig, 1, &s32ConfigNumbers);
    CHECK(EGL_SUCCESS == eglGetError());
    CHECK(s32ConfigNumbers > 0) << "No EGL config supports EGL_PBUFFER_BIT";
    const EGLint aPbufferAttribs[] = {
        EGL_WIDTH, g_s32OffscreenWidth,
        EGL_HEIGHT, g_s32OffscreenHeight,
        EGL_NONE
    };
    g_pEglsurface = eglCreatePbufferSurface(g_pEglDisplay, eglconfig, aPbufferAttribs);
    CHECK(EGL_SUCCESS == eglGetError());
    CHECK(EGL_NO_SURFACE != g_pEglsurface) << "eglCreatePbufferSurface failed";
    }
    EGLint ContextAttribList[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    g_pEglContex = eglCreateContext(g_pEglDisplay, eglconfig, EGL_NO_CONTEXT, ContextAttribList);
    CHECK(EGL_SUCCESS == eglGetError());
    eglMakeCurrent(g_pEglDisplay, g_pEglsurface, g_pEglsurface, g_pEglContex);
    CHECK(EGL_SUCCESS == eglGetError());
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

SV_VOID InnerSV_DisplayClear(SV_VOID) {
 // glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

SV_VOID InnerSV_DisplaySwap(SV_VOID) {
CHECK(NULL != g_pEglDisplay || NULL != g_pEglsurface);
  //离屏模式下不做swap:保留后台缓冲内容,便于退出前读回渲染结果
if (SV_TRUE == g_bOffscreenMode) {
    return ;
}
  //pthread_mutex_lock(&g_stDisplaySwapMutex);
    eglSwapBuffers(g_pEglDisplay, g_pEglsurface);
  //pthread_mutex_unlock(&g_stDisplaySwapMutex);
}

SV_VOID InnerSV_SetOffscreenConfig(const char* s8OutputPath, const SV_S32& s32Width, const SV_S32& s32Height) {
if (NULL == s8OutputPath || '\0' == s8OutputPath[0]) {
    g_strOffscreenPath.clear();
    return ;
}
if (s32Width <= 0 || s32Height <= 0) {
    LOG(WARNING) << "Invalid offscreen size " << s32Width << "x" << s32Height
                << ";offscreen rendering disabled";
    g_strOffscreenPath.clear();
    return ;
}
g_strOffscreenPath = s8OutputPath;
g_s32OffscreenWidth = s32Width;
g_s32OffscreenHeight = s32Height;
}

SV_VOID InnerSV_SetForceOffscreen(const SV_BOOL& bForce) {
g_bForceOffscreen = bForce;
}

SV_BOOL InnerSV_bIsOffscreenMode(SV_VOID) {
return g_bOffscreenMode;
}

SV_BOOL InnerSV_bSaveOffscreenFrame(SV_VOID) {
if (SV_TRUE != g_bOffscreenMode) {
    return SV_FALSE;
}
if (NULL == g_pEglDisplay || NULL == g_pEglsurface) {
    LOG(ERROR) << "Offscreen surface not available;cannot save frame";
    return SV_FALSE;
}
const SV_S32 s32W = g_s32OffscreenWidth;
const SV_S32 s32H = g_s32OffscreenHeight;
  //RGBA是GLES2下glReadPixels唯一保证支持的格式
std::vector<SV_U8> vRgba((size_t)s32W * (size_t)s32H * 4U);
glReadPixels(0, 0, s32W, s32H, GL_RGBA, GL_UNSIGNED_BYTE, vRgba.data());
const GLenum enGlErr = glGetError();
if (GL_NO_ERROR != enGlErr) {
    LOG(ERROR) << "glReadPixels failed: 0x" << std::hex << enGlErr;
    return SV_FALSE;
}
cv::Mat mRgba(s32H, s32W, CV_8UC4, vRgba.data());
cv::Mat mBgr;
cv::cvtColor(mRgba, mBgr, cv::COLOR_RGBA2BGR);
  //GL原点在左下,图片原点在左上,需垂直翻转
cv::flip(mBgr, mBgr, 0);

  //输出路径含目录时先创建目录
const size_t szSlash = g_strOffscreenPath.find_last_of('/');
if (std::string::npos != szSlash && 0 != szSlash) {
    const std::string strDir = g_strOffscreenPath.substr(0, szSlash);
    if (0 != mkdir(strDir.c_str(), 0755) && EEXIST != errno) {
        LOG(WARNING) << "mkdir " << strDir << " failed: " << strerror(errno);
    }
}
if (!cv::imwrite(g_strOffscreenPath, mBgr)) {
    LOG(ERROR) << "Failed to write offscreen frame: " << g_strOffscreenPath;
    return SV_FALSE;
}
LOG(INFO) << "Offscreen frame saved: " << g_strOffscreenPath
            << " (" << s32W << "x" << s32H << ")";
return SV_TRUE;
}

SV_VOID InnerSV_DeleteDisplay(const SV_S32& s32FbDevIdx) {
  //离屏模式下g_pEglNativeDisplayType可能为EGL_DEFAULT_DISPLAY(0),需单独判断以释放EGL资源
if (SV_TRUE == g_bOffscreenMode) {
    if (NULL != g_pEglDisplay) {
        eglDestroyContext(g_pEglDisplay, g_pEglContex);
        eglDestroySurface(g_pEglDisplay, g_pEglsurface);
        eglMakeCurrent(g_pEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(g_pEglDisplay);
        eglReleaseThread();
    }
#ifdef EGL_USE_X11
    //窗口表面创建失败而退化为离屏时,X Display仍处于打开状态,需关闭
    if (NULL != g_pEglNativeDisplayType) {
        XCloseDisplay(g_pEglNativeDisplayType);
    }
#else
    InnerSV_CloseInputDev();
#endif
    g_bOffscreenMode = SV_FALSE;
} else if (NULL != g_pEglNativeDisplayType) {
    eglDestroyContext(g_pEglDisplay, g_pEglContex);
    eglDestroySurface(g_pEglDisplay, g_pEglsurface);
    eglMakeCurrent(g_pEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(g_pEglDisplay);
    eglReleaseThread();
 #ifdef EGL_USE_X11
    XCloseDisplay(g_pEglNativeDisplayType);
#else
    InnerSV_s32CloseFbDev(s32FbDevIdx);
    InnerSV_CloseInputDev();
#endif
}
g_pEglDisplay = NULL;
g_pEglNativeDisplayType = NULL;
g_pEglsurface = NULL;
g_pEglContex = NULL;
pthread_mutex_unlock(&g_stDisplayMutex);
}


SV_SIZE_S InnerSV_GetDisplayFrameSize(SV_VOID) {
  //离屏模式下无显示器可查询,直接返回配置的离屏渲染尺寸
if (SV_TRUE == g_bOffscreenMode) {
    SV_SIZE_S stOffscreenSize;
    stOffscreenSize.s32Width = g_s32OffscreenWidth;
    stOffscreenSize.s32Height = g_s32OffscreenHeight;
    return stOffscreenSize;
}
CHECK(g_pEglNativeDisplayType != NULL) << "pEglNativeDisplayType==NULL";
#ifdef EGL_USE_X11
    SV_SIZE_S stSize;
    stSize.s32Width = g_pEglNativeDisplayType->screens->width;
    stSize.s32Height = g_pEglNativeDisplayType->screens->height;
#else
SV_SIZE_S stSize;
long unsigned int u32Physical;
SV_S32 s32Stride, s32BitsPerPixel;
fbGetDisplayInfo(g_pEglNativeDisplayType, &stSize.s32Width, &stSize.s32Height, &u32Physical, reinterpret_cast<int*>(&s32Stride), &s32BitsPerPixel);
#endif
return stSize;
}

SV_VOID* InnerSV_GetEglDisplay(SV_VOID) {
return reinterpret_cast<SV_VOID*>(g_pEglDisplay);
}

SV_S32 InnerSV_DisplayGetEventNum(SV_VOID) {
#ifdef EGL_USE_X11
return XPending(g_pEglNativeDisplayType);
#else
g_s32FdMax = 0;
struct timeval timeout = {1, 0}; //select等待1秒，1秒轮询，要非阻塞就置0
FD_ZERO(&g_FdSets);
if (-1 != g_s32KeyBoadFd) {
    FD_SET(g_s32KeyBoadFd, &g_FdSets);
    g_s32FdMax = (g_s32FdMax < (g_s32KeyBoadFd+1)?g_s32KeyBoadFd+1:g_s32FdMax);
}
if (-1 != g_s32MouseFd) {
    FD_SET(g_s32MouseFd, &g_FdSets);
    g_s32FdMax = (g_s32FdMax < (g_s32MouseFd+1)?g_s32MouseFd+1:g_s32FdMax);
}
if (g_s32FdMax == 0)
    return 0;
if (select(g_s32FdMax, &g_FdSets, NULL, NULL, &timeout) > 0) {
    if (FD_ISSET(g_s32KeyBoadFd, &g_FdSets)) {
        LOG(INFO) << "ir keyboard";
        if (read(g_s32KeyBoadFd, &g_stInevent, sizeof(g_stInevent)) == sizeof(g_stInevent))
        if ((g_stInevent.type == EV_KEY) && (g_stInevent.value == 1))
        return 1;
    }
    if (FD_ISSET(g_s32MouseFd, &g_FdSets)) {
        if (read(g_s32MouseFd, &g_stInevent, sizeof(g_stInevent)) == sizeof(g_stInevent))
        if ((g_stInevent.type == EV_KEY) && (g_stInevent.value == 1))
        return 1;
    }
}
return 0;
#endif
}

SV_S32 InnerSV_DisplayNextEvent(SV_VOID) {
#ifdef EGL_USE_X11
XEvent report;
        XNextEvent(g_pEglNativeDisplayType, &report);
        if (KeyPress == report.type) {
        return InnerSV_KeyBoardEventProcessX11(report);
        }
        else if ((MotionNotify == report.type) || (ButtonPress == report.type)) {
        return InnerSV_MouseEventProcessX11(report);
        }
#else
if (EV_KEY == g_stInevent.type) {
    return InnerSV_KeyBoardEventProcessFb();
}
else if (EV_REL == g_stInevent.type) {
    return InnerSV_MouseEventProcessFb();
}
#endif
else
    return InnerSV_ENUM_KEY_NONE_E;
}

#ifndef EGL_USE_X11
static SV_VOID InnerSV_OpenInputDev(const char* s8KeyBoardDev, const char* s8MouseDev) {
if (s8KeyBoardDev != NULL) {
    g_s32KeyBoadFd = open(s8KeyBoardDev, O_RDONLY |O_NONBLOCK);
    if (g_s32KeyBoadFd < 0) {
        LOG(ERROR) << "Faild to open KeyBooard Dev";
    }
}
if (s8MouseDev != NULL) {
    g_s32MouseFd =  open(s8MouseDev, O_RDONLY |O_NONBLOCK);
    if (g_s32MouseFd < 0) {
        LOG(ERROR) << "Faild to open Mouse Dev";
    }
    g_bBtn_mouse_left = SV_FALSE;
    g_af32Mouse_offset[0] = 0;
    g_af32Mouse_offset[1] = 0;
}
return;
}

static SV_VOID InnerSV_CloseInputDev(SV_VOID) {
if (g_s32KeyBoadFd != -1) {
    close(g_s32KeyBoadFd);
    g_s32KeyBoadFd = -1;
}
if (g_s32MouseFd != -1) {
    close(g_s32MouseFd);
    g_s32MouseFd = -1;
}
}

static SV_S32 InnerSV_KeyBoardEventProcessFb(SV_VOID) {
SV_U16 u16Code = g_stInevent.code;
switch (u16Code) {
    case KEY_LEFT:
        return InnerSV_ENUM_KEY_LEFT_E;
        break;
    case KEY_RIGHT:
        return InnerSV_ENUM_KEY_RIGHT_E;
        break;
    case KEY_UP:
        return InnerSV_ENUM_KEY_UP_E;
        break;
    case KEY_DOWN:
        return InnerSV_ENUM_KEY_DOWN_E;
        break;
    case KEY_TAB:
        return InnerSV_ENUM_KEY_QUARD_E;
        break;
    case KEY_F12:
        return InnerSV_ENUM_KEY_SCAN_E;
        break;
    case BTN_LEFT:
        g_bBtn_mouse_left = g_stInevent.value;
        return InnerSV_ENUM_KEY_NONE_E;
        break;
    default:
        return InnerSV_ENUM_KEY_NONE_E;
}
}

static SV_S32 InnerSV_MouseEventProcessFb(SV_VOID) {
SV_U16 u16Code = g_stInevent.code;
switch (u16Code) {
    case REL_WHEEL:
        return (g_stInevent.value == 1?InnerSV_ENUM_M_SCROLL_UP_E:InnerSV_ENUM_M_SCROLL_DOWN_E);
        break;
    case REL_X:
        if (g_bBtn_mouse_left) {
        if ((g_stInevent.value < 10) || (g_stInevent.value > -10))
        g_af32Mouse_offset[0] = 2 * g_stInevent.value;
        else
        g_af32Mouse_offset[0] = 0;
        g_af32Mouse_offset[1] = 0;

        return InnerSV_ENUM_M_MOVE_E;
        }
        else
        return InnerSV_ENUM_KEY_NONE_E;
        break;
    case REL_Y:
        if (g_bBtn_mouse_left) {
        if ((g_stInevent.value < 10) || (g_stInevent.value > -10))
            g_af32Mouse_offset[1] = -2 * g_stInevent.value;
        else
            g_af32Mouse_offset[1] = 0;
        g_af32Mouse_offset[0] = 0;
        return InnerSV_ENUM_M_MOVE_E;
        }
        else
        return InnerSV_ENUM_KEY_NONE_E;
        break;
    default:
        return InnerSV_ENUM_KEY_NONE_E;
        break;
}
}
#else
static SV_S32 InnerSV_KeyBoardEventProcessX11(const XEvent &report) {
SV_U16 u16Xcode = XLookupKeysym(&report.xkey, 0);
switch (u16Xcode) {
    case XK_Left:
        return InnerSV_ENUM_KEY_LEFT_E;
        break;
    case XK_Right:
        return InnerSV_ENUM_KEY_RIGHT_E;
        break;
    case XK_Up:
        return InnerSV_ENUM_KEY_UP_E;
        break;
    case XK_Down:
        return InnerSV_ENUM_KEY_DOWN_E;
        break;
    case XK_Tab:
        return InnerSV_ENUM_KEY_QUARD_E;
        break;
    case XK_F12:
        return InnerSV_ENUM_KEY_SCAN_E;
        break;
    default:
        return InnerSV_ENUM_KEY_NONE_E;
}
}
static SV_S32 InnerSV_MouseEventProcessX11(const XEvent &report) {
switch (report.type) {
    case MotionNotify:      // Mouse moving
        if (report.xmotion.state & Button1Mask) {
        g_af32Mouse_offset[0] = report.xmotion.x - g_af32Mouse_pos[0];
        g_af32Mouse_offset[1] = g_af32Mouse_pos[1] - report.xmotion.y;
        g_af32Mouse_pos[0] = report.xmotion.x;
        g_af32Mouse_pos[1] = report.xmotion.y;
        return InnerSV_ENUM_M_MOVE_E;
        }
        break;
    case ButtonPress:       //Mouse scrolling
        if (report.xbutton.button == Button1) {         // left clock
        g_af32Mouse_pos[0] = report.xmotion.x;
        g_af32Mouse_pos[1] = report.xmotion.y;
        }
        if (report.xbutton.button == Button4) {         // scrolling up
        return InnerSV_ENUM_M_SCROLL_UP_E;
        }
        if (report.xbutton.button == Button5) {         // scrolling down
        return InnerSV_ENUM_M_SCROLL_DOWN_E;
        }
        break;
}
}
#endif

}  // namespace display
}  // namespace svrender
}  // namespace sv_avm
}  // namespace sm

