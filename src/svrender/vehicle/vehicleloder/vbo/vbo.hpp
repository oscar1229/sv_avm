/*
 * vbo.hpp
 *
 *此头文件包含了有关车模OpenGL缓冲区对象相关的类的声明
 *
 */

#ifndef VBO_HPP
#define VBO_HPP
#pragma once
#include<assert.h>
#include<vector>

#include <glog/logging.h>//glog 谷歌日志库

#include "common/svtype.hpp"//POD数据自定义及SV公共数据结构

namespace sm {
namespace sv_avm {
namespace svrender {
namespace vehicle {
namespace vehicleloder {
namespace vbo{
//@brief 车模OpenGL缓冲区对象相关的类
//@remarks 该类不可复制
//         该类禁用调用Init前的数据成员读取操作
//         该类的使用实例如下：
//         InnerSV_VehicleVboClass clVehicleVbo;
//         clVehicleVbo.Init(stVertexVect,stNormalVect,stTexCoordVect,s32Id);
//         ...
class InnerSV_VehicleVboClass {
public:
  //车模openGL顶点缓冲区对象类型枚举
enum SV_VBO_INDICES_E {
    SV_ENUM_VBO_P_VERTEX, //顶点
    SV_ENUM_VBO_P_NORMAL, //这个暂时不知道是什么.
    SV_ENUM_VBO_P_TEXCOORD, //纹理
    SV_ENUM_VBO_P_INDEX//面向
};
explicit InnerSV_VehicleVboClass();
~InnerSV_VehicleVboClass();
  //@brief 创建车模opengl缓冲区数组对象及Vertex，normal，texcoord,faces等顶点缓冲对象
  //@param in stVertexVect 车模顶点坐标
  //       in stNormalVect 暂时不知道是什么
  //       in stTexCoordVect 车模纹理映射坐标
  //       in s32Id 车模opengl缓冲区数组对象Id号
  //stVertexVect、stNormalVect、stTexCoordVect的Size必须一致，否则会生成断言

const SV_VOID Init(const std::vector<SV_POINT3F32_S>& stVertexVect,
        const std::vector<SV_POINT3F32_S>& stNormalVect,
        const std::vector<SV_POINT2F32_S>& stTexCoordVect,
        const SV_U32 &s32Id);

inline SV_U32 u32GetVAO(){CHECK(SV_TRUE == bInitialized);return u32Vao;};
inline SV_U32 u32GetId(){CHECK(SV_TRUE == bInitialized);return u32Id;};
inline SV_U32 u32GetCount(){CHECK(SV_TRUE == bInitialized);return u32Count;};
protected:
SV_BOOL bInitialized;
private:
  //显式申明赋值运算符，禁用当前类的复制，只声明，不做定义
InnerSV_VehicleVboClass& operator = (const InnerSV_VehicleVboClass& m);

SV_U32 u32Count;
SV_U32 u32Id;
  //pointer vertex array
SV_U32 u32Vao;
  //ointers to all buffers: vertex, normal, texcoordinate and faces
SV_U32 au32Buffer[4];
};//end of InnerSV_VboClass
}  // namespace vbo

//定义vehicleloder命名空间的结构体，方便在vehicleloder.hpp中，前置声明调用vbo头文件中vbo命名空间的InnerSV_VehicleVboClass类
struct SV_VBO_VECT_S {
std::vector<vbo::InnerSV_VehicleVboClass> clVect;
};
}  // namespace vehicleloder
}  // namespace vehicle
}  // namespace svrender
}  // namespace sv_avm
}  // namespace sm



#endif  // VBO_HPP
