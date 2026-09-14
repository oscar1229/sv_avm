/*
 * mesh.hpp
 *
 */

#ifndef MESH_HPP
#define MESH_HPP
#pragma once

#include<vector>

#include "common/svtype.hpp"//POD数据自定义及SV公共数据结构
#include "include/sv_avmcommon.hpp"

namespace sm {
namespace sv_avm {
namespace svrender {
namespace camera {
namespace mesh {
//@brief 用于生成网面Mesh的输入数据结构体
struct SV_MESH_GEN_PARAM_S{
SV_BOWL_GRID_PARAM_S stBowlGrid;//碗面网格参数
SV_S32 s32CameraChannl;//摄像头通道号，可选值为SV_ENUM_CAMERA_LEFT、SV_ENUM_CAMERA_RIGHT
  //                       SV_ENUM_CAMERA_FRONT 、SV_ENUM_CAMERA_BACK
SV_F32 f32VehicleDialog;//归一化的车辆矩形区域对角线长度的一半
SV_SIZEF_S stNormalizeVehicleSize;//归一化的车型尺寸参数
};

//@brief  摄像头Mesh网格数据
struct SV_MESH_S {//
SV_POINT3F32_S stVerter;//碗面顶点坐标
SV_POINT3F32_S stCoord;//像素纹理坐标加透明度
};
//            .    .
//           .  .
//          x x
//每个角度，每个弧长对应有两个点,每四个点组成2个三角型
struct SV_MESH_BOWLPOINT_S {
SV_POINT3F32_S stPoint_e; //start弧度，即为当前弧度-1
SV_POINT3F32_S stPoint_s;//end 弧度，即为当前弧度
};

//@brief 单通道网格规模统计,用于对比"配置暗示的理论上限"与"实际提交GPU的网格量"
//@remarks 理论上限按 grid_subdiv(s32Angles) 全角度 x 每角度最大径向采样点数 推算,
//         即假设每个离散弧度都参与、且所有顶点投影均落在图像内时的规模;
//         实际值为经过角度扇区裁剪、重叠区半径裁剪(f32Rmax)以及
//         bIsImagePointAvalid 投影有效性裁剪之后真正写入VBO的顶点数。
struct SV_MESH_STAT_S {
SV_S32 s32Angles;          //grid_subdiv,即碗面1+2象限角度离散份数
SV_S32 s32AngleUsed;       //实际生成网格的角度条数
SV_S32 s32MaxRangePerAngle;//单条角度上实际出现过的最大径向采样点数
SV_S64 s64VertexActual;    //实际顶点数(= glDrawArrays count)
SV_S64 s64TriangleActual;  //实际三角形数
SV_S64 s64TriangleTheory;  //理论上限三角形数
};
//@brief 获取归一化的车型参数尺寸
//@param in stIn 未归一化的车型尺寸参数，即（宽，长）
//@return 归一化的车型参数尺寸
SV_SIZEF_S GetNormalizeVehicleSize(const SV_SIZE_S& stIn);
//@brief 求取归一化的车身矩形的1/2对角线长度
//@param stVehicleSize 未归一化的车型尺寸参数，即（宽，长）
//@return 归一化的车身矩形的1/2对角线长度
SV_F32 CalcHalfDiagLengthFromVehicleSize(const SV_SIZE_S& stVehicleSize);
//@brief 计算虚拟碗状曲面的高度
//@param in stCameraParamsVector 各摄像头内外参数
//       in stVehicleSize 车辆矩形参数
//       in stGridParam 碗装曲面参数
//@remarks 虚拟碗面的z轴高度为，所有摄像头能覆盖的高度区域间的最小值,由安装角度俯仰角最小的摄像头决定
//         因此为了保证足够的虚拟碗面高度，摄像头的安装角度不可过于垂直
SV_S32 CalcGridBowlHeight(const std::vector<SV_CAMERA_PARAMS_S> &stCameraParamsVector,
    const SV_F32& f32VehicleHalfDialog, const SV_BOWL_GRID_PARAM_S& stGridParam) ;

//@brief 计算SV_MESH_GEN_PARAM_S结构体
//@Param  in stGridParam 碗面网格参数
//        in stVehicleSize 实际的未归一化的车型尺寸参数
//        in s32CamerCh 摄像头通道号
//@Return SV_MESH_GEN_PARAM_S结构体数据
SV_MESH_GEN_PARAM_S CalcMeshGenParam(const SV_BOWL_GRID_PARAM_S& stGridParam, const SV_SIZE_S& stVehicleSize,
    const SV_S32& s32CamerCh);
//@brief 利用碗面网格参数及相机参数生成openGL mesh结构的类
//@remarks 该类不可复制及移动
//         当前类的使用实例如下：
//         SV_F32 f32VehicleHalfDialog =CalcHalfDiagLengthFromVehicleSize(stVehicleSize);
//         SV_S32 s32NopZ =  CalcGridBowlHeight(stCameraParamsVector,f32VehicleHalfDialog,stGridParam);
//         stGridParam.s32NopZ = s32NopZ;
//         std::vector< std::vector<SV_MESH_S> > stCamerasVect;
//         for(SV_S32 i=0;i< stCameraParamsVector.size();i++) {
//             SV_MESH_GEN_PARAM_S stGenParams = CalcMeshGenParam(stGridParam,stVehicleSize,i);;
//             InnerSV_CameraMeshClass stCameraMeshClass(stGenParams)
//             std::vector<SV_MESH_S> stMeshVect；
//             stCameraMeshClass.GenGLMesh(stCameraParamsVector[i],&stMeshVect);
//             stCamerasVect.pushback(stMeshVect);
//          }
class InnerSV_CameraMeshClass {
public:
explicit InnerSV_CameraMeshClass(const SV_MESH_GEN_PARAM_S &stGenParams);
  //@brief  利用相机参数及stMeshGenParams生成opengl 顶点数组对象
  //@param in stCameraParam 相机参数
  //@remarks 顶点数组数据的结构为（顶点坐标，带权重纹理坐标）
  //         顶点坐标结构（x,y,z）
  //         纹理坐标结构（x,y,weight）
  //         内部处理流程如下：1.计算碗面顶点向量及其权重向量，2.计算带权重相机纹理向量，3.利用顶点向量及带权重相机纹理向量生成opengl 顶点数组对象
SV_VOID GenGLMesh(const SV_CAMERA_PARAMS_S& stCameraParam, \
        std::vector<SV_MESH_S> * stMeshVects);
  //@brief 获取上一次GenGLMesh的网格规模统计(实际值与理论上限)
  //@return SV_MESH_STAT_S 网格规模统计
  //@remarks 必须在GenGLMesh之后调用,否则各计数为0
const SV_MESH_STAT_S& stGetMeshStat(SV_VOID) const { return stMeshStat; }
protected:
SV_MESH_GEN_PARAM_S stMeshGenParams;//生成网面Mesh的输入数据
private:
  //@brief 重叠区域枚举
  //@remarks  前后及左右通道区域分布
  //          前后通道
  //          |交叠区A|非交叠区|交叠区A|
  //          左右通道
  //          |交叠区B|交叠区A|非交叠区|交叠区A|交叠区B|
enum {
    SV_ENUM_OVERLAP_REGION_A = 0,
    SV_ENUM_OVERLAP_REGION_B,
    SV_ENUM_OVELAP_BUTT
};
  //@remarks 显式申明移动构造函数和赋值运算符，禁用当前类的复制，只声明，不做定义
InnerSV_CameraMeshClass(const InnerSV_CameraMeshClass&);
InnerSV_CameraMeshClass& operator = (const InnerSV_CameraMeshClass& m);
  //@breif 计算各通道交叠区域中 s32StartAngle与s32EndAngle之间区域中分隔线与不同半径同心圆交点的离散弧度值
  //@param in f32HalfVehicleDialo 归一化车型矩形对角线长度的一半
  //       in f32Step 车型矩形框长度与矩形框长宽间较大值的比值
  //       out pstAngleOfSplitLineVect 不同半径f32R同心圆交点的角度值
  //@remarks 可以利用此函数的计算结果，结算交叠区的图像融合权重，具体原理参照文件“3d网格映射及交叠去权重计算.pdf”
SV_VOID CalcAngleOfSplitLineForRanges(const SV_S32& s32StartAngle, const SV_S32& s32EndAngle, const SV_F32& f32Step,
        std::vector<SV_POINT2F32_S>* pstAngleOfSplitLineVect);

