/*
 * viewtransform.hpp
 *
 *此文件声明了有关视点变换的类及其成员函数
 *
 */

#ifndef SV_SVM_SVRENDER_VIEWTRANSOFORM_VIEWTRANSFORM_HPP_
#define SV_SVM_SVRENDER_VIEWTRANSOFORM_VIEWTRANSFORM_HPP_
#pragma once
#include <pthread.h>
#include <glog/logging.h>//glog

#include "src/svrender/common/glm/glm.hpp"
#include "src/svrender/common/glm/gtc/matrix_transform.hpp"
#include "src/svrender/common/glm/gtc/type_ptr.hpp"

#include"common/svtype.hpp"//POD数据自定义及SV公共数据结构
namespace sm {
namespace sv_avm {
namespace svrender {
namespace mvp{



class InnerSV_MvCalss {
public:
  enum {
    SV_ENUM_VIEWMODE_3D=0, //任意3D视点
    SV_ENUM_VIEWMODE_2D, //鸟瞰视点
    SV_ENUM_VIEWMODE_BUTT
  };

  explicit InnerSV_MvCalss(SV_VOID);
  ~InnerSV_MvCalss(SV_VOID);
  //@brief 获取Mv矩阵
  //@return mv矩阵
  //@remarks 调用此接口前，InnerSV_MvCalss需已初始化，否则会产生断言
  inline glm::mat4 GetMv(SV_VOID){
    CHECK(bInitialized);
    return stMv;
  };
  //@brief 获取Mn矩阵
  //@return Mn矩阵
  //@remarks 调用此接口前，InnerSV_MvCalss需已初始化，否则会产生断言
  inline glm::mat4 GetMn(SV_VOID){CHECK(bInitialized)<<"Not bInitialized";return stMn;};
  inline glm::vec3 GetVehicleEyeLight(SV_VOID){CHECK(bInitialized)<<"Not bInitialized";return stVehicleEyeLight;}
  //@brief 获取camera Mvp矩阵
  //@return Mvp矩阵
  //@remarks 调用此接口前，InnerSV_MvCalss需已初始化，否则会产生断言
  //         因此车模Mvp矩阵与摄像头Mvp读取接口不同
  inline glm::mat4 GetVehicleMvp(const SV_S32& s32ViewMode){CHECK(bInitialized)<<"Not bInitialized";return SV_ENUM_VIEWMODE_3D==s32ViewMode?stVehicleMvp3D:stVehicleMvpBird;};
  //@brief 计算相机虚拟视点Mvp矩阵
  //@param in s32ViewMode 虚拟视点模式
  //@remarks s32ViewMode可选值为SV_ENUM_VIEWMODE_3D、SV_ENUM_VIEWMODE_2D
  glm::mat4 GetCameraMvpMatrix(const SV_S32& s32ViewMode);
  //@brief 获取当前虚拟视点参数
  //@param out glm::vec3* stPos 当前虚拟视点相机坐标
  //       out glm::vec2* stRotate 当前虚拟视点相机旋转量
  inline SV_VOID GetVirtualCameraParams(glm::vec3* stPos,glm::vec2* stRotate) {*stPos = stViewPointCameraPos; *stRotate = stViewCameraRotate;};
  //@brief 初始化函数，初始化stMv及stMn、stCameraMvp3D、stCameraMvpBird
  SV_VOID Initialized(SV_VOID);
  //@brief 设置虚拟视点参数
  //@param in stPos 虚拟视点相机坐标
  //       in stRot 虚拟视点相机旋转量
  //@remarks 此函数内部会比较设置的虚拟相机参数是否与protect数据stViewPointCameraPos、stViewCameraRotate一致
  //         如不一致，变更stViewPointCameraPos与stViewCameraRotate的值，同时生成新的stMv、stMn
  SV_VOID SetVirtualCameraParams(const glm::vec3& stPos,const glm::vec2& stRot);
  //@brief 设置车模型缩放比
  //@param in vehicleScale 车模缩放比
  //@remarks 车模映射子模块加载车模后调用
  SV_VOID SetVehicleScale(const glm::vec3& vehicleScale);
protected:
  SV_BOOL bInitialized;//是否已初始化
  glm::vec3 stViewPointCameraPos; //虚拟视点相机位置
  glm::vec2 stViewCameraRotate;//虚拟视点相机旋转
private:
  const glm::mat4 gstProjection3D =  glm::perspective(45.0f, 1.f, 0.1f, 10.0f); //任意视点透视变换矩阵
  const glm::mat4 gstProjectionBird = glm::perspective(45.0f, 1.0f, 0.1f, 10.0f); //鸟瞰视点透视变换矩阵
  const glm::mat4 gMvBird =glm::rotate(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -4.5)),
      0.f, glm::vec3(1, 0, 0)), -1.57f, glm::vec3(0, 0, 1));
  const SV_F32 gf32CarOrigitationX = 90.0;
  const SV_F32 gf32CarOrigitationY = 270.0;
  //prevent the camera to flip the model upside down and look under the model
  const SV_F32 gf32CamLimitRyMin = -1.57;
  const SV_F32 gf32CamLimitRyMax = 0.0;
  // prevent the camera from zooming too close
  const SV_F32 gf3CamLimitZoomMin =  -11.5;
  const SV_F32 gf32CamLimitZoomMax = -2.5;




  //@brief 利用stViewPointCameraPos及stViewCameraRotate计算虚拟视点stMv矩阵
  //@return stMv矩阵
  glm::mat4 CalcMvMatrix(SV_VOID);
  //@brief 利用stViewPointCameraPos及stViewCameraRotate计算虚拟视点stMn矩阵
  //@return stMn
  glm::mat4 CalcMnMatrix(SV_VOID);//
  //@breif 比较设置的虚拟视点参数是否与protect数据stViewPointCameraPos、stViewCameraRotate一致
  //@return SV_TRUE/SV_FALSE
  SV_BOOL IsViewPointParamsEqual(const glm::vec3& stPos,const glm::vec2& stRot);
  //@brief 计算所有私有数据对象，包括stMv、stMn、stCameraMvp3D、stCameraMvpBird
  SV_VOID CalcAllPrivateDatas(SV_VOID);

  //@brief 计算车模虚拟相机矩阵
  //@param in stVehicleScale 车模DAE文件三维尺度与实际车辆三维尺度的比值
  glm::mat4 CalcVehicleCameraMatrix(const glm::vec3& stVehicleScale);

  //@brief 计算车模虚拟视点Mvp矩阵
  //@param in stVehicleScale 车模DAE文件三维尺度与实际车辆三维尺度的比值
  //       in s32ViewMode 虚拟视点模式
  //         s32ViewMode可选值为SV_ENUM_VIEWMODE_3D、SV_ENUM_VIEWMODE_2D
  //         调用此接口前，InnerSV_MvCalss需已初始化，否则会产生断言
  glm::mat4 CalcVehicleMvpMatrix(const glm::vec3& stVehicleScale,const SV_S32& s32ViewMode);

  glm::mat4 stMv;
  glm::mat4 stMn;

  glm::mat4 stCameraMvp3D;
  glm::mat4 stCameraMvpBird;

  glm::vec3 stVehicleScale;
//  glm::mat4 stVehicleCameraMatrix;
  glm::mat4 stVehicleMvp3D;
  glm::mat4 stVehicleMvpBird;
  glm::vec3 stVehicleEyeLight;
};

}//end of mvp
}//end of svrender
}//end of sv_avm
}//end of sm

#endif /* SRC_SVRENDER_VIEWTRANSOFORM_VIEWTRANSFORM_HPP_ */
