/*
 * svrender.hpp
 *
 */

#ifndef SV_AVM_SVRENDER_HPP_
#define SV_AVM_SVRENDER_HPP_
#pragma once
#include <pthread.h>
#include <string>
#include <vector>

#include"common/svtype.hpp"//POD数据自定义及SV公共数据结构
namespace sm {
namespace sv_avm {
struct SV_CAMERA_PARAMS_S;
struct SV_BOWL_GRID_PARAM_S;
namespace svrender {
typedef std::string (*CALLBACK_GETVEHICLEDAE)(void);//获取车模文件全路径名称回调函数类型
typedef SV_BOWL_GRID_PARAM_S (*CALLBACK_GETBOWLGRID)();//获取碗面网格参数
typedef std::vector<SV_IMAGE_S> (*CALLBACK_GETFRAMEVECT)(void);//获取摄像头帧向量回调函数类型
typedef void (*CALLBACK_PUTFRAMEVECT)( std::vector<SV_IMAGE_S>&);//释放摄像头帧向量回调函数类型
typedef SV_BOOL (*CALLBACK_GET_RENDER2D_FLAG)(SV_VOID);//回调，是否为2D显示模式

enum {
  SV_ENUM_VIEW_3D=0,// 3D显示模式
  SV_ENUM_VIEW_DUAL_LEFT,//俯视+左视
  SV_ENUM_VIEW_DUAL_RIGHT,//俯视+右
  SV_ENUM_VIEW_DUAL_FRONT,//俯视+前
  SV_ENUM_VIEW_DUAL_BACK,//俯视+后
  SV_ENUM_VIEW_QUAD,//四分割显示
  SV_ENUM_VIEW_BUTT
};

enum {
  SV_ENUM_CLASSIC_3DVIEW_TLEFT=0,//左转弯视角
  SV_ENUM_CLASSIC_3DVIEW_TRIGHT,//右转弯视角
  SV_ENUM_CLASSIC_3DVIEW_FORMAT,//前世
  SV_ENUM_CLASSIC_3DVIEW_BACKWARD,//倒车
  SV_ENUM_CLASSIC_3DVIEW_SCAN,
};

//开启Render任务需要的回调函数
struct SV_RENDER_CONFIG_S {
  CALLBACK_GETVEHICLEDAE pCallGetVehicleDae;
  CALLBACK_GETBOWLGRID  pCallGetBowlGridParam;
  CALLBACK_GETFRAMEVECT pCallGetFrameS;
  CALLBACK_PUTFRAMEVECT pCallPutFrameS;
  CALLBACK_GET_RENDER2D_FLAG pCallGet2DModeFlag;
  SV_S8* s8XmlFileName; //标定结果xml文件路径
  SV_F32 f32VehicleTranslucency;//车模半透明度0.5-1.0
  SV_S8* s8KeyBoardDevName; //键盘设备全路径名称
  SV_S8* s8MouseDevName; //鼠标设备全路径名称
};

//@brief 虚拟视点参数
struct SV_RENDER_VIRTULVIEW_PARAM_S {
  SV_POINT3F32_S stCamPosition;
  SV_POINT2F32_S stCamRotate;
};
//@brief 开启全景映射任务
//@param in stCallBack 回调函数结构体
//@return SV_FALSE/SV_BOOL
//@remarks 函数内部检查回调函数是否已注册，同时创建映射线程
//         RenderTaskOpen 需与RenderTaskClose成对出现
//         RenderTaskOpen只能在主线程中调用
#ifdef EGL_USE_X11
SV_BOOL SV_RenderTaskOpen(const SV_RENDER_CONFIG_S& stConfigs);
#else
SV_BOOL SV_RenderTaskOpen(const SV_RENDER_CONFIG_S& stConfigs,const SV_S32& s32FbDevIdx);
#endif
//@brief 关闭全景映射任务
//@remarks 销毁映射线程，并去注册回调函数
//         只能在主线程中调用
SV_VOID SV_RenderTaskClose(SV_VOID);
//@brief 当变更摄像头mesh时，刷新用于绘制的摄像头网格或车模网格
//@return SV_FALSE/SV_TRUE
//@remaks 可在每次标定成功时或更换车模时调用，用于显示结果
SV_BOOL SV_RenderTaskUpdateMesh(SV_VOID);
//@brief Render视点变换
//@param in S32DisplayMode显示模式
//       in stVirtualParam虚拟视点参数
//@return 任务未创建，返回SV_FALSE;
//@remarks   可在其他线程中作为回调函数调用，变更当前的3D显示视角或显示模式
//           S32DisplayMode 可选值为SV_ENUM_VIEW_3D、SV_ENUM_VIEW_DUAL_LEFT-SV_ENUM_VIEW_DUAL_BACK\SV_ENUM_VIEW_QUAD
//           SV_ENUM_VIEW_3D显示模式几种视角推荐参数值
//           左转 stCamRotate = {2.09,-0.75} stCamPosition采用默认值{0,0.1,-3.45}
//           右转 stCamRot={1.05,-0.75} stCamPosition采用默认值{0,0.1，-3.45}
//           常态显示 stCamRot采用默认值，即{1.75，,0.75} stCamPosition采用默认值
//
SV_BOOL SV_RenderTransform(const SV_S32& S32DisplayMode,const SV_RENDER_VIRTULVIEW_PARAM_S& stVirtualParam);
//@brief 获取当前的虚拟视点参数
//@param out pstVirtualParam 虚拟视点参数
//@return 任务未创建，返回SV_FALSE;
SV_BOOL SV_GetRenderVirtualViewParams(SV_RENDER_VIRTULVIEW_PARAM_S* pstVirtualParam);

//@brief 经典视角
//@param in s32ClassicalView 经典视角模式
//@remarks s32ClassicalView可选值为SV_ENUM_CLASSIC_3DVIEW_TLEFT->SV_ENUM_CLASSIC_3DVIEW_SCAN
SV_VOID SV_RenderClassicalView(const SV_S32& s32ClassicalView);


}
}
}
#endif /* SV_AVM_SVRENDER_HPP_ */