  //@breif 创建非重叠区的某一弧度的碗面网格
  //@param in s32Angle 离散弧度值
  //       in f32NormalizeYOffset 归一化的y坐标偏移量
  //       out pstGridAngleVect 某一弧度的碗面顶点坐标向量
  //       out pstWeightAngleVect 某一弧度的碗面顶点坐标的权重向量
SV_VOID GenGridEachAngleAngleNoOverlap(const SV_S32& s32Angle,
        const SV_F32& f32NormalizeYOffset,
        std::vector<SV_MESH_BOWLPOINT_S>* pstGridAngleVect,
        std::vector<SV_POINT2F32_S> * pstWeightAngleVect);
  //@brief 创建重叠区域内某一弧度的碗面网格顶点坐标向量及其权重
  //@param in s32Angle 离散弧度值
  //       in f32Rmax 当前弧度在重叠区域内的最大极坐标长度
  //       in f32NormalizeYOffset  归一化的y坐标偏移量
  //       in stSpliLineAngleVect  不同半径同心圆与分割线的交的弧度值向量
  //       in s32Quarter 重叠区所处的笛卡尔坐标系象限
  //       out pstGridAngleVect 某一弧度的碗面网格顶点坐标向量
  //       out pstWeightAngleVect 某一弧度碗面网格顶点坐标权重向量
SV_VOID GenGridEachAngleAngleOverlap(const SV_S32& s32Angle, const SV_F32&  f32Rmax,
        const SV_F32& f32NormalizeYOffset, const std::vector<SV_POINT2F32_S> & stSpliLineAngleVect,
        const SV_S32 &s32Quarter,
        std::vector<SV_MESH_BOWLPOINT_S>* pstGridAngleVect,
        std::vector<SV_POINT2F32_S>* pstWeightAngleVect);
  //@breif 创建各通道s32NoOvelayAngleStart与s32NoOverlayAngleEnd之间非重叠区域的碗面网格
  //@param in s32NoOvelayAngleStart 非重叠区起点离散弧度值
  //       in s32NoOverlayAngleEnd  非重叠区终点离散弧度值
  //       out pstGridsVect 碗面网格顶点坐标向量
  //       out pstWeights 碗面网格顶点坐标权重向量
SV_VOID GenGridsNoOverLay(const SV_S32& s32NoOvelayAngleStart, const SV_S32& s32NoOverlayAngleEnd,
        std::vector <std::vector<SV_MESH_BOWLPOINT_S> >* pstGridsVect,
        std::vector< std::vector<SV_POINT2F32_S> >* pstWeights);
  //@brief 创建视图重叠区域s32Start到S32End碗面顶点向量及其权重向量
  //@param in s32Start 开始离散弧度值
  //       in s32End 结束离散弧度值
  //       in stSpliLineAngleVect 交叠区域中分隔线与不同半径同心圆交点的角度值
  //       in stSplitLine 分割线指向方程参数ax+by+c=0
  //       in s32OverlapRegion 交叠区域类型
  //       out pstGridsVect 碗面顶点向量指针
  //       out pstWeights 顶点权重向量
  //@remarks s32OverlapRegion可选值为SV_ENUM_OVERLAP_REGION_A、SV_ENUM_OVERLAP_REGION_B
SV_VOID GenGridsOverlapOfRegion(const SV_S32& s32Start, const SV_S32& s32End,
        const std::vector<SV_POINT2F32_S> & stSpliLineAngleVect,
        const SV_POINT3F32_S& stSplitLine, const SV_S32 &s32OverlapRegion,
        std::vector <std::vector<SV_MESH_BOWLPOINT_S> >* pstGridsVect,
        std::vector< std::vector<SV_POINT2F32_S> >* pstWeights);
  //@brief 根据当前摄像头通道弧度范围及交叠区弧度范围修正stMeshGenParams.stBowlGrid.f32OverLayAngle值
  //@param in s32StartAngle 当前摄像头通道起始离散弧度
  //       in s32NoOverLapAngleStart 当前摄像头通道非交叠区起始离散弧度
  //       in f32AngleStep 离散弧度1代表的实际弧度值
  //@remarks f32AngleStep = CV_PI/stMeshGenParams.stBowlGrid.s32Angles
SV_VOID ModifyOverlapAngle(const SV_S32& s32StartAngle, const SV_S32& s32NoOverLapAngleStart, const SV_F32 &f32AngleStep);
  //@brief 计算交叠区分割线ab
  //@remarks 交叠区分割线的定义如下所示，直线ab即为分割线
  //                     b
  //                   .
  //                 .
  //               .
  //     ------- ...........
  //    |      | a
SV_POINT3F32_S CalcSplitLine(SV_VOID);
  //@brief 生成前/后视摄像头的碗面顶点向量及其纹理向量
  //@param in s32StartAngle 当前通道开始离散弧度值
  //       in s32NoOverLapAngleStart 非交叠区起始离散弧度
  //       in f32AngleStep 离散弧度1代表的实际弧度值
  //       out pstGridsVect 碗面顶点向量
  //       out pstWeights 顶点权重向量
  //@remarks 生成的stGridsVect与pstWeights具有同样的Size,且stGridsVect[i]与pstWeights[i]具有同样的Size
  //         函数内部流程如下：
  //         1.ModifyOverlapAngle,修正交叠区大小
  //         2.调用CalcAngleOfSolitLineForRanges生成分隔线与不同半径同心圆交点的离散弧度值向量
  //         3.调用GenGridsOverlapOfRegion 生成弧度范围s32StartAngle - s32NoOverLapAngleStart之间的A型交叠区碗面顶点向量
  //         4.调用GenGridsNoOverLay 生成弧度范围s32NoOverLapAngleStart - (s32Angles-s32NoOverLapAngleStart)之间的非交叠区顶点向量
  //         5.调用GenGridsOverlapOfRegion 生成弧度范围（s32Angles-s32NoOverLapAngleStart）- (s32Angles-s32StartAngle)之间的A型交叠区碗面顶点向量
SV_VOID GenGridsFBChannl(const SV_S32& s32StartAngle, const SV_S32& s32NoOverLapAngleStart,
        const SV_F32 &f32AngleStep,
        std::vector <std::vector<SV_MESH_BOWLPOINT_S> >* pstGridsVect,
        std::vector< std::vector<SV_POINT2F32_S> >* pstWeights);
  //@brief 计算左右通道A,B型交叠区的边界离散弧度值
  //@param in s32StartAngle 当前通道开始离散弧度值
  //       in f32AngleStep 离散弧度1代表的实际弧度值
  //@return A,B型交叠区的边界离散弧度值
  //@remarks A,B型交叠区的边界离散弧度值利用正弦定力求取而来
SV_S32 CalcAngleBetweenOverlapRegionAB(const SV_S32& s32StartAngle, const SV_F32 &f32AngleStep);
  //@brief 生成左/右视摄像头的碗面顶点向量及其权重向量
  //@param in s32StartAngle 当前通道开始离散弧度值
  //       in s32NoOverLapAngleStart 非交叠区起始离散弧度
  //       in f32AngleStep 离散弧度1代表的实际弧度值
  //       out pstGridsVect 碗面顶点向量
  //       out pstWeights 顶点权重向量
  //@remarks 生成的stGridsVect与pstWeights具有同样的Size,且stGridsVect[i]与pstWeights[i]具有同样的Size
  //         函数内部流程如下：
  //         1.ModifyOverlapAngle,修正交叠区大小
  //         2.调用CalcSplitLine计算交叠区分割线ab
  //         3.调用CalcAngleOfSolitLineForRanges生成分隔线与不同半径同心圆交点的离散弧度值向量
  //         4.调用CalcAngleBetweenOverlapRegionAB计算左右通道A,B型交叠区的边界离散弧度值s32AngleAB
  //         5.GenGridsOverlapOfRegion 生成弧度范围s32StartAngle - s32AngleAB 之间的B型交叠区顶点向量
  //         6.调用GenGridsOverlapOfRegion 生成弧度范围s32AngleAB - s32NoOverLapAngleStart之间的A型交叠区碗面顶点向量
  //         7.调用GenGridsNoOverLay 生成弧度范围s32NoOverLapAngleStart - (s32Angles-s32NoOverLapAngleStart)之间的非交叠区顶点向量
  //         8.调用GenGridsOverlapOfRegion 生成弧度范围（s32Angles-s32NoOverLapAngleStart）- (s32Angles-s32AngleAB)之间的A型交叠区碗面顶点向量
  //         9.调用GenGridsOverlapOfRegion 生成弧度范围（s32Angles-s32AngleAB）- (s32Angles-s32StartAngle)之间的B型交叠区碗面顶点向量
SV_VOID GenGridsLRChannl(const SV_S32& s32StartAngle, const SV_S32& s32NoOverLapAngleStart,
        const SV_F32 &f32AngleStep,
        std::vector <std::vector<SV_MESH_BOWLPOINT_S> >* pstGridsVect,
        std::vector< std::vector<SV_POINT2F32_S> >* pstWeights);
  //@brief 计算当前通道碗面网格
  //@param out pstGridsVect 碗面顶点向量
  //       out pstWeights 顶点权重向量
  //@remarks 函数内部调用GenGridsFBChannl或GenGridsLRChannl生成碗面顶点向量及其权重向量
SV_VOID GenGrids(std::vector <std::vector<SV_MESH_BOWLPOINT_S> >* pstGridsVect,
        std::vector< std::vector<SV_POINT2F32_S> >* pstWeights);
  //@brief 计算网面网格的带权重摄像头纹理坐标
  //@param in stBowlPointVect 碗面顶点向量
  //       in stWeightVect 顶点权重向量
  //       in stCameraParam 摄像头参数
  //       out pstImagePointVect  带权重摄像头纹理坐标。
  //@remarks 摄像头纹理坐标值结构为{（x0,y0,weight0）,(x1,y,weight1)}
  //         函数内部，采用stBowlPointVect为世界坐标点，读取摄像头参数stCameraParam，计算各世界坐标
  //         点对象的像素坐标点，并归一化各像素坐标点。
SV_VOID ProjectCameraTexCoord(const std::vector <std::vector<SV_MESH_BOWLPOINT_S> >& stBowlPointVect, \
        const std::vector< std::vector<SV_POINT2F32_S> > &stWeightVect, \
        const SV_CAMERA_PARAMS_S& stCameraParam, \
        std::vector< std::vector<SV_MESH_BOWLPOINT_S> >* pstImagePointVect);
  //@brief 利用当前离散弧度值碗面顶点坐标及相机纹理坐标生成当前离散弧度值的opengl顶点数组
  //@param in stCameraFrameSize 摄像头图像尺寸
  //       in stP3dfVect 碗面顶点向量
  //       in stP2dVect 带权重摄像头纹理坐标
  //       out stMeshVects 前离散弧度值的opengl顶点数组
  //@remarks 对于每个极坐标(s32R,s32Angle),计算两个点，组成一个SV_MESH_BOWLPOINT_S对象
  //         利用同一弧度值临近的2个SV_MESH_BOWLPOINT_S数据，生成两个三角型构成的opengl顶点数组元素
  //                 3   4
  //                1  2
  //         1 2 3 构成三角形1；2 4 3 构成三角形2
SV_VOID AngeleGridToMesh(const SV_SIZE_S &stCameraFrameSize,
        const std::vector<SV_MESH_BOWLPOINT_S> &stP3dfVect,
        const std::vector<SV_MESH_BOWLPOINT_S> &stP2dVect,
        std::vector<SV_MESH_S> * stMeshVects);
  //@brief  利用碗面顶点坐标及相机纹理坐标生成opengl顶点数组
  //@param in stCameraFrameSize 摄像头图像尺寸
  //       in stP3dVects 碗面顶点向量
  //       in stP2dVects 带权重摄像头纹理坐标
  //       out pstMeshVects opengl顶点数组
  //@remarks 内部调用AngeleGridToMesh，生成每个离散弧度值的opengl顶点数组对象
SV_VOID GridToMesh(const SV_SIZE_S &stCameraFrameSize,
        const std::vector< std::vector<SV_MESH_BOWLPOINT_S> > &stP3dVects,
        const std::vector< std::vector<SV_MESH_BOWLPOINT_S> > &stP2dVects,
        std::vector<SV_MESH_S> * pstMeshVects);

  //@brief 依据GenGrids输出的碗面网格,统计实际网格规模并推算理论上限
  //@param in stGridsVect 碗面顶点向量(每个元素为一条离散弧度上的点对序列)
  //       in s64VertexActual 实际写入VBO的顶点数
SV_VOID CalcMeshStat(const std::vector <std::vector<SV_MESH_BOWLPOINT_S> >& stGridsVect,
        const SV_S64& s64VertexActual);

SV_MESH_STAT_S stMeshStat;//上一次GenGLMesh的网格规模统计
std::vector<SV_F32> f32BowlHeightVect;//碗装曲面非平面部分高度的离散值
std::vector<SV_F32> f32CosValVect;//各个角度离散的余弦值
std::vector<SV_F32> f32SinValVect;//各个角度离散的正弦值
SV_F32 f32RGround;
SV_F32 f32RBowl;
};//end of InnerSV_CameraMeshClass

}  // namespace mesh
}  // namespace camera
}  // namespace svrender
}  // namespace sv_avm
}  // namespace sm

#endif  // MESH_HPP
