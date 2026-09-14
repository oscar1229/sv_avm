/*
 * mesh.cpp
 *
 */
#include "mesh.hpp"

#include<glog/logging.h> //glog

#include <cstring>

#include <opencv2/opencv.hpp>

namespace sm {
namespace sv_avm {
namespace svrender {
namespace camera {
namespace mesh {
static const SV_F32 gf32InvPi = 1.0/CV_PI;
static const SV_F32 gf32HalfPi = 0.5*CV_PI;

namespace _local {
enum {
SV_ENUM_PARAM_RVECT = 0,
SV_ENUM_PARAM_TVECT,
SV_ENUM_PARAM_BUTT,
};

enum {
SV_ENUM_QUARTER_1 = 0,
SV_ENUM_QUARTER_2,
SV_ENUM_QUARTER_BUTT,
};

//@brief  SV_CAMERA_PARAMS_S结构体数据中的af64CameraRotateVect分量或af64CameraTranslateVect分量转换为open cv Mat结构
//
//param  in stCameraParam 标定获得的相机参数
//       in  s32RvecOrTvec获取旋转或平移向量的Flag
//@return  cv::Mat类的1X3的平移或旋转向量矩阵
//@remarks  s32RvecOrTvec可选值为SV_ENUM_PARAM_RVECT或SV_ENUM_PARAM_TVEC
static cv::Mat GetMatRvecOrTvecFromSvCameraParameters(const SV_CAMERA_PARAMS_S &stCameraParam, const SV_S32& s32RvecOrTvec);

//@brief  SV_CAMERA_PARAMS_S结构体数据中的af64CameraK分量，转换为opencv 3x3的mat结构
//@param in stCameraParam 标定获得的相机参数
//@return cv::Mat类的3x3的相机内参矩阵
static cv::Mat GetMatKFromSvCameraParameters(const SV_CAMERA_PARAMS_S &stCameraParam);

//@brief 计算每个通道虚拟碗面的Z轴高度
//@param in f32VehicleDialo 归一化车型矩形对角线长度的一半
//       in stCameraParam 摄像头内外参参数及像素尺寸
//       in stGridParam 碗装曲面参数
//@return 每个通道虚拟碗面的Z轴高度
static SV_S32 CalcGridBowlHeightEachChannl(const SV_CAMERA_PARAMS_S &stCameraParam,
    const SV_BOWL_GRID_PARAM_S& stGridParam, const SV_F32 &f32VehiclehalfDialog);

//@brief 计算前/后通道交叠区域分隔线与半径1.0/f32InvR的圆的交点的弧度值组
//@param  in f32InvR 同心圆半径的倒数
//        in f32RadianBeta 应用正弦定理时某个角的弧度值，具体哪个角，哥哥我忘记了^_^
//        in f32HalfVehicleDialo 归一化车型矩形对角线长度的一半
//        in f32OverlayAngle交叠区角度的弧度值
//        in f32Yoffset 车型矩形框长度于矩形框长宽间较大值的比值
//        in f32AngeStep s32Angles/CV_PI
//@return  两条分割线与圆的角点
//@remark  可以利用此函数的计算结果，计算交叠区的图像融合权重，
static SV_POINT2F32_S CalcSplitLineAnglesEachRforFBChannl(const SV_F32 &f32InvR, const SV_F32& f32RadianBeta,
    const SV_F32 &f32HalfVehicleDialo, const SV_F32 &f32OverlayAngle, const SV_F32& f32Yoffset,
    const SV_F32 f32AngeStep);

//@brief 计算左/右通道交叠区域分隔线与不同半径1.0/f32InvR同心圆交点的角度值
//@param  in f32InvR 同心圆半径的倒数
//        in f32RadianBeta 应用正弦定理时某个角的弧度值，具体哪个角，哥哥我忘记了^_^
//        in f32HalfVehicleDialo 归一化车型矩形对角线长度的一半
//        in f32OverlayAngle交叠区角度的弧度值
//        in f32Step 车型矩形框长度与矩形框长宽间较大值的比值
//@return  交叠区域分隔线与不同半径f32R同心圆交点的弧度值
//@remarks 可以利用此函数的计算结果，结算交叠区的图像融合权重

static SV_POINT2F32_S CalcSplitLineAnglesEachRforLRChannl(const SV_F32 &f32InvR, const SV_F32& f32RadianBeta,
    const SV_F32 &f32HalfVehicleDialo, const SV_F32 &f32OverlayAngle, const SV_F32& f32Yoffset,
    const SV_F32 f32AngeStep);
//@brief  计算交叠区内点(s32Range,s32Angle)的图像融合权重
//@param in stSpliLineAngleVect交叠区域分隔线与不同半径f32R同心圆交点的角度值组
//       in s32GridAngles 碗面1+2象限角度个数，比如以1°为阶梯，则s32GridAngles为180
//@return  SV_POINT2F32_S 接否提数据
//         返回结果为计算交叠区内点(s32Range,s32Angle)的图像融合权重
static SV_POINT2F32_S CalcMixWeightEachRange(const std::vector<SV_POINT2F32_S> & stSpliLineAngleVect,
    const SV_S32 &s32Angle, const SV_S32& s32GridAngles,
    const SV_S32 &s32Quarter, const SV_S32 &s32Range);
// @brief           Rotate point according to the camera index
// @param  in       index - camera index
//         in       Point3f point - grid point
// @return          Point3f point - point after rotation
// @remarks         The function rotate grid point according to the camera index value:
//                     index = 1 - without rotation ;
//                     index = 0 - 180 degree rotation;
//                     index = 3 -  90
//                     index = 2 - 270 degree clockwise rotation;
static SV_POINT3F32_S stRotatePoint(const SV_POINT3F32_S &stIn, const SV_S32& s32CameraChannl);

//@brief 判断像素坐标点是否可用，即像素坐标点是否在图像大小范围内
//@param in stIn 带权重的像素坐标点
//       in stImageSize 摄像头图像尺寸
//return SV_TRUE/SV_FALSE
static SV_BOOL bIsImagePointAvalid(const SV_POINT3F32_S &stIn, const SV_SIZE_S& stImageSize);
//@brief 归一化坐标点，用于图像像素坐标点归一化
//@param in stPoint 非归一化的像素坐标点
//@param in stNorm  X,Y,Z各参数的模的倒数
//@remarks  stNorm的计算公式如下
//          stNorm.f32X = 1.0/相机图像宽度
//          stNorm.f32Y = 1.0/相机图像高度
//          stNorm.f32Z =  1.0
static SV_POINT3F32_S NormarizePoint3F(const SV_POINT3F32_S& stPoint, const SV_POINT3F32_S& stNorm);

//@brief 利用碗面网格顶点向量、权重向量、相机内外参数计算带权重的相机纹理坐标向量
//@param in stBowlPointVect 碗面网格顶点向量
//       in stWeightVect  权重向量
//       in mR 相机旋转向量 1x3 CV_64FC1 cv::Mat
//       in mT 相机平移向量 1x3 CV_64FC1 cv::Mat
//       in mK 相机内参矩阵 3x3 CV_64FC1 cv::Mat
//       out stImagePointVect 带权重的相机纹理坐标向量
//@remarks   带权重的相机纹理坐标的数据组成为（x,y,weight）
//           函数内部寻找当前弧度第一个Avalid像素坐标点，以便剔除
//           由于opencv Project2d函数缺陷所导致的无效点
static SV_VOID GenCameraGrid(const std::vector<SV_MESH_BOWLPOINT_S>& stBowlPointVect, \
    const std::vector< SV_POINT2F32_S> &stWeightVect,
    const cv::Mat& mR, const cv::Mat& mT, const cv::Mat& mK, const cv::Size &stimgSize, \
    std::vector<SV_MESH_BOWLPOINT_S>* stImagePointVect);
#ifdef STONKAM_DEBUG
//@brief 绘制Grid并保存为jpg文件
//@param in stGridsVect 碗面网格顶点向量
//       in stWeightsVect 权重向量
//       in f32MaxRange 碗面最大极坐标长度
//       in s32Ch 摄像头通道号
static SV_VOID DrawCameraGrid(const std::vector <std::vector<SV_MESH_BOWLPOINT_S> > &stGridsVect,
    const std::vector< std::vector<SV_POINT2F32_S> > &stWeightsVect, const SV_F32 &f32MaxRange,
    const SV_S32& s32Ch);
static SV_VOID DrawCameraMesh(const std::vector <std::vector<SV_MESH_BOWLPOINT_S> > &stImageMeshVect, const SV_S32& s32Ch);
#endif
}  // namespace _local

//获取归一化的车身矩形的1/2对角线长度
SV_F32 CalcHalfDiagLengthFromVehicleSize(const SV_SIZE_S& stVehicleSize)
{
SV_SIZEF_S stNormalize = GetNormalizeVehicleSize(stVehicleSize);
return sqrt(stNormalize.f32Width*stNormalize.f32Width+stNormalize.f32Height*stNormalize.f32Height);
}

SV_SIZEF_S GetNormalizeVehicleSize(const SV_SIZE_S& stIn)
{
SV_F32 f32Yu = static_cast<SV_F32>(stIn.s32Height);
SV_F32  f32Xr = static_cast<SV_F32>(stIn.s32Width);
if (f32Yu > f32Xr)
{
    return {f32Xr/f32Yu, 1.0};
}
else
    return {1.0, f32Yu/f32Xr};
}

SV_S32 CalcGridBowlHeight(const std::vector<SV_CAMERA_PARAMS_S> &stCameraParamsVector,
    const SV_F32& f32VehicleHalfDialog, const SV_BOWL_GRID_PARAM_S& stGridParam)
{
SV_S32 s32MinNop = INT_MAX;
CHECK(!stCameraParamsVector.empty());
for (SV_S32 i = 0 ;i < stCameraParamsVector.size();++i)
{
    s32MinNop = std::min(s32MinNop, _local::CalcGridBowlHeightEachChannl(stCameraParamsVector[i], stGridParam, f32VehicleHalfDialog));
}
return s32MinNop;
}

SV_MESH_GEN_PARAM_S CalcMeshGenParam(const SV_BOWL_GRID_PARAM_S& stGridParam,
    const SV_SIZE_S& stVehicleSize, const SV_S32& s32CamerCh) {
SV_MESH_GEN_PARAM_S stTmp;
stTmp.stBowlGrid = stGridParam;
stTmp.s32CameraChannl = s32CamerCh;
stTmp.f32VehicleDialog = CalcHalfDiagLengthFromVehicleSize(stVehicleSize);
stTmp.stNormalizeVehicleSize = GetNormalizeVehicleSize(stVehicleSize);
return stTmp;
}

InnerSV_CameraMeshClass::InnerSV_CameraMeshClass(const SV_MESH_GEN_PARAM_S &stGenParams)
{

this->stMeshGenParams = stGenParams;
  //计算各离散值
SV_F32 f32AngleStep = CV_PI/stMeshGenParams.stBowlGrid.s32Angles;
f32SinValVect.clear();
f32CosValVect.clear();
for (SV_S32 i = 0;i < stMeshGenParams.stBowlGrid.s32Angles;++i)
{
    this->f32SinValVect.push_back(sin(static_cast<SV_F32>(i)*f32AngleStep));
    this->f32CosValVect.push_back(cos(static_cast<SV_F32>(i)*f32AngleStep));
}
f32BowlHeightVect.clear();
for (uint i = 1;i < stMeshGenParams.stBowlGrid.s32NopZ+1;i++)
{
    f32BowlHeightVect.push_back(pow(i*stMeshGenParams.stBowlGrid.f32StepX, 2));
}
f32RGround = stMeshGenParams.f32VehicleDialog*stMeshGenParams.stBowlGrid.f32GroundRadiusScal;
f32RBowl = f32RGround+ stMeshGenParams.stBowlGrid.s32NopZ*stMeshGenParams.stBowlGrid.f32StepX;
memset(&stMeshStat, 0, sizeof(stMeshStat));
stMeshStat.s32Angles = stMeshGenParams.stBowlGrid.s32Angles;
}

SV_VOID InnerSV_CameraMeshClass::CalcMeshStat(
    const std::vector <std::vector<SV_MESH_BOWLPOINT_S> >& stGridsVect,
    const SV_S64& s64VertexActual) {
stMeshStat.s32Angles = stMeshGenParams.stBowlGrid.s32Angles;
stMeshStat.s32AngleUsed = static_cast<SV_S32>(stGridsVect.size());
SV_S32 s32MaxRange = 0;
for (SV_S32 i = 0;i<static_cast<SV_S32>(stGridsVect.size());++i) {
    s32MaxRange = std::max(s32MaxRange, static_cast<SV_S32>(stGridsVect[i].size()));
}
stMeshStat.s32MaxRangePerAngle = s32MaxRange;
stMeshStat.s64VertexActual = s64VertexActual;
stMeshStat.s64TriangleActual = s64VertexActual/3;
  //理论上限:grid_subdiv条角度全部参与,每条角度取实际出现过的最大径向采样点数,
  //         相邻两个点对生成2个三角形(6个顶点),且假定投影全部有效、无重叠区半径裁剪
SV_S64 s64TheoryTri = 0;
if (s32MaxRange >= 2) {
    s64TheoryTri = static_cast<SV_S64>(stMeshGenParams.stBowlGrid.s32Angles)*
        (static_cast<SV_S64>(s32MaxRange)-1)*2;
}
stMeshStat.s64TriangleTheory = s64TheoryTri;
}

SV_VOID InnerSV_CameraMeshClass::GenGLMesh(const SV_CAMERA_PARAMS_S& stCameraParam,
    std::vector<SV_MESH_S> * stMeshVects)
{
    std::vector <std::vector<SV_MESH_BOWLPOINT_S> > stGridsVect;
    std::vector< std::vector<SV_POINT2F32_S> > stWeightsVect;
    GenGrids(&stGridsVect, &stWeightsVect);//计算网面顶点网格
#ifdef STONKAM_DEBUG
    _local::DrawCameraGrid(stGridsVect, stWeightsVect, this->f32RBowl, stMeshGenParams.s32CameraChannl);
#endif
    std::vector< std::vector<SV_MESH_BOWLPOINT_S> > stImagePointVect;
   //计算网面顶点对应相机纹理坐标
    ProjectCameraTexCoord(stGridsVect, stWeightsVect, stCameraParam, &stImagePointVect);
#ifdef STONKAM_DEBUG
    _local::DrawCameraMesh(stImagePointVect, stMeshGenParams.s32CameraChannl);
#endif
    stWeightsVect.clear();
   //生成opengl Mesh
    GridToMesh(stCameraParam.stImageSize, stGridsVect, stImagePointVect, stMeshVects);
   //统计实际网格规模及配置暗示的理论上限
    CalcMeshStat(stGridsVect, static_cast<SV_S64>(stMeshVects->size()));
}

SV_VOID InnerSV_CameraMeshClass::CalcAngleOfSplitLineForRanges(
    const SV_S32& s32StartAngle, const SV_S32& s32EndAngle, const SV_F32& f32Step,
    std::vector<SV_POINT2F32_S>* pstAngleOfSplitLineVect){
pstAngleOfSplitLineVect->clear();
SV_S32 s32IdxNum =  round((f32RGround-1)/(stMeshGenParams.stBowlGrid.f32StepX)) + stMeshGenParams.stBowlGrid.s32NopZ;
SV_F32 f32R = stMeshGenParams.f32VehicleDialog;
SV_F32 f32Beta = static_cast<SV_F32>((stMeshGenParams.stBowlGrid.s32Angles-s32EndAngle))*CV_PI/ \
        static_cast<SV_F32>(stMeshGenParams.stBowlGrid.s32Angles)+stMeshGenParams.stBowlGrid.f32OverLayAngle;
SV_BOOL bFBChannl = (SV_ENUM_CAMERA_FRONT == stMeshGenParams.s32CameraChannl ||SV_ENUM_CAMERA_BACK == stMeshGenParams.s32CameraChannl);
SV_F32 f32AngleStep = static_cast<SV_F32>(stMeshGenParams.stBowlGrid.s32Angles)*gf32InvPi;
for (uint i = 0;i < s32IdxNum;i++)
{
    SV_F32 f32InvR = 1.0/f32R;
    SV_POINT2F32_S stTmp = bFBChannl?
        _local::CalcSplitLineAnglesEachRforFBChannl(f32InvR, f32Beta, stMeshGenParams.f32VehicleDialog, stMeshGenParams.stBowlGrid.f32OverLayAngle, f32Step, f32AngleStep): \
        _local::CalcSplitLineAnglesEachRforLRChannl(f32InvR, f32Beta, stMeshGenParams.f32VehicleDialog, stMeshGenParams.stBowlGrid.f32OverLayAngle, f32Step, f32AngleStep);
    pstAngleOfSplitLineVect->push_back(stTmp);
    f32R+=stMeshGenParams.stBowlGrid.f32StepX;
}
return;
}

SV_VOID InnerSV_CameraMeshClass::GenGridEachAngleAngleNoOverlap(const SV_S32& s32Angle,
    const SV_F32& f32NormalizeYOffset,
    std::vector<SV_MESH_BOWLPOINT_S>* stGridAngleVect,
    std::vector<SV_POINT2F32_S> *stWeightAngleVect) {
stGridAngleVect->clear();
stWeightAngleVect->clear();
SV_S32 angle_start = std::max(s32Angle-1, 0);
SV_F32 Ff32Range = f32NormalizeYOffset/f32SinValVect[angle_start];
  //生成地面网格
while (Ff32Range <= f32RGround)
{
    SV_POINT3F32_S  stPoint_e = {Ff32Range*f32CosValVect[s32Angle], Ff32Range*f32SinValVect[s32Angle], 0}; //生成3D碗面网格点
    SV_POINT3F32_S stPoint_s = {Ff32Range*f32CosValVect[angle_start], Ff32Range*f32SinValVect[angle_start], 0};
    stGridAngleVect->push_back({stPoint_e, stPoint_s});
    stWeightAngleVect->push_back({1.0, 1.0});//无重叠区域，权重为1
    Ff32Range += stMeshGenParams.stBowlGrid.f32StepX;
}
SV_S32 s32Num = stMeshGenParams.stBowlGrid.s32NopZ;
CHECK(s32Num <= f32BowlHeightVect.size());//断言，防止错误的操作导致的内存越界
  //生成碗面网格
for (SV_S32 i = 1;i < s32Num;++i) {
    SV_F32 f32Rstep = static_cast<SV_F32>(i)* stMeshGenParams.stBowlGrid.f32StepX;
    Ff32Range = f32RGround + f32Rstep;
    SV_POINT3F32_S  stPoint_e = {Ff32Range*f32CosValVect[s32Angle], Ff32Range*f32SinValVect[s32Angle], f32BowlHeightVect[i]};//生成3D碗面网格点
    SV_POINT3F32_S stPoint_s = {Ff32Range*f32CosValVect[angle_start], Ff32Range*f32SinValVect[angle_start], f32BowlHeightVect[i]};
    stGridAngleVect->push_back({stPoint_e, stPoint_s});
    stWeightAngleVect->push_back({1.0, 1.0});
}
return;
}

SV_VOID InnerSV_CameraMeshClass::GenGridEachAngleAngleOverlap(const SV_S32& s32Angle, const SV_F32&  f32Rmax,
    const SV_F32& f32NormalizeYOffset, const std::vector<SV_POINT2F32_S> & stSpliLineAngleVect,
    const SV_S32 &s32Quarter, std::vector<SV_MESH_BOWLPOINT_S>* stGridAngleVect, std::vector<SV_POINT2F32_S> *stWeightAngleVect) {
stGridAngleVect->clear();
stWeightAngleVect->clear();
SV_S32 angle_start = std::max(s32Angle-1, 0);
SV_F32 f32RangMin = f32NormalizeYOffset/f32SinValVect[angle_start];
SV_F32 f32Range = f32RangMin;
  //生成地面网格
SV_F32 f32RangeMax = std::min(f32RGround, f32Rmax);
SV_S32 s32Rs = round((f32Range-stMeshGenParams.f32VehicleDialog)/stMeshGenParams.stBowlGrid.f32StepX);
while (f32Range <= f32RangeMax)
{
    SV_POINT3F32_S  stPoint_e = {f32Range*f32CosValVect[s32Angle], f32Range*f32SinValVect[s32Angle], 0};
    SV_POINT3F32_S stPoint_s = {f32Range*f32CosValVect[angle_start], f32Range*f32SinValVect[angle_start], 0};
    stGridAngleVect->push_back({stPoint_e, stPoint_s});
    SV_POINT2F32_S stWeight = _local::CalcMixWeightEachRange(stSpliLineAngleVect, \
        s32Angle, stMeshGenParams.stBowlGrid.s32Angles, s32Quarter, s32Rs);
    //计算权重并输出
    stWeightAngleVect->push_back(stWeight);
    f32Range += stMeshGenParams.stBowlGrid.f32StepX;
    s32Rs++;
}
SV_S32 s32Min = std::max(static_cast<SV_S32>((f32RangMin-f32RGround)/stMeshGenParams.stBowlGrid.f32StepX), 1);
SV_S32 s32Num = std::min(static_cast<SV_S32>((f32Rmax-f32RGround)/stMeshGenParams.stBowlGrid.f32StepX), stMeshGenParams.stBowlGrid.s32NopZ+1);
for (SV_S32 i = s32Min;i < s32Num;++i) {
    SV_F32 f32Rstep = static_cast<SV_F32>(i)* stMeshGenParams.stBowlGrid.f32StepX;
    f32Range = f32RGround + f32Rstep;
    SV_POINT3F32_S  stPoint_e = {f32Range*f32CosValVect[s32Angle], f32Range*f32SinValVect[s32Angle], f32BowlHeightVect[i]};
    SV_POINT3F32_S stPoint_s = {f32Range*f32CosValVect[angle_start], f32Range*f32SinValVect[angle_start], f32BowlHeightVect[i]};
    stGridAngleVect->push_back({stPoint_e, stPoint_s});
     //计算权重并输出
    stWeightAngleVect->push_back(_local::CalcMixWeightEachRange(stSpliLineAngleVect, \
        s32Angle, stMeshGenParams.stBowlGrid.s32Angles, s32Quarter, s32Rs));
    s32Rs++;
    }
    return;
}

SV_VOID InnerSV_CameraMeshClass::GenGridsNoOverLay(const SV_S32& s32NoOvelayAngleStart,
    const SV_S32& s32NoOverlayAngleEnd,
    std::vector <std::vector<SV_MESH_BOWLPOINT_S> >* stGridsVect,
    std::vector< std::vector<SV_POINT2F32_S> >* stWeights){
DLOG(INFO) << __FUNCTION__;
SV_F32 f32NormalizeYOffset = stMeshGenParams.s32CameraChannl <SV_ENUM_CAMERA_FRONT?  \
        stMeshGenParams.stNormalizeVehicleSize.f32Width:stMeshGenParams.stNormalizeVehicleSize.f32Height;
for (SV_S32 i = s32NoOvelayAngleStart;i < s32NoOverlayAngleEnd;++i)
{
    std::vector<SV_MESH_BOWLPOINT_S> stGridVect;
    stGridVect.clear();
    std::vector<SV_POINT2F32_S> stWeightVect;
    stWeightVect.clear();
    GenGridEachAngleAngleNoOverlap(i,
        f32NormalizeYOffset,
        &stGridVect,
        &stWeightVect);
    stGridsVect->push_back(stGridVect);
    stWeights->push_back(stWeightVect);
}
return;
}

SV_VOID InnerSV_CameraMeshClass::GenGridsOverlapOfRegion(const SV_S32& s32Start, const SV_S32& S32End,
    const std::vector<SV_POINT2F32_S> & stSpliLineAngleVect,
    const SV_POINT3F32_S& stSplitLine, const SV_S32& s32OverlapRegion,
    std::vector <std::vector<SV_MESH_BOWLPOINT_S> >* stGridsVect,
        std::vector< std::vector<SV_POINT2F32_S> >* stWeights) {
DLOG(INFO) << __FUNCTION__;
SV_S32 s32MidAng = stMeshGenParams.stBowlGrid.s32Angles*0.5;
SV_S32 s32Quarder = s32Start <s32MidAng?_local::SV_ENUM_QUARTER_1:_local::SV_ENUM_QUARTER_2;
SV_F32 f32NormalizeYOffset = stMeshGenParams.s32CameraChannl < SV_ENUM_CAMERA_FRONT?  \
        stMeshGenParams.stNormalizeVehicleSize.f32Width:stMeshGenParams.stNormalizeVehicleSize.f32Height;
for (SV_S32 i = s32Start;i< S32End;i++) {
    std::vector<SV_MESH_BOWLPOINT_S> stGridVect;
    stGridVect.clear();
    std::vector<SV_POINT2F32_S> stWeightVect;
    stWeightVect.clear();
    SV_F32 f32SplitXtMP = _local::SV_ENUM_QUARTER_1 == s32Quarder?stSplitLine.f32X:-stSplitLine.f32X;
    SV_F32 f32Rmax = SV_ENUM_OVERLAP_REGION_A == s32OverlapRegion?f32RBowl: \
        stSplitLine.f32Z/(f32SplitXtMP*f32CosValVect[i]+stSplitLine.f32Y*f32SinValVect[i]);
    GenGridEachAngleAngleOverlap(i, f32Rmax,
        f32NormalizeYOffset, stSpliLineAngleVect,
        s32Quarder,
        &stGridVect,
        &stWeightVect);
    stGridsVect->push_back(stGridVect);
    stWeights->push_back(stWeightVect);
    }
    return;
}
SV_VOID InnerSV_CameraMeshClass::ModifyOverlapAngle(const SV_S32& s32StartAngle, const SV_S32& s32NoOverLapAngleStart, const SV_F32 &f32AngleStep) {
DLOG(INFO) << __FUNCTION__;
stMeshGenParams.stBowlGrid.f32OverLayAngle = std::min(s32NoOverLapAngleStart*f32AngleStep, stMeshGenParams.stBowlGrid.f32OverLayAngle);
stMeshGenParams.stBowlGrid.f32OverLayAngle = std::max(s32StartAngle*f32AngleStep, stMeshGenParams.stBowlGrid.f32OverLayAngle);
}

SV_POINT3F32_S InnerSV_CameraMeshClass::CalcSplitLine(SV_VOID) {
SV_F32 f32A = tan(gf32HalfPi- stMeshGenParams.stBowlGrid.f32OverLayAngle);
SV_F32 f32Xr = stMeshGenParams.stNormalizeVehicleSize.f32Width, \
        f32Yu = stMeshGenParams.stNormalizeVehicleSize.f32Height;
return {f32A, -1.0, f32A*f32Yu-f32Xr};
}

SV_VOID InnerSV_CameraMeshClass::GenGridsFBChannl(const SV_S32& s32StartAngle, const SV_S32& s32NoOverLapAngleStart,
    const SV_F32& f32AngleStep,
    std::vector <std::vector<SV_MESH_BOWLPOINT_S> >* stGridsVect,
    std::vector< std::vector<SV_POINT2F32_S> >* stWeights)
{
ModifyOverlapAngle(s32StartAngle, s32NoOverLapAngleStart, f32AngleStep);
std::vector<SV_POINT2F32_S> AngleOfSplitLineVect;
CalcAngleOfSplitLineForRanges(s32StartAngle, s32NoOverLapAngleStart, stMeshGenParams.stNormalizeVehicleSize.f32Height, &AngleOfSplitLineVect);
  //一像限重叠区A
SV_S32 s32Start = s32StartAngle, s32End = s32NoOverLapAngleStart;
GenGridsOverlapOfRegion(s32Start, s32End, AngleOfSplitLineVect, {0.0, 0.0, 0.0}, SV_ENUM_OVERLAP_REGION_A, stGridsVect, stWeights);
  //非重叠区
s32Start = s32End, s32End = stMeshGenParams.stBowlGrid.s32Angles-s32End+1;
GenGridsNoOverLay(s32Start, s32End, stGridsVect, stWeights);
  //二象限重叠区A
s32Start = s32End, s32End = stMeshGenParams.stBowlGrid.s32Angles-s32StartAngle;
GenGridsOverlapOfRegion(s32Start, s32End, AngleOfSplitLineVect, {0.0, 0.0, 0.0}, SV_ENUM_OVERLAP_REGION_A, stGridsVect, stWeights);
return;
}

SV_S32 InnerSV_CameraMeshClass::CalcAngleBetweenOverlapRegionAB(const SV_S32& s32StartAngle, const SV_F32 &f32AngleStep) {
DLOG(INFO) << __FUNCTION__;
SV_F32 f32Beta = (0.5*stMeshGenParams.stBowlGrid.s32Angles+s32StartAngle)*f32AngleStep;
    f32Beta+=stMeshGenParams.stBowlGrid.f32OverLayAngle;
    SV_F32 f32SinVal = asin(stMeshGenParams.f32VehicleDialog*sin(f32Beta)/f32RBowl);
    SV_F32 f32X = f32SinVal+stMeshGenParams.stBowlGrid.f32OverLayAngle;
    SV_S32 s32AngleAB = static_cast<SV_S32>((f32X)*gf32InvPi*stMeshGenParams.stBowlGrid.s32Angles);
    return static_cast<SV_S32>(0.5*stMeshGenParams.stBowlGrid.s32Angles)-s32AngleAB;
}

SV_VOID InnerSV_CameraMeshClass::GenGridsLRChannl(const SV_S32& s32StartAngle, const SV_S32& s32NoOverLapAngleStart,
    const SV_F32 &f32AngleStep,
    std::vector <std::vector<SV_MESH_BOWLPOINT_S> >* stGridsVect,
    std::vector< std::vector<SV_POINT2F32_S> >* stWeights)
{
    DLOG(INFO) << __FUNCTION__;
    ModifyOverlapAngle(s32StartAngle, s32NoOverLapAngleStart, f32AngleStep);
    SV_POINT3F32_S stSplitLine = CalcSplitLine();
   //计算各弧长同心圆与分割线交点弧度
    std::vector<SV_POINT2F32_S> AngleOfSplitLineVect;
    SV_S32 s32HalfAngles = stMeshGenParams.stBowlGrid.s32Angles*0.5;
    CalcAngleOfSplitLineForRanges(s32HalfAngles-s32NoOverLapAngleStart, s32HalfAngles-s32StartAngle, stMeshGenParams.stNormalizeVehicleSize.f32Height, &AngleOfSplitLineVect);
   //计算交叠区A与交叠区之间的过渡弧度值
    SV_S32 s32AngleAB = CalcAngleBetweenOverlapRegionAB(s32StartAngle, f32AngleStep);
    SV_S32 s32Start = s32StartAngle, s32End = s32AngleAB;//一象限交叠区B
    GenGridsOverlapOfRegion(s32Start, s32End, AngleOfSplitLineVect, stSplitLine, SV_ENUM_OVERLAP_REGION_B, stGridsVect, stWeights);
    s32Start = s32End , s32End = s32NoOverLapAngleStart;//一象限交叠区A
    GenGridsOverlapOfRegion(s32Start, s32End, AngleOfSplitLineVect, stSplitLine, SV_ENUM_OVERLAP_REGION_A, stGridsVect, stWeights);
    s32Start = s32End, s32End = stMeshGenParams.stBowlGrid.s32Angles-s32End+1;//非交叠区
    GenGridsNoOverLay(s32Start, s32End, stGridsVect, stWeights);
    s32Start = s32End, s32End =  stMeshGenParams.stBowlGrid.s32Angles-s32AngleAB;//二象限交叠区A
    GenGridsOverlapOfRegion(s32Start, s32End, AngleOfSplitLineVect, stSplitLine, SV_ENUM_OVERLAP_REGION_A, stGridsVect, stWeights);
    s32Start = s32End, s32End = stMeshGenParams.stBowlGrid.s32Angles-s32StartAngle;
    GenGridsOverlapOfRegion(s32Start, s32End, AngleOfSplitLineVect, stSplitLine, SV_ENUM_OVERLAP_REGION_B, stGridsVect, stWeights);
    return;
}


SV_VOID InnerSV_CameraMeshClass::GenGrids(std::vector <std::vector<SV_MESH_BOWLPOINT_S> >* stGridsVect,
    std::vector< std::vector<SV_POINT2F32_S> >* stWeightsVect)
{
DLOG(INFO) << __FUNCTION__;
stWeightsVect->clear();
stGridsVect->clear();
SV_F32 f32Angles = static_cast<SV_F32>(stMeshGenParams.stBowlGrid.s32Angles);
SV_F32 f32Xr = stMeshGenParams.stNormalizeVehicleSize.f32Width, \
        f32Yu = stMeshGenParams.stNormalizeVehicleSize.f32Height;
SV_S32 s32StartAngle = static_cast<SV_S32>(f32Angles*(stMeshGenParams.s32CameraChannl >= SV_ENUM_CAMERA_FRONT? \
        (asin(f32Yu/f32RBowl)*gf32InvPi):(atan(f32Xr/f32Yu)*gf32InvPi)));
SV_S32 s32NoOverlapAngle = static_cast<SV_S32>(f32Angles*(stMeshGenParams.s32CameraChannl >= SV_ENUM_CAMERA_FRONT? \
        (atan(f32Yu/f32Xr)*gf32InvPi):(acos(f32Yu/f32RBowl)*gf32InvPi)));
SV_F32 f32AngleStep = CV_PI/f32Angles;
if (stMeshGenParams.s32CameraChannl >= SV_ENUM_CAMERA_FRONT)
{//前后通道
    GenGridsFBChannl(s32StartAngle, s32NoOverlapAngle, f32AngleStep, stGridsVect, stWeightsVect);
}
else
{//左右通道
    GenGridsLRChannl(s32StartAngle, s32NoOverlapAngle, f32AngleStep, stGridsVect, stWeightsVect);
}
return;
}

SV_VOID InnerSV_CameraMeshClass::ProjectCameraTexCoord(const std::vector <std::vector<SV_MESH_BOWLPOINT_S> >& stBowlPointVect, \
    const std::vector< std::vector<SV_POINT2F32_S> > &stWeightVect,
    const SV_CAMERA_PARAMS_S& stCameraParam, \
    std::vector< std::vector<SV_MESH_BOWLPOINT_S> >* stImagePointVect)
{
//stBowlPointVect与stWeightVect需由相同的Size
CHECK(stBowlPointVect.size() == stWeightVect.size());
cv::Mat mR = _local::GetMatRvecOrTvecFromSvCameraParameters(stCameraParam, _local::SV_ENUM_PARAM_RVECT);
cv::Mat mT = _local::GetMatRvecOrTvecFromSvCameraParameters(stCameraParam, _local::SV_ENUM_PARAM_TVECT);
cv::Mat mK = _local::GetMatKFromSvCameraParameters(stCameraParam);
cv::Size stImgSize(stCameraParam.stImageSize.s32Width, stCameraParam.stImageSize.s32Height);
for (SV_S32 i = 0;i < stBowlPointVect.size();++i)

{
    std::vector<SV_MESH_BOWLPOINT_S> stTmpVect;
    _local::GenCameraGrid(stBowlPointVect[i], stWeightVect[i], mR, mT, mK, stImgSize, &stTmpVect);
    stImagePointVect->push_back(stTmpVect);
}
return;
}


SV_VOID InnerSV_CameraMeshClass::AngeleGridToMesh(const SV_SIZE_S &stCameraFrameSize,
    const std::vector<SV_MESH_BOWLPOINT_S> &stP3dfVect, const std::vector<SV_MESH_BOWLPOINT_S> &stP2dVect,
    std::vector<SV_MESH_S> * stMeshVects)
{
SV_S32 s32um = stP3dfVect.size()-1;
//  SV_F32 f32Xnorm = 1.0/static_cast<SV_F32>(stCameraFrameSize.s32Width), \
      //f32Ynorm = 1.0/static_cast<SV_F32>(stCameraFrameSize.s32Height);
SV_POINT3F32_S stNorm = {1.0f/static_cast<SV_F32>(stCameraFrameSize.s32Width), \
        1.0f/static_cast<SV_F32>(stCameraFrameSize.s32Height), 1.0f};
if (stP3dfVect.size() < 4)
    return;
for (SV_S32 i = 0;i < s32um;++i)
{
    //  3   4
    // 1  2
    if (_local::bIsImagePointAvalid(stP2dVect[i].stPoint_s, stCameraFrameSize) && \
        _local::bIsImagePointAvalid(stP2dVect[i+1].stPoint_e, stCameraFrameSize))
    {
        SV_POINT3F32_S stCoord2 = _local::NormarizePoint3F(stP2dVect[i].stPoint_s, stNorm) , \
                    stCoord3 = _local::NormarizePoint3F(stP2dVect[i+1].stPoint_e, stNorm) , \
                    stVertex2 =  _local::stRotatePoint(stP3dfVect[i].stPoint_s, stMeshGenParams.s32CameraChannl) , \
                    stVertex3 =  _local::stRotatePoint(stP3dfVect[i+1].stPoint_e, stMeshGenParams.s32CameraChannl) ;
      //1 2 3 构成三角型1
        if (_local::bIsImagePointAvalid(stP2dVect[i].stPoint_e, stCameraFrameSize))
        {
        SV_POINT3F32_S stCoord1 = _local::NormarizePoint3F(stP2dVect[i].stPoint_e, stNorm), \
            stVertex1 = _local::stRotatePoint(stP3dfVect[i].stPoint_e, stMeshGenParams.s32CameraChannl);
        stMeshVects->push_back({stVertex1, stCoord1});
        stMeshVects->push_back({stVertex2, stCoord2});
        stMeshVects->push_back({stVertex3, stCoord3});
        }
      //2 4 3 构成三角型2
        if (_local::bIsImagePointAvalid(stP2dVect[i+1].stPoint_s, stCameraFrameSize))
        {
        SV_POINT3F32_S stCoord4 = _local::NormarizePoint3F(stP2dVect[i+1].stPoint_s, stNorm), \
            stVertex4 = _local::stRotatePoint(stP3dfVect[i+1].stPoint_s, stMeshGenParams.s32CameraChannl);
        stMeshVects->push_back({stVertex2, stCoord2});
        stMeshVects->push_back({stVertex4, stCoord4});
        stMeshVects->push_back({stVertex3, stCoord3});
        }
    }
}//end loop
return;
}

SV_VOID InnerSV_CameraMeshClass::GridToMesh(const SV_SIZE_S &stCameraFrameSize,
    const std::vector< std::vector<SV_MESH_BOWLPOINT_S> > &stP3dVects,
    const std::vector< std::vector<SV_MESH_BOWLPOINT_S> > &stP2dVects,
    std::vector<SV_MESH_S> *stMeshVects) {
uint i;
stMeshVects->clear();
for (i = 0;i < stP3dVects.size();++i)
{
    AngeleGridToMesh(stCameraFrameSize, stP3dVects[i], stP2dVects[i], stMeshVects);
}
}
namespace _local {
static cv::Mat GetMatRvecOrTvecFromSvCameraParameters(const SV_CAMERA_PARAMS_S &stCameraParam, const SV_S32& s32RvecOrTvec)
{
cv::Mat mTmp = SV_ENUM_PARAM_RVECT == s32RvecOrTvec? cv::Mat(1, sizeof(stCameraParam.af64CameraRotateVect)/sizeof(stCameraParam.af64CameraRotateVect[0]), \
        sizeof(stCameraParam.af64CameraRotateVect[0]) == 8?CV_64FC1:CV_32FC1, const_cast<SV_F64*>(&stCameraParam.af64CameraRotateVect[0])).clone():
        cv::Mat(1, sizeof(stCameraParam.af64CameraTranslateVect)/sizeof(stCameraParam.af64CameraTranslateVect[0]), \
        sizeof(stCameraParam.af64CameraTranslateVect[0]) == 8?CV_64FC1:CV_32FC1, const_cast<SV_F64*>(&stCameraParam.af64CameraTranslateVect[0])).clone();
return mTmp;
}

static cv::Mat GetMatKFromSvCameraParameters(const SV_CAMERA_PARAMS_S &stCameraParam)
{
cv::Mat mTmp = cv::Mat(3, 3, sizeof(stCameraParam.af64CameraK[0]) == 8?CV_64FC1:CV_32FC1, (void*)&stCameraParam.af64CameraK[0]).clone();
return mTmp;
}

static SV_S32 CalcGridBowlHeightEachChannl(const SV_CAMERA_PARAMS_S &stCameraParam,
    const SV_BOWL_GRID_PARAM_S& stGridParam, const SV_F32 &f32VehiclehalfDialog)
{
SV_F32 f32Radius = stGridParam.f32GroundRadiusScal*f32VehiclehalfDialog;
SV_BOOL bNextPoint = SV_TRUE;
SV_S32 S32Mid = 1;
cv::Mat mRvec = GetMatRvecOrTvecFromSvCameraParameters(stCameraParam, SV_ENUM_PARAM_RVECT);
cv::Mat mTvec = GetMatRvecOrTvecFromSvCameraParameters(stCameraParam, SV_ENUM_PARAM_TVECT);
cv::Mat mK = GetMatKFromSvCameraParameters(stCameraParam);
while ((bNextPoint) && (S32Mid < stGridParam.s32NopZ))
{
    std::vector<cv::Point3f> stP3dVect;
    std::vector<cv::Point2f> stP2dVect;
    SV_F32 f32Step = stGridParam.f32StepX*S32Mid;

    stP3dVect.push_back(cv::Point3f(0, f32Radius+f32Step, pow(f32Step, 2))); // Get num point for 3D template
    cv::fisheye::projectPoints(stP3dVect, stP2dVect, mRvec, mTvec,
        mK, cv::Mat::zeros(1, 4, CV_64F));//Project the point into 2D image
    if (stP2dVect[0].y<0 || stP2dVect[0].y >= stCameraParam.stImageSize.s32Height ||  \
        stP2dVect[0].x <0 || stP2dVect[0].x >= stCameraParam.stImageSize.s32Width)
        bNextPoint = SV_FALSE;
    S32Mid++;
}
return std::min(S32Mid-1, stGridParam.s32NopZ);
}

static SV_POINT2F32_S CalcSplitLineAnglesEachRforFBChannl(const SV_F32 &f32InvR, const SV_F32& f32Beta,
    const SV_F32 &f32HalfVehicleDialo, const SV_F32 &f32OverlayAngle, const SV_F32& f32Yoffset,
    const SV_F32 f32AngeStep) {
  /*********************************************
   * a/sin(A)=b/sin(B)=c/sin(C)angles
   *******************************************/
SV_F32 f32A = asin(f32HalfVehicleDialo*sin(f32Beta)*f32InvR);
SV_F32 f32X = f32A+f32OverlayAngle;
SV_POINT2F32_S stP = {asin(f32Yoffset*f32InvR)*f32AngeStep, f32X*f32AngeStep};
return stP;
}

static SV_POINT2F32_S CalcSplitLineAnglesEachRforLRChannl(const SV_F32 &f32InvR, const SV_F32& f32Beta,
    const SV_F32 &f32HalfVehicleDialo, const SV_F32 &f32OverlayAngle, const SV_F32& f32Yoffset,
    const SV_F32 f32AngeStep) {
  /*********************************************
   * a/sin(A)=b/sin(B)=c/sin(C)angles
   *******************************************/
SV_F32 f32A = asin(f32HalfVehicleDialo*sin(f32Beta)*f32InvR);;
SV_F32 f32X = f32A+f32OverlayAngle;
SV_POINT2F32_S stP = {(gf32HalfPi-f32X)*f32AngeStep, (gf32HalfPi-asin(f32Yoffset*f32InvR))*f32AngeStep};
return stP;
}

static SV_POINT2F32_S CalcMixWeightEachRange(const std::vector<SV_POINT2F32_S> & stSpliLineAngleVect,
    const SV_S32 &s32Angle, const SV_S32& s32GridAngles,
    const SV_S32 &s32Quarter, const SV_S32 &s32Range){
SV_POINT2F32_S stWeight = {1.0, 1.0};
if (s32Range > 0) {
    SV_S32 s32Start = s32Angle-1;
    SV_S32 s32Tmp_s = SV_ENUM_QUARTER_1 == s32Quarter?s32Start-stSpliLineAngleVect[s32Range].f32X: \
        s32GridAngles-s32Start-stSpliLineAngleVect[s32Range].f32X;
    SV_S32 s32Tmp_e = SV_ENUM_QUARTER_1 == s32Quarter?s32Angle-stSpliLineAngleVect[s32Range].f32X: \
        s32GridAngles-s32Angle-stSpliLineAngleVect[s32Range].f32X;
    SV_F32 f32Tmp = (stSpliLineAngleVect[s32Range].f32Y-stSpliLineAngleVect[s32Range].f32X);
        f32Tmp = fabs((0.0 == f32Tmp?0.0:1.0f/f32Tmp));
    stWeight.f32X = std::min(1.0, pow(static_cast<SV_F32>(std::max(s32Tmp_e, 0))*f32Tmp, 0.5));
    stWeight.f32Y = std::min(1.0, pow(static_cast<SV_F32>(std::max(s32Tmp_s, 0))*f32Tmp, 0.5));
}
return stWeight;
}

static SV_POINT3F32_S stRotatePoint(const SV_POINT3F32_S &stIn, const SV_S32& s32CameraChannl)
{
SV_POINT3F32_S stRet;
switch (s32CameraChannl)
{
    case SV_ENUM_CAMERA_RIGHT:
        stRet = stIn;
        break;
    case SV_ENUM_CAMERA_LEFT: {
        stRet.f32X = -stIn.f32X;
        stRet.f32Y = -stIn.f32Y;
        stRet.f32Z = stIn.f32Z;
        break;
    }
    case SV_ENUM_CAMERA_BACK: {
        stRet.f32X = stIn.f32Y;
        stRet.f32Y = -stIn.f32X;
        stRet.f32Z = stIn.f32Z;
        break;
    }
    case SV_ENUM_CAMERA_FRONT: {
        stRet.f32X = -stIn.f32Y;
        stRet.f32Y = stIn.f32X;
        stRet.f32Z = stIn.f32Z;
        break;
    }
}
return stRet;
}
static SV_BOOL bIsImagePointAvalid(const SV_POINT3F32_S &stIn, const SV_SIZE_S& stImageSize) {
return stIn.f32X >= 0 && stIn.f32X < stImageSize.s32Width && stIn.f32Y >= 0 && stIn.f32Y < stImageSize.s32Height;
}
static SV_POINT3F32_S NormarizePoint3F(const SV_POINT3F32_S& stPoint, const SV_POINT3F32_S& stNorm)
{
return {stPoint.f32X*stNorm.f32X, stPoint.f32Y*stNorm.f32Y, stPoint.f32Z*stNorm.f32Z};
}

static SV_VOID GenCameraGrid(const std::vector<SV_MESH_BOWLPOINT_S>& stBowlPointVect, \
    const std::vector< SV_POINT2F32_S> &stWeightVect,
    const cv::Mat& mR, const cv::Mat& mT, const cv::Mat& mK, const cv::Size &stimgSize, \
    std::vector<SV_MESH_BOWLPOINT_S>* stImagePointVect)
{
  //stBowlPointVect与stWeightVect需由相同的Size
    CHECK(stBowlPointVect.size() == stWeightVect.size());
    //SV_MESH_BOWLPOINT_S结构体向量转cv::Point3f结构体向量
    cv::Point3f *pstP3d = reinterpret_cast<cv::Point3f*>(const_cast<SV_MESH_BOWLPOINT_S*>(&stBowlPointVect[0]));
    std::vector<cv::Point3f> stP3dVect(pstP3d, pstP3d+2*stBowlPointVect.size());
    std::vector<cv::Point2f> stP2dVect;
    cv::fisheye::projectPoints(stP3dVect, stP2dVect, mR, mT, mK, cv::Mat::zeros(1, 4, CV_64FC1));
    //将获取的像素坐标点及权重值一起导出
    SV_S64 s32Num = stWeightVect.size();
    CHECK(stP2dVect.size() == 2*s32Num);
    SV_POINT2F32_S stFirstAvalid = {-1.0, -1.0};
    //找到当前弧度第一个Avalid的点,用于剔除某些计算得到的无效点
    for (SV_S32 i = 0;i < stP2dVect.size();++i)
    {
        if (stP2dVect[i].x >= 0 && stP2dVect[i].x < stimgSize.width && stP2dVect[i].y >= 0 && stP2dVect[i].y < stimgSize.height)
        {
        stFirstAvalid.f32X = stP2dVect[i].x;
        stFirstAvalid.f32Y = stP2dVect[i].y;
        break;
        }
    }
    SV_F32 f32YValid = stFirstAvalid.f32Y+5;
    for (SV_S32 i = 0;i < s32Num;i++)
    {
        SV_S32 s32Dex = i*2;
        SV_POINT3F32_S stPoint_e = {stP2dVect[s32Dex].x, stP2dVect[s32Dex].y >= f32YValid?-1.0:stP2dVect[s32Dex].y , stWeightVect[i].f32X};
        SV_POINT3F32_S stPoint_s = {stP2dVect[s32Dex+1].x, stP2dVect[s32Dex+1].y >= f32YValid?-1.0:stP2dVect[s32Dex+1].y, stWeightVect[i].f32Y};

        SV_MESH_BOWLPOINT_S stMesh = {stPoint_e, stPoint_s};
        stImagePointVect->push_back(stMesh);
    }
    return;
}

#ifdef STONKAM_DEBUG
static SV_VOID DrawCameraGrid(const std::vector <std::vector<SV_MESH_BOWLPOINT_S> > &stGridsVect,
    const std::vector< std::vector<SV_POINT2F32_S> > &stWeightsVect, const SV_F32 &f32MaxRange,
    const SV_S32& s32Ch) {
SV_S32 s32Wh = f32MaxRange*100;
int wW = 2*s32Wh;
    cv::Mat img_e = cv::Mat::zeros(s32Wh, wW, CV_8UC3);
    cv::Mat img_s = cv::Mat::zeros(s32Wh, wW, CV_8UC3);
    for (SV_S32 i = 0;i < stGridsVect.size();i++)
    {
        for (SV_S32 j = 0;j < stGridsVect[i].size();++j)
        {
        SV_F64 f64lpha = stWeightsVect[i][j].f32X;
        cv::circle(img_e, cv::Point((stGridsVect[i][j].stPoint_e.f32X+f32MaxRange)*100, s32Wh-stGridsVect[i][j].stPoint_e.f32Y*100), \
            1, cv::Scalar(255.0*f64lpha, 255.0*f64lpha, 0, 0));
        cv::circle(img_s, cv::Point((stGridsVect[i][j].stPoint_s.f32X+f32MaxRange)*100, s32Wh-stGridsVect[i][j].stPoint_s.f32Y*100), \
                1, cv::Scalar(255.0, 255.0, 0, 0));
        }
    }
    std::string s8NodeStr_temp = "GridEnd";
    SV_S8 s8ChannlStr_temp = '0'+s32Ch;
    s8NodeStr_temp+=s8ChannlStr_temp;
    s8NodeStr_temp+=".jpg";
    std::string s8NodeStr_temp_s = "GridStart";
    s8NodeStr_temp_s+=s8ChannlStr_temp;
    s8NodeStr_temp_s+=".jpg";
    imwrite(s8NodeStr_temp.c_str(), img_e);
    imwrite(s8NodeStr_temp_s.c_str(), img_s);
}

static SV_VOID DrawCameraMesh(const std::vector <std::vector<SV_MESH_BOWLPOINT_S> > &stImageMeshVect, const SV_S32& s32Ch) {
cv::Mat img_e = cv::Mat::zeros(720, 1280, CV_8UC3);
cv::Mat img_s =  cv::Mat::zeros(720, 1280, CV_8UC3);
for (SV_S32 i = 0;i < stImageMeshVect.size();++i) {
    for (SV_S32 j = 0;j < stImageMeshVect[i].size();++j) {
        SV_POINT3F32_S stEnd = stImageMeshVect[i][j].stPoint_e;
        SV_POINT3F32_S stStart = stImageMeshVect[i][j].stPoint_s;
        if (stEnd.f32X >= 0 && stEnd.f32X < 1280&&stEnd.f32Y >= 0 &&stEnd.f32Y < 720) {
        cv::circle(img_e, cv::Point(stEnd.f32X, stEnd.f32Y), \
            1, cv::Scalar(255.0*stEnd.f32Z, 0, 0, 0));
        }
        if (stStart.f32X >= 0 && stStart.f32X < 1280&&stStart.f32Y >= 0 &&stStart.f32Y < 720) {
        cv::circle(img_s, cv::Point(stStart.f32X, stStart.f32Y), \
            1, cv::Scalar(255.0*stStart.f32Z, 0, 0, 0));
        }
    }
}
std::string s8NodeStr_temp = "CameraGridEnd";
SV_S8 s8ChannlStr_temp = '0'+s32Ch;
s8NodeStr_temp+=s8ChannlStr_temp;
s8NodeStr_temp+=".jpg";
std::string s8NodeStr_temp_s = "CameraGridStart";
s8NodeStr_temp_s+=s8ChannlStr_temp;
s8NodeStr_temp_s+=".jpg";
imwrite(s8NodeStr_temp.c_str(), img_e);
imwrite(s8NodeStr_temp_s.c_str(), img_s);
}
#endif

}  // namespace _local

}  // namespace mesh
}  // namespace camera
}  // namespace svrender
}  // namespace sv_avm
}  // namespace sm

