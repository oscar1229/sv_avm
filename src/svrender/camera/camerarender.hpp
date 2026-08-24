/*
 * camerarender.hpp
 *
 */

#ifndef SV_AVM_SVRENDER_CAMERA_CAMERARENDER_HPP_
#define SV_AVM__SVRENDER_CAMERA_CAMERARENDER_HPP_
#pragma once
#include<vector>
#include"common/svtype.hpp"//POD数据自定义及SV公共数据结构

#include "include/sv_avmcommon.hpp"
#include "src/svrender/common/glm/glm.hpp"
#include "src/svrender/common/shader/glshader.hpp"

namespace sm {
namespace sv_avm {
namespace svrender {
namespace mvp{
 class InnerSV_MvCalss;
}
namespace camera {
//@brief 有关摄像头OprnGL视图绘制的类
//@remarks 当前类不可移动及复制
//         使用示例如下：
//         display::InnerSV_CreateDisplay();
//         SV_SIZE_S stSize=display::InnerSV_GetDisplayFrameSize();
//         mvp::InnerSV_MvCalss stMvClass;
//         stMvClass.Initialized();
//         InnerSv_CameraRenderClass stCameraRenderClass(&stMvClass);
//         stCameraRenderClass.Init(stCameraParamsVector,stVehicleSize,stGridParam);
//         while(!bExit) {
//             display::InnerSV_DisplayClear();
//             stVehicleRenderClass.Render(mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_3D,{{0,0},{1080,1080}});
//             stVehicleRenderClass.Render(mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_2D,{{1080,0},{840,1080}});
//             display::InnerSV_DisplaySwap();
//             usleep(20000);
//         }
//         display::InnerSV_DeleteDisplay();
class InnerSv_CameraRenderClass {
public:
  explicit InnerSv_CameraRenderClass(mvp::InnerSV_MvCalss* pclMvClass):gpclMvClass(pclMvClass),u32MvpUniform(0),u32MvpUniformOES(0),bUseOES(SV_FALSE){}

  SV_BOOL Init(const std::vector<SV_CAMERA_PARAMS_S> &stCameraParamsVector, \
      const SV_SIZE_S& stVehicleSize,const SV_BOWL_GRID_PARAM_S& stGridParam);
  SV_VOID GenCameraTextrue(const std::vector<SV_IMAGE_S> &img);
  //SV_VOID Render(const std::vector<SV_IMAGE_S> &stImageVect,const SV_S32& s32ViewMode,const SV_RECT_S& stViewPoint);
  SV_VOID Render(const SV_S32& s32ViewMode,const SV_RECT_S& stViewPoint2D);
  //@brief 映射单视图图像
  //@param in stImageVect摄像头视图
  //       in s32Chnl摄像头通道号
  //       in stViewPoint 视点，即当前视图的显示范围
  SV_VOID RenderSingleChl(const SV_S32 &s32Chnl,const SV_RECT_S& stViewPoint);
private:
  //@remarks 显式申明移动构造函数和赋值运算符，禁用当前类的复制，只声明，不做定义
  InnerSv_CameraRenderClass(const InnerSv_CameraRenderClass&);
  InnerSv_CameraRenderClass& operator = (const InnerSv_CameraRenderClass& m);

  const glm::mat4 kstCameraOriginMvp=glm::mat4(glm::vec4(1,0,0,0),glm::vec4(0,1,0,0),glm::vec4(0,0,1,0),glm::vec4(0,0,0,1));//摄像头单视图映射Mvp矩阵

  SV_BOOL ProgramInit(SV_VOID);
  SV_VOID CameraTextInit(const std::vector<SV_CAMERA_PARAMS_S> &stCameraParamsVector, \
      const SV_SIZE_S& stVehicleSize,const SV_BOWL_GRID_PARAM_S& stGridParam);
  //零拷贝(NV12 dma_buf)路径:把img的dma_fd导入为EGLImage并绑定到external纹理
  //@return 成功返回SV_TRUE;若任一通道不具备dma_fd则返回SV_FALSE(调用方回退普通路径)
  SV_BOOL GenCameraTextrueZeroCopy(const std::vector<SV_IMAGE_S> &img);
  glshader::InnerSV_ProgramClass clProgram;
  glshader::InnerSV_ProgramClass clProgramOES;//零拷贝路径:采样external纹理的着色器程序
  mvp::InnerSV_MvCalss* gpclMvClass;
  SV_U32 u32MvpUniform;
  SV_U32 u32MvpUniformOES;//clProgramOES的mvp uniform位置
  SV_BOOL bUseOES;//本帧纹理是否为external(零拷贝)纹理,决定Render选用哪个program
  std::vector<SV_U32> u32VAOVect;//
  std::vector<SV_S32> s32MeshSize;//各通道摄像头mesh的Size
  std::vector<SV_U32>u32TexObjVect;//纹理对象向量
  std::vector<SV_U32>u32TexObjOESVect;//external(OES)纹理对象向量,用于零拷贝路径
  std::vector<SV_S32>s32EglImageFdVect;//各通道当前已导入EGLImage对应的dma_fd,用于判断是否需重建
  std::vector<SV_S32>s32EglImageTypeVect;//各通道当前EGLImage图像格式
  std::vector<SV_SIZE_S>stEglImageSizeVect;//各通道当前EGLImage尺寸
  std::vector<SV_U32>u32EglImageStride0Vect;//各通道当前EGLImage plane0 pitch
  std::vector<SV_U32>u32EglImageStride1Vect;//各通道当前EGLImage plane1 pitch
  std::vector<SV_U32>u32EglImageOffset0Vect;//各通道当前EGLImage plane0 offset
  std::vector<SV_U32>u32EglImageOffset1Vect;//各通道当前EGLImage plane1 offset
  std::vector<void*>pEglImageVect;//各通道当前的EGLImageKHR句柄(void*存储,避免头文件引入EGL)
  std::vector<SV_SIZE_S> stTexAllocSize;//各纹理已分配的存储尺寸,用于决定glTexImage2D(重分配)还是glTexSubImage2D(仅更新)
  std::vector<SV_U32> u32CameraOrigiVaoVect;//各摄像头原始视图的顶点缓冲对象，其中，左右前为原像，后视为水平镜像
};

}//end camera
}//end svrender
}//end sv_avm
}//end sm



#endif /* SV_AVM__SVRENDER_CAMERA_CAMERARENDER_HPP_ */
