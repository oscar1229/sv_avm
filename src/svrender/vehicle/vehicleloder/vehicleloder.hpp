/*
 * vehicleloder.hpp
 *
 *此文件声明了有关调用assimp库，将dae车模文件加载为opengl 顶点数组对象的类InnerSV_VehicleLoader
 */

#ifndef SV_SVM_SVRENDER_VEHICLE_VEHICLELODER_VEHICLELODER_HPP_
#define SV_SVM_SVRENDER_VEHICLE_VEHICLELODER_VEHICLELODER_HPP_
#pragma once

#include<vector>
#include<glog/logging.h>

#include "common/svtype.hpp"//POD数据自定义及SV公共数据结构

#include "src/svrender/common/glm/glm.hpp"

namespace sm {
namespace sv_avm {
namespace svrender {
namespace vehicle {
namespace vehicleloder {

struct SV_MATERIAL_S {
  glm::vec3 stAmbient;//颜色环境光分量
  glm::vec3 stDiffuse;//颜色散射光分量
  glm::vec3 stSpecular;//颜色镜面光分量
  SV_F32 f32shininess;//本身发光光亮
};

typedef std::vector<SV_MATERIAL_S> PST_MATERIA_VECT;
//@brief 调用assimp库加载车模dae文件为opengl 顶点缓存对象的类
//@remarks 该类不可移动及复制
//         该类的使用实例如下
//         InnerSV_VehicleLoader clVehicleLoder;
//         if(clVehicleLoder.Initialize(filepath,stVehicleSize) {
//          ...
//          glm::vec3 stVehicleScal = clVehicleLoder.GetVehicleScal();
//         }
//         ...
//         while(true) {
//           ...
//           clVehicleLoder.draw(s32Shader);
//         }
class InnerSV_VehicleLoader {
public:
  InnerSV_VehicleLoader();
  ~InnerSV_VehicleLoader(void);
 inline glm::vec3 GetVehicleScal(SV_VOID) {CHECK(bInitialized)<<"Get param before init";return stCarScal;}
//@brief 初始化创建对象
 //@param in filepath 车模全路径dae文件名
 //       in stVehicleSize 实际车辆尺寸参数
 //@return dae文件不存在，或内部内存空间申请失败返回SV_FALSE
 //@remarks 函数内部读取filepath指向的dae文件，调用assimp库，将3D车模文件加载成opengl 顶点缓冲区对象
 //         函数内部申请的内存空间无需调用者释放，在析构时自动释放
 const SV_BOOL Initialize(const SV_S8* filepath,const SV_SIZE_S& stVehicleSize);
 //@brief opengl 3D车模绘制
 //@param s32Shader 着色器程序句柄
 const SV_VOID Draw(SV_U32 s32Shader,const SV_U32 &u32AmbientLoc,\
    const SV_U32 &u32DiffuseLoc);

protected:
  SV_BOOL  bInitialized;
private:
  //@remarks 显式申明移动构造函数和赋值运算符，禁用当前类的复制，只声明，不做定义
  InnerSV_VehicleLoader(const InnerSV_VehicleLoader&);
  InnerSV_VehicleLoader& operator = (const InnerSV_VehicleLoader& m);

  PST_MATERIA_VECT pstMaterialVect;
 // PST_VBO_VECT pVBOVect;
  struct SV_VBO_VECT_S* pclVbVector;
 //车模DAE文件三维尺度与实际车辆三维尺度的比值
  glm::vec3 stCarScal;

};

}//end of vehicleloder
}//end of vehicle
}//end of svrender
}//end of sv_avm
}//end of sm




#endif /* SV_SVM_SVRENDER_VEHICLE_VEHICLELODER_VEHICLELODER_HPP_ */
