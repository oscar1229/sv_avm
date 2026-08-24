/*
 * vehicle.hpp
 *
 */

#ifndef SV_svm_SVRENDER_VEHICLE_VEHICLE_HPP_
#define SV_svm_SVRENDER_VEHICLE_VEHICLE_HPP_
#pragma once
#include<vector>

#include<glog/logging.h>
#include "common/svtype.hpp"//POD数据自定义及SV公共数据结构
#include "src/svrender/common/glm/glm.hpp"
#include "src/svrender/common/shader/glshader.hpp"

namespace sm {
namespace sv_avm {
namespace svrender {
namespace mvp{
 class InnerSV_MvCalss;
}
namespace vehicle {
namespace  vehicleloder {
  class InnerSV_VehicleLoader;
}
//@brief 有关3D车模加载及映射的类
//@remarks 该类不可复制及移动
//         使用实例如下:
//         display::InnerSV_CreateDisplay();
//         SV_SIZE_S stSize=display::InnerSV_GetDisplayFrameSize();
//         mvp::InnerSV_MvCalss stMvClass;
//         stMvClass.Initialized();
//         vehicle::InnerSV_VehicleRenderClass stVehicleRenderClass(&stMvClass);
//         stVehicleRenderClass.Init(s8DaeFileName,vehicleSize，0.8f);
//         while(!bExit) {
//             display::InnerSV_DisplayClear();
//             stVehicleRenderClass.Render(mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_3D,{{0,0},{1080,1080}});
//             stVehicleRenderClass.Render(mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_2D,{{1080,0},{840,1080}});
//             display::InnerSV_DisplaySwap();
//             usleep(20000);
//         }
//         display::InnerSV_DeleteDisplay();
class InnerSV_VehicleRenderClass {
public:
  explicit InnerSV_VehicleRenderClass(mvp::InnerSV_MvCalss* pclMvClass):gpclMvClass(pclMvClass),
    pclVehicleLoder(NULL){
    DLOG(INFO) <<__FUNCTION__;
    DLOG(INFO) << "Before Create This Objects,the pclMvClass should be Init First";
    f32Translucency=1.0;
  }
  //@brief 初始化对象
  //@param in s8DaeFile dae车模文件全路径名称
  //       in stVehicleSize 车辆尺寸参数
  //       in f32Transluce车模映射透明度可设置为0.5-1.0,0.5为半透明，1.0为不透明
  SV_BOOL Init(const std::string& s8DaeFile,const SV_SIZE_S& stVehicleSize,const SV_F32& f32Transluce);
  //@brief opengl 车模绘制
  //@param in s32ViewMode 当前绘制的视图模式
  //       in stViewPoint 当前绘制的视点
  //@remarks s32ViewMode可设置值为mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_3D、
  //         mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_2D
  //         绘制视点即，即当前绘制的视图在画面上的显示范围
  SV_VOID Render(const SV_S32& s32ViewMode,const SV_RECT_S& stViewPoint);
private:
  //车模映射视点变换相关着色器GLSL全局量结构体，CPU可通过相关的量，变更着色器GLSL中的权价格量值
  //以达到实现车模视点变换的目的
  struct SV_VEHICLE_GLSLUNIFORM_S{
    SV_U32 u32MvpUniform;
    SV_U32 u32MvUniform;
    SV_U32 u32MnUniform;
    SV_U32 u32EyeLightUniform;
    SV_U32 u32AmbientLoc;
    SV_U32 u32DiffuseLoc;
    SV_U32 u32translucencelOC;
  };
  //@remarks 显式申明移动构造函数和赋值运算符，禁用当前类的复制，只声明，不做定义
  InnerSV_VehicleRenderClass(const InnerSV_VehicleRenderClass&);
  InnerSV_VehicleRenderClass& operator = (const InnerSV_VehicleRenderClass& m);
  //@brief OpenGL 车模着色器程序加载及初始化
  SV_BOOL ProgramInit(SV_VOID);
  //@breif 车模DAE文件加载及初始化
  SV_BOOL VehicleInit(const std::string& s8DaeFile,const SV_SIZE_S& stVehicleSize);
  //车模映射opengl着色器程序对象及GLSL全局量结构体
  glshader::InnerSV_ProgramClass clProgram;
  vehicleloder:: InnerSV_VehicleLoader* pclVehicleLoder;
  mvp::InnerSV_MvCalss* gpclMvClass;
  SV_VEHICLE_GLSLUNIFORM_S stGLSLUniform;
  SV_F32 af32VehicleScale[3];//车模DAE文件与实际车型参数之间比值
  SV_F32 f32Translucency;//映射车模的半透明读

};


}//end of vehicle
}//end of svrender
}//end of sv_avm
}//end of sm





#endif /* SV_svm_SVRENDER_VEHICLE_VEHICLE_HPP_ */
