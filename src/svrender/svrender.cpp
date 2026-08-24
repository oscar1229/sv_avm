/*
 * svrender.cpp
 *
 */
#include "include/svrender.hpp"

#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<unistd.h>
#include <GLES3/gl3.h>
#include <glog/logging.h>

#include "display/display.hpp"
#include "common/viewtransoform/viewtransform.hpp"
#include "camera/camerarender.hpp"
#include "vehicle/vehiclerender.hpp"
#include "include/sv_avmcommon.hpp"
#include"src/svmparam/svmparam.hpp"
namespace sm {
namespace sv_avm {
namespace svrender {

struct InnerSV_VIEWTHREAD_S
{
  //@brief 360°视角扫描线程ID
  pthread_t gScanViewMode_t;
  SV_BOOL gbScanViewTaskRun ;
  //@brief 360°视角扫描线程ID
  pthread_t gLeftViewMode_t;
  SV_BOOL gbLeftViewTaskRun;
  //@brief 360°视角扫描线程ID
  pthread_t gRightViewMode_t;
  SV_BOOL gbRightViewTaskRun;
};

//@brief 互斥锁，防止多线程多次调用
static pthread_mutex_t stRenderMutex ={0};
//@brief 回调函数指针
static SV_RENDER_CONFIG_S gstConfigs ={NULL,NULL,NULL,NULL,NULL,NULL,0.8,NULL,NULL};

static mvp::InnerSV_MvCalss* gpstMvpClass=NULL;
static vehicle::InnerSV_VehicleRenderClass* gpstVehicleRenderClass=NULL;
static camera::InnerSv_CameraRenderClass* gpstCameraRenderClass=NULL;
static pthread_mutex_t TransformMutex_t = {0};
static SV_S32 gs32DisplayMode = SV_ENUM_VIEW_3D;

//@brief映射线程ID
static pthread_t gRender_t=0;
//@brief 映射线程运行标志位
static SV_BOOL gbRenderTaskRun=SV_FALSE;
//@brief输入事件处理线程ID
static pthread_t gRenderInputEvent_t=0;
//@brief 输入事件处理线程运行标志位
static SV_BOOL gbRenderInputEventTaskRun=SV_FALSE;
SV_S32 gs32EglFbDevIdx = 0;//Egl使用的Fb设备号

//@左右转以及一键旋转360子线程相关结构体
static InnerSV_VIEWTHREAD_S stLRScanViewModeS={0,0,0,0,0,0};

//@brief 清空所有的回调函数
static SV_VOID InnerSV_RenderUnregisterCallBack(SV_VOID);
//@brief 将所有的子模块对象指针清空为NULL
static SV_VOID InnerSV_ClassObjectDeInit(SV_VOID);
//@brief 检查所有的回调函数是否已注册
static SV_BOOL InnerSV_bCheckCallbackValid(SV_VOID);
//@breif 映射线程
static void* InnerSV_Render_thread(void* arg);
//&brif inputevent 处理线程
static void* InnerSV_Render_InputEvent(void* arg);

/***************************************************************************************
***************************************************************************************/
/*******************************************************************************************
 * Macros
 *******************************************************************************************/
#define Timespec_Double(t) ((double)((t)->tv_sec) + (1.e-9 * (double)((t)->tv_nsec)))

#define Timespec_Sub(r, a, b) \
  do { \
    if ((a)->tv_nsec < (b)->tv_nsec) { \
      (r)->tv_nsec = 1000000000 + (a)->tv_nsec - (b)->tv_nsec; \
      (r)->tv_sec = (a)->tv_sec - (b)->tv_sec - 1; \
    } else { \
      (r)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
      (r)->tv_sec  = (a)->tv_sec  - (b)->tv_sec; \
    } \
  } while (0)
double report_fps(void)
{
    static bool first_call = true;
    static unsigned int fps_count = 0;
    static struct timespec t_start = { 0, 0 };
    static struct timespec t_end = { 0, 0 };
    struct timespec dt;
    static double fpsValue = 0;

    if (first_call) {
        if (clock_gettime(CLOCK_REALTIME, &t_start) != 0) {
           LOG(ERROR)<< "clock_gettime(): error " << strerror(errno);
            return fpsValue;
        }
        first_call = false;
    }

    if (fps_count >= 100) {
        if (clock_gettime(CLOCK_REALTIME, &t_end) != 0) {
           LOG(ERROR)<<" clock_gettime(): error " << strerror(errno);
            return fpsValue;
        }
        Timespec_Sub(&dt, &t_end, &t_start);
        fpsValue = (double)fps_count / (double)Timespec_Double(&dt);
        memcpy(&t_start, &t_end, sizeof(timespec));
        fps_count = 0;
    }
    ++fps_count;

    return fpsValue;
}

static SV_VOID InnerSV_GetCamParamVectAndVehicleSize(const SV_S8* s8XmlFileName,SV_SIZE_S *stVehicleSize,
    std::vector<SV_CAMERA_PARAMS_S> *stCameraParamsVector);
#ifdef EGL_USE_X11
SV_BOOL SV_RenderTaskOpen(const SV_RENDER_CONFIG_S& stConfigs) {
#else
SV_BOOL SV_RenderTaskOpen(const SV_RENDER_CONFIG_S& stConfigs,const SV_S32& s32FbDevIdx) {
#endif
  if(pthread_mutex_trylock(&stRenderMutex)!=0) {
    LOG(ERROR)<<"RenderTask Have Opened";
    return SV_FALSE;
  }
  InnerSV_RenderUnregisterCallBack();
  gstConfigs = stConfigs;
  if(SV_FALSE==InnerSV_bCheckCallbackValid()) {
    LOG(ERROR) << "All CallBack Function Ptr Should Setup";
    pthread_mutex_unlock(&stRenderMutex);
    return SV_FALSE;
  }
#ifndef EGL_USE_X11
  gs32EglFbDevIdx = s32FbDevIdx;
#endif
  pthread_create(&gRender_t,0,InnerSV_Render_thread,NULL);
  usleep(1000);
  pthread_create(&gRenderInputEvent_t,0,InnerSV_Render_InputEvent,NULL);
 // usleep(1000);
  return SV_TRUE;
}

static SV_VOID InnerSV_RenderUnregisterCallBack(SV_VOID) {
  memset(&gstConfigs,NULL,sizeof(gstConfigs));
  gstConfigs.f32VehicleTranslucency=0.8;
}

SV_VOID SV_RenderTaskClose(SV_VOID) {

  gbRenderInputEventTaskRun =SV_TRUE;
  gbRenderTaskRun=SV_FALSE;
  pthread_join(gRender_t,NULL);
  pthread_join(gRenderInputEvent_t,NULL);
  InnerSV_RenderUnregisterCallBack();
  pthread_mutex_unlock(&stRenderMutex);
}

SV_BOOL SV_RenderTaskUpdateMesh(SV_VOID) {
  if(0==gRender_t) {
    return SV_FALSE;
  }
  gbRenderInputEventTaskRun =SV_TRUE;
  gbRenderTaskRun=SV_FALSE;
  pthread_join(gRender_t,NULL);
  pthread_join(gRenderInputEvent_t,NULL);
  pthread_create(&gRender_t,0,InnerSV_Render_thread,NULL);//重新开启线程
  usleep(1000);
  pthread_create(&gRenderInputEvent_t,0,InnerSV_Render_InputEvent,NULL);
  usleep(1000);
  return SV_TRUE;
}

static SV_VOID InnerSV_ClassObjectDeInit(SV_VOID) {
  gpstVehicleRenderClass=NULL;
  gpstCameraRenderClass=NULL;
  pthread_mutex_lock(&TransformMutex_t);
  gpstMvpClass=NULL;
  pthread_mutex_unlock(&TransformMutex_t);
}

static SV_BOOL InnerSV_bCheckCallbackValid(SV_VOID) {

  if(0==gstConfigs.f32VehicleTranslucency)
    gstConfigs.f32VehicleTranslucency=1.0;
  return gstConfigs.pCallGetVehicleDae!=NULL &&gstConfigs.pCallGetBowlGridParam!=NULL&&
      gstConfigs.pCallGetFrameS!=NULL&&gstConfigs.pCallPutFrameS!=NULL&&gstConfigs.s8XmlFileName!=NULL&& \
      gstConfigs.pCallGet2DModeFlag!=NULL;
}

static SV_VOID InnerSV_GetCamParamVectAndVehicleSize(const SV_S8* s8XmlFileName,
    SV_SIZE_S *pstVehicleSize,std::vector<SV_CAMERA_PARAMS_S> *pstCameraParamsVector) {
  svmparam::InnerSV_SvmParamClass stParam;
  SV_BOOL bNeed=SV_FALSE;
  SV_S32 s32Ret=stParam.InnerSV_s32ReadFromXml( s8XmlFileName,bNeed);
  if(svmparam::ISV_ENUM_SUCCEED!=s32Ret) {
    stParam.InnerSV_Init();
  }
  for(SV_S32 i=0;i<4;i++) {

    SV_CAMERA_PARAMS_S stCamParams = stParam.InnerSV_stGetCameraParamsEachChannl(i);
    pstCameraParamsVector->push_back(stCamParams);
  }
  *pstVehicleSize = stParam.InnerSV_stGetVehicleSize();
  return;
}
static SV_S32 s32RenderVehicle=0;
static SV_VOID InnerSV_Render3D(camera::InnerSv_CameraRenderClass &stCameraRenderTmp,
    vehicle::InnerSV_VehicleRenderClass &stVehicleRenderTmp,
    const SV_RECT_S& st2DRect,const SV_RECT_S& st3DRect) {
  pthread_mutex_lock(&TransformMutex_t);
  std::vector<SV_IMAGE_S> stImgVecTmp =gstConfigs.pCallGetFrameS();//获取帧图像
  stCameraRenderTmp.GenCameraTextrue(stImgVecTmp);
  gstConfigs.pCallPutFrameS(stImgVecTmp);//释放帧图像
  //映射3D
  stCameraRenderTmp.Render(mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_3D,st3DRect);
  stCameraRenderTmp.Render(mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_2D,st2DRect);
    stVehicleRenderTmp.Render(mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_2D,st2DRect);
    stVehicleRenderTmp.Render(mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_3D,st3DRect);
  pthread_mutex_unlock(&TransformMutex_t);
}

static SV_VOID InnerSV_Render2D(camera::InnerSv_CameraRenderClass &stCameraRenderTmp,
    vehicle::InnerSV_VehicleRenderClass &stVehicleRenderTmp,
    const SV_RECT_S& st2DRect,const SV_RECT_S& st3DRect) {
  std::vector<SV_IMAGE_S> stImgVecTmp =gstConfigs.pCallGetFrameS();//获取帧图像
  stCameraRenderTmp.GenCameraTextrue(stImgVecTmp);
  gstConfigs.pCallPutFrameS(stImgVecTmp);//释放帧图像
  //映射俯视视角
  stCameraRenderTmp.Render(mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_2D,st2DRect);
  stVehicleRenderTmp.Render(mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_2D,st2DRect);
  //映射单视图
  stCameraRenderTmp.RenderSingleChl(std::max(gs32DisplayMode-SV_ENUM_VIEW_DUAL_LEFT,0),st3DRect);
}

static SV_VOID InnerSV_RenderQuad(camera::InnerSv_CameraRenderClass &stCameraRenderTmp, \
    const SV_RECT_S stQuadRect[4]) {
  std::vector<SV_IMAGE_S> stImgVecTmp =gstConfigs.pCallGetFrameS();
  stCameraRenderTmp.GenCameraTextrue(stImgVecTmp);
  gstConfigs.pCallPutFrameS(stImgVecTmp);//释放帧图像
  SV_S32 s32CameraNum =4;
  s32CameraNum = std::min(static_cast<SV_S32>(stImgVecTmp.size()),s32CameraNum);
  for(SV_S32 i=0;i<s32CameraNum;++i) {
    stCameraRenderTmp.RenderSingleChl(i,stQuadRect[i]);
  }
  gstConfigs.pCallPutFrameS(stImgVecTmp);
}


static SV_VOID InnerSV_RenderLoop(camera::InnerSv_CameraRenderClass &stCameraRenderTmp,
    vehicle::InnerSV_VehicleRenderClass &stVehicleRenderTmp,
    const SV_RECT_S& st2DRect,const SV_RECT_S& st3DRect,
    const SV_RECT_S& stCameraSingChlRect,
    const SV_RECT_S stQuadRect[4]) {
  gbRenderTaskRun=SV_TRUE;
  while(gbRenderTaskRun) {
     display::InnerSV_DisplayClear();
     switch(gs32DisplayMode) {
     case SV_ENUM_VIEW_3D:
       InnerSV_Render3D(stCameraRenderTmp,stVehicleRenderTmp,st2DRect,st3DRect);
       break;
     case SV_ENUM_VIEW_QUAD:
       InnerSV_RenderQuad(stCameraRenderTmp, stQuadRect);
       break;
     default:
       InnerSV_Render2D(stCameraRenderTmp,stVehicleRenderTmp,st2DRect,stCameraSingChlRect);
       break;
     }
     DLOG(INFO)<<"fps:"<<report_fps();
     display::InnerSV_DisplaySwap();
   }
}

static void* InnerSV_Render_thread(void* arg) {
#ifdef EGL_USE_X11
  display::InnerSV_CreateDisplay(gstConfigs.s8KeyBoardDevName,gstConfigs.s8MouseDevName);
#else
  display::InnerSV_CreateDisplay(gs32EglFbDevIdx,gstConfigs.s8KeyBoardDevName,gstConfigs.s8MouseDevName);
#endif
  SV_SIZE_S stSize=display::InnerSV_GetDisplayFrameSize();
  SV_RECT_S st2DRect={{0,0},{stSize.s32Width*7/16,stSize.s32Height}};
  SV_RECT_S st3DRect={{st2DRect.stRectSize.s32Width,0},{stSize.s32Width-st2DRect.stRectSize.s32Width,stSize.s32Height}};
  SV_S32 s32CameraSingleChlH= (stSize.s32Width-st2DRect.stRectSize.s32Width)*9/16;
  SV_RECT_S stCameraSingChlRect={{st2DRect.stRectSize.s32Width,(stSize.s32Height-s32CameraSingleChlH)*0.5}, \
      {stSize.s32Width-st2DRect.stRectSize.s32Width,s32CameraSingleChlH}};//摄像头单视图视窗
  SV_RECT_S stQuadRect[4]={{{0,0},{stSize.s32Width*0.5,stSize.s32Height*0.5}}, \
      {{stSize.s32Width*0.5,0},{stSize.s32Width*0.5,stSize.s32Height*0.5}}, \
      {{0,stSize.s32Height*0.5},{stSize.s32Width*0.5,stSize.s32Height*0.5}} , \
      {{stSize.s32Width*0.5,stSize.s32Height*0.5},{stSize.s32Width*0.5,stSize.s32Height*0.5}}};
  //创建个子对象
  SV_SIZE_S stVehicleSize;
  std::vector<SV_CAMERA_PARAMS_S> stCameraParamsVector;
  InnerSV_GetCamParamVectAndVehicleSize(gstConfigs.s8XmlFileName,&stVehicleSize,&stCameraParamsVector);//获取摄像头参数
  mvp::InnerSV_MvCalss stMvTmp;
  stMvTmp.Initialized();
  vehicle::InnerSV_VehicleRenderClass stVehicleRenderTmp(&stMvTmp);
  stVehicleRenderTmp.Init(gstConfigs.pCallGetVehicleDae(),stVehicleSize,gstConfigs.f32VehicleTranslucency);
  camera::InnerSv_CameraRenderClass stCameraRenderTmp(&stMvTmp);
  stCameraRenderTmp.Init(stCameraParamsVector,stVehicleSize,gstConfigs.pCallGetBowlGridParam());
  gpstMvpClass = &stMvTmp;
  gpstCameraRenderClass = &stCameraRenderTmp;
  gpstVehicleRenderClass =&stVehicleRenderTmp;
  InnerSV_RenderLoop(stCameraRenderTmp,stVehicleRenderTmp,st2DRect,st3DRect,stCameraSingChlRect,stQuadRect);
  display::InnerSV_DeleteDisplay(gs32EglFbDevIdx);
  InnerSV_ClassObjectDeInit();
  pthread_exit(NULL);
}//end of Render_thread

static void* InnerSV_Render_InputEvent(void* arg) {
  LOG(INFO)<<"Render InputEvent process";
  gbRenderInputEventTaskRun=SV_FALSE;
  SV_RENDER_VIRTULVIEW_PARAM_S stVirtualParam={0};
  while(!gbRenderInputEventTaskRun) {
   if(display::InnerSV_DisplayGetEventNum()){
     LOG(INFO)<<"Render InputEvent process";
     SV_S32 s32Ret = display::InnerSV_DisplayNextEvent();
     SV_BOOL b2DFlag =gstConfigs.pCallGet2DModeFlag();
     switch(s32Ret) {
       case display::InnerSV_ENUM_KEY_LEFT_E:
         if(b2DFlag)
          SV_RenderTransform(sm::sv_avm::svrender::SV_ENUM_VIEW_DUAL_LEFT,stVirtualParam);
         else
          SV_RenderClassicalView(sm::sv_avm::svrender::SV_ENUM_CLASSIC_3DVIEW_TLEFT);
         break;
       case display::InnerSV_ENUM_KEY_RIGHT_E:
         if(b2DFlag)
          SV_RenderTransform(sm::sv_avm::svrender::SV_ENUM_VIEW_DUAL_RIGHT,stVirtualParam);
         else
          SV_RenderClassicalView(sm::sv_avm::svrender::SV_ENUM_CLASSIC_3DVIEW_TRIGHT);
         break;
       case display::InnerSV_ENUM_KEY_UP_E:
         if(b2DFlag)
            SV_RenderTransform(sm::sv_avm::svrender::SV_ENUM_VIEW_DUAL_FRONT,stVirtualParam);
           else
            SV_RenderClassicalView(sm::sv_avm::svrender::SV_ENUM_CLASSIC_3DVIEW_FORMAT);
         break;
       case display::InnerSV_ENUM_KEY_DOWN_E:
         if(b2DFlag)
            SV_RenderTransform(sm::sv_avm::svrender::SV_ENUM_VIEW_DUAL_BACK,stVirtualParam);
           else
            SV_RenderClassicalView(sm::sv_avm::svrender::SV_ENUM_CLASSIC_3DVIEW_BACKWARD);
         break;
       case display::InnerSV_ENUM_KEY_QUARD_E:
         sm::sv_avm::svrender::SV_RenderTransform(sm::sv_avm::svrender::SV_ENUM_VIEW_QUAD,stVirtualParam);
         break;
       case display::InnerSV_ENUM_KEY_SCAN_E:
         if(!b2DFlag) {
           sm::sv_avm::svrender::SV_RenderClassicalView( sm::sv_avm::svrender::SV_ENUM_CLASSIC_3DVIEW_SCAN);
         }
         break;
     }
   }
  }
  LOG(INFO)<<"InnerSV_Render_InputEvent Exit";
}

SV_BOOL SV_RenderTransform(const SV_S32& s32DisplayMode,const SV_RENDER_VIRTULVIEW_PARAM_S& stVirtualParam) {
  SV_S32 s32DisplayModeMax = SV_ENUM_VIEW_QUAD, s32DisplayModeMin = SV_ENUM_VIEW_3D;
  gs32DisplayMode = std::min(s32DisplayMode,s32DisplayModeMax);
  gs32DisplayMode = std::max(s32DisplayMode,s32DisplayModeMin);
  if(gs32DisplayMode == SV_ENUM_VIEW_3D) {
    glm::vec3 stPos(stVirtualParam.stCamPosition.f32X,stVirtualParam.stCamPosition.f32Y,stVirtualParam.stCamPosition.f32Z);
    glm::vec2 stRot(stVirtualParam.stCamRotate.f32X,stVirtualParam.stCamRotate.f32Y);
    if(NULL == gpstMvpClass)
      return SV_FALSE;
    pthread_mutex_lock(&TransformMutex_t);
    gpstMvpClass->SetVirtualCameraParams(stPos,stRot);
    pthread_mutex_unlock(&TransformMutex_t);
    return SV_TRUE;
  }
  return SV_TRUE;
}

SV_BOOL SV_GetRenderVirtualViewParams(SV_RENDER_VIRTULVIEW_PARAM_S* pstVirtualParam) {
    glm::vec3 stPos;
    glm::vec2 stRot;
    if(NULL == gpstMvpClass)
      return SV_FALSE;
    gpstMvpClass->GetVirtualCameraParams(&stPos,&stRot);
    pstVirtualParam->stCamPosition.f32X = stPos[0];
    pstVirtualParam->stCamPosition.f32Y = stPos[1];
    pstVirtualParam->stCamPosition.f32Z = stPos[2];
    pstVirtualParam->stCamRotate.f32X= stRot[0];
    pstVirtualParam->stCamRotate.f32Y=stRot[1];
    return SV_TRUE;
  }

static SV_VOID InnerSV_DeletaScanViewThread(SV_VOID) {
  if(SV_TRUE==stLRScanViewModeS.gbLeftViewTaskRun) {
    stLRScanViewModeS.gbLeftViewTaskRun=SV_FALSE;
    pthread_join(stLRScanViewModeS.gLeftViewMode_t,NULL);
  }
  if(SV_TRUE==stLRScanViewModeS.gbRightViewTaskRun) {
    stLRScanViewModeS.gbRightViewTaskRun=SV_FALSE;
    pthread_join(stLRScanViewModeS.gRightViewMode_t,NULL);
  }
  if(SV_TRUE==stLRScanViewModeS.gbScanViewTaskRun) {
    stLRScanViewModeS.gbScanViewTaskRun=SV_FALSE;
    pthread_join(stLRScanViewModeS.gScanViewMode_t,NULL);
  }
}

static SV_VOID* InnerSV_TurnLeftDisplay(SV_VOID* arg) {
  stLRScanViewModeS.gbLeftViewTaskRun=SV_TRUE;
  svrender::SV_RENDER_VIRTULVIEW_PARAM_S stvirViewParam = {{0,0.1,-3.45},{-1.57,-0.75}};
  svrender::SV_RENDER_VIRTULVIEW_PARAM_S stCur;
  SV_S32 s32MaxNum = (2.09-1.57)*20+1;
  pthread_detach(pthread_self());
  for(SV_S32 i=0;i<s32MaxNum;++i) {
    if(SV_FALSE==stLRScanViewModeS.gbLeftViewTaskRun)
      break;
    stvirViewParam.stCamRotate.f32X-=0.05;
    svrender::SV_RenderTransform(svrender::SV_ENUM_VIEW_3D,stvirViewParam);
    usleep(50000);
  }
  stLRScanViewModeS.gbLeftViewTaskRun=SV_FALSE;
  pthread_exit(NULL);
}

static SV_VOID* InnerSV_TurnRightDisplay(SV_VOID* arg) {
  stLRScanViewModeS.gbRightViewTaskRun=SV_TRUE;
  svrender::SV_RENDER_VIRTULVIEW_PARAM_S stvirViewParam = {{0,0.1,-3.45},{-1.57,-0.75}};
   SV_S32 s32MaxNum = (1.57-1.04)*20+1;
   pthread_detach(pthread_self());
   for(SV_S32 i=0;i<s32MaxNum;++i) {
     if(SV_FALSE==stLRScanViewModeS.gbRightViewTaskRun)
       break;
     stvirViewParam.stCamRotate.f32X+=0.05;
     svrender::SV_RenderTransform(svrender::SV_ENUM_VIEW_3D,stvirViewParam);
     usleep(50000);
   }
   stLRScanViewModeS.gbRightViewTaskRun=SV_FALSE;
   pthread_exit(NULL);
}

static SV_VOID InnerSV_BackwardDisplay(SV_VOID) {
  InnerSV_DeletaScanViewThread();
  svrender::SV_RENDER_VIRTULVIEW_PARAM_S stvirViewParam = {{0,1.0,-3.45},{-1.57,-0.75}};
  svrender::SV_RenderTransform(svrender::SV_ENUM_VIEW_3D,stvirViewParam);
}

static SV_VOID InnerSV_ForwardDisplay(SV_VOID) {
  InnerSV_DeletaScanViewThread();
  svrender::SV_RENDER_VIRTULVIEW_PARAM_S stvirViewParam = {{0,0.1,-3.45},{-1.57,-0.75}};
  svrender::SV_RenderTransform(svrender::SV_ENUM_VIEW_3D,stvirViewParam);
}
static SV_VOID* gbScanViewTask_thread(void* arg) {
  stLRScanViewModeS.gbScanViewTaskRun = SV_TRUE;
  svrender::SV_RENDER_VIRTULVIEW_PARAM_S stvirViewParam = {{0,0.1,-3.45},{-1.57,-0.75}};
  pthread_detach(pthread_self());
  for(SV_S32 i=0;i<125;++i) {
    if(SV_FALSE==stLRScanViewModeS.gbScanViewTaskRun)
      goto _Exit;
    stvirViewParam.stCamRotate.f32X+=0.05;
    svrender::SV_RenderTransform(svrender::SV_ENUM_VIEW_3D,stvirViewParam);
    usleep(50000);
  }
  InnerSV_ForwardDisplay();
  _Exit:
  stLRScanViewModeS.gbScanViewTaskRun=SV_FALSE;
  pthread_exit(NULL);
}

SV_VOID SV_RenderClassicalView(const SV_S32& s32ClassicalView) {
  switch(s32ClassicalView) {
  case SV_ENUM_CLASSIC_3DVIEW_TLEFT: {
    if(SV_TRUE==stLRScanViewModeS.gbLeftViewTaskRun)
      return;
    InnerSV_DeletaScanViewThread();
    pthread_create(&stLRScanViewModeS.gLeftViewMode_t,0,InnerSV_TurnLeftDisplay,0);
    break;
  }
  case SV_ENUM_CLASSIC_3DVIEW_TRIGHT: {
    if(SV_TRUE==stLRScanViewModeS.gbRightViewTaskRun)
      return;
    InnerSV_DeletaScanViewThread();
    pthread_create(&stLRScanViewModeS.gRightViewMode_t,0,InnerSV_TurnRightDisplay,0);
    break;
  }
  default:
  case SV_ENUM_CLASSIC_3DVIEW_FORMAT: {
    InnerSV_ForwardDisplay();
    break;
  }
  case SV_ENUM_CLASSIC_3DVIEW_BACKWARD: {
    InnerSV_BackwardDisplay();
    break;
  }
  case SV_ENUM_CLASSIC_3DVIEW_SCAN: {
    if(SV_TRUE==stLRScanViewModeS.gbScanViewTaskRun)
      return;
    InnerSV_DeletaScanViewThread();
    pthread_create(&stLRScanViewModeS.gScanViewMode_t,0,gbScanViewTask_thread,0);
  }
  }
}



}
}
}
