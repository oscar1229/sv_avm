/*
 * svmcalibrate.cpp
 *
 *有关标定操作相关的局部函数及功能借口函数定义
 *
 */
#include "include/svmcalibrate.hpp"

#include <stdio.h>

#include <opencv2/opencv.hpp>

#include "src/svmparam/svmparam.hpp"

namespace sm {
namespace sv_avm {
namespace svmcalibrate {

//实际棋盘格标定过程所需的标定所需要的参考坐标点及棋盘格参数
 struct SV_ORIGIN_CHESS_INPUT_S
{
  cv::Size stCenter;//实际提取中间区域棋盘格角点规格
  cv::Size stEdge;//实际提取边缘区域棋盘格角点规格
  cv::Point2f stOrgion;//当前通道参考坐标点
  SV_F64 f64Gsize;//单格棋盘格尺寸
  SV_F64 f64MinOfInvHAndInvW;//车模尺寸中，长宽的倒数之间的最小值
};//end of


 //标定结果结构体
 struct ISV_CALI_RESULT_S {
   //标定结果，可选值为:SV_ENUM_CALI_NOIMAGE，
   //                SV_ENUM_CALI_NOCHESSBOARD，
   //                SV_ENUM_CALI_CALCFAILED，
   //                SV_ENUM_CALI_FAILED，
   //                SV_ENUM_CALI_SUCCEED
   SV_S32 s32Result;
   ////摄像头通道号,可选: SV_ENUM_CAMERA_LEFT =0,
   //                 SV_ENUM_CAMERA_RIGHT,
   //                 SV_ENUM_CAMERA_FRONT,
   //                 SV_ENUM_CAMERA_BACK,
   SV_S32 s32CameraChannl;
   //后续参数只有在标定结果为SV_ENUM_CALI_SUCCEED时才赋值
   SV_CAMERA_PARAMS_S stCameraParam;
   SV_S32 s32CaliMethod;//标定方式，可选值为 SV_ENUM_CALIMETHOD_UCHESSBORD或SV_ENUM_CALIMETHOD_U8POINTS或SV_ENUM_CALIMETHOD_BUTT
   union {
     SV_CALI_UCHESSBOARD_PATERN_S stChessboard;//棋盘格标定模板
     SV_CALI_U8P_PATERN_S stU8Point;//8点式标定模板
   }stCaliPatern;
 };


 //判断输入的stImage是否为空图像，即数据指针为NULL或图像大小为NULL
const static inline SV_BOOL InnerSV_CheckIfSVImageEmpty(const SV_IMAGE_S& stImage);

//@brief 将标定模板及车型尺寸、摄像头通道号转换为各通道实际标定所需要的参考坐标点及棋盘格参数
//@param in stChessBoardPatern 棋盘格标定模板；
//       in stVehicleSiz 车辆尺寸规格参数；
//       in s32CameraCh 摄像头通道可选值为：SV_ENUM_CAMERA_LEFT、SV_ENUM_CAMERA_RIGHT、SV_ENUM_CAMERA_FRONT、SV_ENUM_CAMERA_BACK
//@return SV_GEN_CHESSWORDPOINT_INPUT_S结构体数据
//@remarks 因为实际提取的是黑白格之间交叉点，所以在函数内部对stChessBoardPatern输出的中间区域棋盘格规格及边缘区域棋盘格规格做修正，
//         即width height各-1
const static SV_ORIGIN_CHESS_INPUT_S InnerSV_GnerateOriginChessBoardEach( const SV_CALI_UCHESSBOARD_PATERN_S &stChessBoardPatern,
    const SV_SIZE_S &stVehicleSiz,
    const sm::SV_S32 &s32CameraCh);
//@brief 将输入视图转换为OpenCV mat对象，同时将行色彩变换，输出Mat对象为灰度图
//@param in stImage 输入图像
//       out pGray opencv Mat对象，灰度图像
//@return 输入不支持格式的图像返回SV_FALSE
//@remarks 函数内部通过判断输入SV_IMAGE_S图像stImage的图像类型，调用不同的色彩变换函数，得到灰度图像pGray
//         stImage支持的图像类型包括SV_IMAGE_TYPE_UYVY SV_IMAGE_TYPE_BGR SV_IMAGE_TYPE_BGRA
const static SV_BOOL InnerSV_ImageToGray(const SV_IMAGE_S& stImage,cv::Mat* pGray);
//@brief 提取某一区域棋盘
//@param in clTmp 用于标定的摄像头视图
//       in stChessBoardSize 棋盘格参数
//       in s32Xoffset 提取到棋盘格角点的X坐标偏移量
//       out
//@return 当棋盘格提取失败时返回SV_FALSE
//@remarks 输入参数clTmp必须是灰度图，即单通道视图
//         s32Xoffset，对中间区域与左侧区域，偏移量为0，右侧区域，需增加X坐标偏移量
const static SV_BOOL InnerSV_DetectChessBoardEachRegion(const cv::Mat &clTemp,const cv::Size &stChessBoardSize,
    const SV_F32& s32Xoffset, std::vector< std::vector<cv::Point2f> > *pstImgPointsVect);
//@brief 获取中间区域棋盘格范围，以便后续将视图分割为左侧区域及右侧区域,以便后续提取边缘区域棋盘格
//@param in stCenterPoints 中间区域棋盘格角点
//       out ps32Xmin，存贮中间棋盘格区域左侧边界值的指针
//       out ps32Xmax 存贮中间棋盘格右侧边界的指针
const static SV_VOID InnerSV_stGetCenterRegion(const std::vector< cv::Point2f>& stCenterPoints,SV_S32* ps32Xmin,SV_S32* ps32Xmax);
//@brief 标定布中间区域1块及边缘区域两块棋盘格的提取
//@pram in stImage，输入帧图像数据
//      in stOrigiAndChess 各通道实际标定所需要的参考坐标点及棋盘格参数
//      out pImgPointsVect 提取到的棋盘格角点向量
//@return 提取到2块及2块以上棋盘格时，返回SV_TRUE,否则，返回SV_FALSE
//@remarks 未提取到2块及2块以上棋盘格是，函数内部清空pImgPointsVect向量
//         函数内部处理流程如下：
//           第一步;调用InnerSV_ImageToGray函数将输入的stImage转换为opencv Mat类灰度图
//           第二步：调用InnerSV_DetectChessBoardEachRegion，提取中间区域棋盘格，如提取失败，返回SV_FALSE
//           第三步：调用InnerSV_stGetCenterRegion获取中间棋盘格区域左侧边界X左边及右侧边界x坐标，便于后续将整个视图，分离出左侧边缘棋盘格区域及右侧棋盘格区域
//           第四步：InnerSV_DetectChessBoardEachRegion函数分别提取两个边缘区域棋盘格
//           第五步：判断是否提取到2组及2组以上棋盘格，如失败则返回SV_FALSE
const static SV_BOOL InnerSV_DetectChessborad(const SV_IMAGE_S& stImage,const SV_ORIGIN_CHESS_INPUT_S &stOrigiAndChess,
   std::vector< std::vector<cv::Point2f> > *pImgPointsVect);
//@brief 根据输入，生成各通道中间区域棋盘格棋盘格对应世界坐标点向量
//@param  in stInput，此前步骤调用InnerSV_GenerrateWordPCalcInput函数生成的输入参数
//@param  out pstWordPointsVects 存储生成的世界坐标点向量的指针
//@remarks 原理介绍参照说明文档svmcalibrate.html
const static SV_VOID InnerSV_GenWordPointsFromOriginChessInput(const SV_ORIGIN_CHESS_INPUT_S& stInput,
    std::vector<cv::Point3f>* pstWordPointsVects);

//@brief 生成8点式标定模板对应像素坐标世界坐标点
//@param  in stPatern 8点式标定标定模板
//        in stVehicleSize 车型尺寸参数 长X宽
//        in s32CameraCh摄像头通道
//        out pstWordPointsVects标定模板中间区域棋盘格角点世界坐标向量指针
const static SV_VOID InnerSV_GenChannl8PointWordPs(const SV_CALI_U8P_PATERN_S& stPatern,
    const cv::Size &stVehicleSiz,const SV_S32& s32CameraCh,
    std::vector<cv::Point3f>* pstWordPointVector);
#ifdef STONKAM_DEBUG
//打印用于标定的s32Ch通道每组世界坐标点及像素坐标点
const  static SV_VOID InnerSV_PrintImagePointAndObjectPoints(const std::vector< std::vector<cv::Point2f>> &stImagePointVect,
    const std::vector< std::vector<cv::Point3f> >&stWordPointsVects,const SV_S32 &s32Ch);
#endif
//@brief 将数据类型为CV_64FC1 CV_64FC2 CV_64FC3的Mat转换为SV_F64型数组
//@param in m cv::Mat对象矩阵
//       in s32ArraySize 目标数组的大小
//       out paArr 目标数组头指针
const static SV_VOID InnerSV_F64MatToF64Array(const cv::Mat& m,const SV_S32& s32ArraySize,SV_F64* paArr);
//@brief 使用像素坐标点集于世界坐标点集，计算摄像头内外参的实际计算过程
//@param in stImagePoints 像素坐标点集
//       in stObjectPoints 世界坐标点集
//       in stImgSize 相机图像尺寸
//       in bCheckCond 是否使能CALIB_CHECK_COND标志
//       out 摄像头参数
//@return  标定结果SV_ENUM_CALI_CALCFAILED 或 SV_ENUM_CALI_SUCCEED
//@remarks  1.stImagePoints与stObjectPoints点集的点组数及对应的每组的坐标点数需相等；
//            比如，如果像素坐标点由2组组成，第一组包含24个像素点，第二组包含12个像素点，则世界坐标点也应有两组组成，且第一组24，第二组12
//          2. 像素坐标点为Point2f型，世界坐标点为Point3f型
//          3.函数内部处理流程
//            第一步：读取stImagePoints及stObjectPoints，调用cv::fisheye::calibrate,完成相机单步标定
//            第二步：调用checkRange，判断计算的参数值是否在合理范围内，否则返回SV_ENUM_CALI_CALCFAILED
//            第三步：输出摄像头参数
const static SV_S32 InnerSV_Calibrate_InnerProcess(const cv:: InputArrayOfArrays& stImagePoints,
    const cv:: InputArrayOfArrays& stObjectPoints,const cv::Size& stImgSize,
    const SV_BOOL& bCheckCond, SV_CAMERA_PARAMS_S* stCameraParams);
//@brief 将标定得到的摄像头参数，标定结果及相关的标定输入信息打包导出到SV_CALI_RESULT_S结构体数据
//@param in stCaliInput 标定输入结构体数据
//       in stCameraParam 调用InnerSV_Calibrate_InnerProcess标定得到的摄像头参数
//       in s32Ret 调用InnerSV_Calibrate_InnerProcess的函数返回值
//       out pstCaliResult 标定结果
//@remarks 当前函数内部根据stCaliInput不同的标定方式，保存不同的标定模板
//          函数内部包含pstCaliResult的初始化操作
const static SV_VOID InnerSV_EachCaliParamExportToCaliResult(const SV_CALI_INPUT_S& stCaliInput,
    const SV_CAMERA_PARAMS_S &stCameraParam,SV_S32& s32Ret,SV_CALI_RESULT_S* pstCaliResult);
//@brief 棋盘格方式标定单通道摄像头
//@param  in stCaliInput 标定输入结构体
//        in vehicleSize 车型尺寸参数
//        out pstCaliResult 标定结果
//@remarks 标定流程如下：
//          第一步：调用InnerSV_CheckIfSVImageEmpty判断stCaliInput中stImage分量是否为非空图像
//          第二步，调用InnerSV_GnerateOriginChessBoardEach将stCaliInput中的stChessboard及vehicleSize转换为
//          实际棋盘格标定所需的SV_ORIGIN_CHESS_INPUT_S结构数据
//          第三步 调用InnerSV_DetectChessborad提取当前视图标定布中棋盘格，如果提取到两块以上棋盘格，则进入下一步，否则返回
//          第四步：调用InnerSV_GenWordPointsFromOriginChessInput生成标定布中中间区域棋盘格格点对应世界坐标，用于内外参标定
//          第五步：生成边缘棋盘格格点世界坐标，用于内参标定
//          第六步：利用二三步生成的世界坐标点，生成与提取到的棋盘格集相匹配的世界坐标集
//          第七步：调用InnerSV_Calibrate_InnerProcess单步计算摄像头内外参数，此种方式下使能bCheckCond
//          第八步：调用InnerSV_EachCaliParamExportToCaliResult 初始化stCaliResult，并输出标定结果到stCaliResult
const static SV_VOID InneSV_CalibrateEachChannelUseChessBoardPatern(const SV_CALI_INPUT_S& stCaliInput,
    const SV_SIZE_S& vehicleSize,SV_CALI_RESULT_S* pstCaliResult);
//@brief 将8个世界坐标点及像素坐标点坐标点，平分为 0 1 2  5  6 7 及3 2 1 6 5 4 两组
//@param in stPatern 8点式模板
//       in stWordPointVect 8点式模板世界坐标
//       out pstImagePointsVect 组数为2组的像素做标集
//       out pstObjPointsVect 组数为2组的世界坐标集
//@remarks  8点的排序顺序为
//          0  1      2  3
//          7  6      5   4
const static SV_VOID InnerSV_SplitImagePointsAndWordPointsToTwoPart(const SV_CALI_U8P_PATERN_S &stPatern,
    const std::vector<cv::Point3f> &stWordPointVect,
    std::vector< std::vector<cv::Point2f> > *pstImagePointsVect,
    std::vector< std::vector<cv::Point3f> > *pstObjPointsVect);
//@brief 8点式方式标定单通道摄像头
//@param in stCaliInput 标定输入结构体
//       in vehicleSize 车型尺寸参数
//       out stCaliResult 标定结果
//remarks 标定流程如下：
//        第一步：调用InnerSV_CheckIfSVImageEmpty判断stCaliInput中stImage分量是否为非空图像，否则返回
//        第二步，调用InnerSV_GenChannl8PointWordPs生成8点式模板对应世界坐标点
//        第三步，调用nnerSV_SplitImagePointsAndWordPointsToTwoPart生成组数为2组的像素做标集及组数为2组的世界坐标集
//        第四步：调用InnerSV_Calibrate_InnerProcess单步计算摄像头内外参数，此种方式下不使能bCheckCond
//        第五步：调用InnerSV_EachCaliParamExportToCaliResult 初始化stCaliResult，并输出标定结果到stCaliResult；
const static SV_VOID InnerSV_CalibrateEachChannelUse8PoitPatern(const SV_CALI_INPUT_S& stCaliInput,
    const SV_SIZE_S& vehicleSize,SV_CALI_RESULT_S* pstCaliResult);

static SV_VOID InnerSV_Calibrate(const std::vector<SV_CALI_INPUT_S>& stCaliInVect,const SV_SIZE_S& stVehicleSize,
    std::vector<SV_CALI_RESULT_S>* pstCaliResultVect) 
{
  DLOG(INFO)<<__FUNCTION__;
  SV_S32 s32CameraNumber = stCaliInVect.size();
  for(SV_S32 i=0;i<s32CameraNumber;++i) 
  {
     SV_S32 s32Method = stCaliInVect[i].s32CaliMethod;
     SV_CALI_RESULT_S stResult;
     memset(&stResult,0,sizeof(stResult));
     if(SV_ENUM_CALIMETHOD_UCHESSBORD == s32Method) {
       InneSV_CalibrateEachChannelUseChessBoardPatern(stCaliInVect[i],stVehicleSize,&stResult);
     }
     else if(SV_ENUM_CALIMETHOD_U8POINTS == s32Method) {
       InnerSV_CalibrateEachChannelUse8PoitPatern(stCaliInVect[i],stVehicleSize,&stResult);
     }
     else;
     pstCaliResultVect->push_back(stResult);
  }
  return;
}//end of InnerSV_Calibrate

SV_BOOL SV_Calibrate(const std::vector<SV_CALI_INPUT_S>& stCaliInVect,
    const SV_SIZE_S& stVehicleSize,const SV_S8* s8SavedXmlFile,
    std::vector<SV_CALI_RESULT_S>* pstCaliResultVect) {
  std::vector<svmcalibrate::SV_CALI_RESULT_S> stCaliResult;
  InnerSV_Calibrate(stCaliInVect,stVehicleSize,&stCaliResult);
  svmparam::InnerSV_SvmParamClass  stSvmParam;
  SV_BOOL bNeedChessBoardFlag =SV_TRUE;
  //读取原有文件
  if(svmparam::ISV_ENUM_SUCCEED!=stSvmParam.InnerSV_s32ReadFromXml(s8SavedXmlFile,bNeedChessBoardFlag)) {
    stSvmParam.InnerSV_Init();
  }
  SV_S32 s32Ret =SV_FALSE;
  for(SV_S32 i=0;i<stCaliResult.size();i++) {
    if(SV_ENUM_CALI_SUCCEED==stCaliResult[i].s32Result) {
      s32Ret = SV_TRUE;
      stSvmParam.InnerSV_SetCameraParamEachChannl(i,stCaliResult[i].stCameraParam);
      stSvmParam.InnerSV_s32SetCalibratePartenChannl(stCaliResult[i].s32CaliMethod,i,&stCaliResult[i].stCaliPatern.stChessboard);
    }
  }
  if(SV_TRUE == s32Ret) {
    stSvmParam.InnerSV_SetVehicleSize(stVehicleSize);
  }
  if(svmparam::ISV_ENUM_SUCCEED != stSvmParam.InnerSV_s32WriteToXml(s8SavedXmlFile))
    return SV_FALSE;
  return SV_TRUE;
}

const static SV_BOOL InnerSV_CheckIfSVImageEmpty(const SV_IMAGE_S& stImage) {
  return NULL != stImage.dataPtr && 0!= stImage.stImageSize.s32Width && 0!= stImage.stImageSize.s32Height;
}
const static SV_ORIGIN_CHESS_INPUT_S InnerSV_GnerateOriginChessBoardEach( const SV_CALI_UCHESSBOARD_PATERN_S &stChessBoardPatern,
    const SV_SIZE_S &stVehicleSiz,
    const sm::SV_S32 &s32CameraCh) {
  SV_F64 f64Y = static_cast<SV_F64>(stVehicleSiz.s32Height),f64X =static_cast<SV_F64>(stVehicleSiz.s32Width);
  SV_ORIGIN_CHESS_INPUT_S stOut;
  //实际提取的是黑白格之间交叉点，所以-1
  stOut.f64Gsize = static_cast<SV_F64>(stChessBoardPatern.s32GridSize);
  stOut.f64MinOfInvHAndInvW =2.0/(std::max(f64X,f64Y));
  stOut.stCenter = cv::Size(stChessBoardPatern.stChessBoardCenter.s32Width-1, \
      stChessBoardPatern.stChessBoardCenter.s32Height-1);
  stOut.stEdge = cv::Size(stChessBoardPatern.stChessBoardEdge.s32Width-1, \
      stChessBoardPatern.stChessBoardEdge.s32Height-1);
  if(s32CameraCh < SV_ENUM_CAMERA_FRONT) {
    stOut.stOrgion.x= SV_ENUM_CAMERA_LEFT == s32CameraCh?static_cast<SV_F64>(stChessBoardPatern.s32ChessPosition)-0.5*f64Y: 
        0.5*f64Y-static_cast<SV_F64>(stChessBoardPatern.s32ChessPosition);
    stOut.stOrgion.y = 0.5*f64X;
  }
  else {
    stOut.stOrgion = cv::Point2f(0,0.5*f64Y);
  }
  return stOut;
}

const static SV_BOOL InnerSV_ImageToGray(const SV_IMAGE_S& stImage,cv::Mat* pGray) {
  SV_SIZE_S stSrcSize=stImage.stImageSize;
  if(stImage.s32ImageType == SV_IMAGE_TYPE_UYVY) {
    cv::Mat matSrc(stSrcSize.s32Height,stSrcSize.s32Width,CV_8UC2,stImage.dataPtr);
    cv::cvtColor(matSrc,*pGray,cv::COLOR_YUV2GRAY_I420);
  }
  else if(stImage.s32ImageType == SV_IMAGE_TYPE_BGR) {
      cv::Mat matSrc(stSrcSize.s32Height,stSrcSize.s32Width,CV_8UC3,stImage.dataPtr);
      cv::cvtColor(matSrc,*pGray,cv::COLOR_BGR2GRAY);
    }
    else if(stImage.s32ImageType == SV_IMAGE_TYPE_BGRA) {
      cv::Mat matSrc(stSrcSize.s32Height,stSrcSize.s32Width,CV_8UC4,stImage.dataPtr);
      cv::cvtColor(matSrc,*pGray,cv::COLOR_BGRA2GRAY);
    }
    else {
      LOG(WARNING)<<__FUNCTION__<<":Format Not Support\n";
      return SV_FALSE;
    }
  return SV_TRUE;
}//end of InnerSV_ImageToGray

const static SV_BOOL InnerSV_DetectChessBoardEachRegion(const cv::Mat &clTemp,const cv::Size &stChessBoardSize,
    const SV_F32& s32Xoffset, std::vector< std::vector<cv::Point2f> > *pstImgPointsVect) {
 // std::vector<cv::Point2f> stPointtVec(stChessBoardSize.width*stChessBoardSize.height);//存储区域棋盘格角点
  //提取中间区域棋盘格角点
    cv::Point2f astPoint[stChessBoardSize.width*stChessBoardSize.height];
    std::vector<cv::Point2f> stPointtVec(astPoint,astPoint+stChessBoardSize.width*stChessBoardSize.height);
   SV_BOOL s32Find = cv::findChessboardCorners(clTemp, stChessBoardSize,stPointtVec,
                                              cv:: CALIB_CB_ADAPTIVE_THRESH
                                               +cv::CALIB_CB_FAST_CHECK);
   if(0 != s32Xoffset)
   {
     for(SV_S32 i=0;i<stPointtVec.size();i++)
       stPointtVec[i].x+=static_cast<float>(s32Xoffset);
   }
   if(!s32Find)//提取中间区域棋盘格失败
     return SV_FALSE;
   pstImgPointsVect->push_back(stPointtVec);
   return SV_TRUE;
}//end of InnerSV_DetectChessBoardEachRegion

const static SV_VOID InnerSV_stGetCenterRegion(const std::vector< cv::Point2f>& stCenterPoints,SV_S32* ps32Xmin,SV_S32* ps32Xmax) {
  SV_F32 f32Xmin=FLT_MAX,f32Xmax=FLT_MIN;
  for(SV_S32 i=0;i<stCenterPoints.size();++i) {
    SV_F32 f32Xtemp=static_cast<SV_F32>(stCenterPoints[i].x);
    f32Xmin = f32Xmin>f32Xtemp?f32Xtemp:f32Xmin;
    f32Xmax = f32Xmax<f32Xtemp?f32Xtemp:f32Xmax;
  }
  SV_F32 f32Xmean=0.5*(f32Xmax+f32Xmin);
  //SV_POINT2S32_S stCenterRegion={static_cast<SV_S32>(0.5*(f32Xmin+f32Xmean)),static_cast<SV_S32>(0.5*(f32Xmean+f32Xmax))};
  *ps32Xmin = static_cast<SV_S32>(0.5*(f32Xmin+f32Xmean));
  *ps32Xmax = static_cast<SV_S32>(0.5*(f32Xmean+f32Xmax));
  return;
}//end of InnerSV_stGetCenterRegion

const static SV_BOOL InnerSV_DetectChessborad(const SV_IMAGE_S& stImage,const SV_ORIGIN_CHESS_INPUT_S &stOrigiAndChess,
   std::vector< std::vector<cv::Point2f> > *pImgPointsVect) {
  //将SV_IMAGE_S类型，转换成灰度图
  cv::Mat clGray;
  if(SV_FALSE == InnerSV_ImageToGray(stImage,&clGray)) {
    return SV_FALSE;
  }
  cv::Size stTempSize=stOrigiAndChess.stCenter;
  //提取中间区域棋盘格角点
  if(SV_FALSE ==  InnerSV_DetectChessBoardEachRegion(clGray,stTempSize,0,pImgPointsVect)) {
    return SV_FALSE;
  }
  //获取中间棋盘格区域范围，即起始X坐标与终止X坐标
  std::vector< cv::Point2f> stPointTemp=pImgPointsVect->operator [](0);
  SV_S32 s32Xmin,s32Xmax;
  InnerSV_stGetCenterRegion(stPointTemp,&s32Xmin,&s32Xmax);

  //提取左侧区域棋盘格
  stTempSize = stOrigiAndChess.stEdge;
  InnerSV_DetectChessBoardEachRegion(clGray.colRange(0,s32Xmin),stTempSize,0,pImgPointsVect);
  //提取右侧棋盘格区域，右侧需加偏移量
  InnerSV_DetectChessBoardEachRegion(clGray.colRange(s32Xmax,clGray.cols),stTempSize,s32Xmax,pImgPointsVect);
  //提取到2块及2块以上棋盘格，清空pImgPointsVect
  if(2>pImgPointsVect->size()) {
    DLOG(INFO)<<"2>pImgPointsVect->size";
    pImgPointsVect->clear();
    return SV_FALSE;
  }//end of if
  DLOG(INFO)<<__FUNCTION__<<":detect CornerGroup="<<pImgPointsVect->size();
  return SV_TRUE;
}//end of DetectChessborad

//根据输入，生成各通道中间区域棋盘格棋盘格对应世界坐标点向量
const static SV_VOID InnerSV_GenWordPointsFromOriginChessInput(const SV_ORIGIN_CHESS_INPUT_S& stInput,
    std::vector<cv::Point3f>* pstWordPointsVects) {
  SV_F64 f64HalfGsize=0.5*stInput.f64Gsize;
 cv::Size stTempSize=stInput.stCenter;
  cv::Point3f stLeftTop(stInput.stOrgion.x-(static_cast<SV_F64>(stTempSize.width-1)*f64HalfGsize),
      stInput.stOrgion.y+(static_cast<SV_F64>(stTempSize.height)+0.5)*stInput.f64Gsize,
      0.0);//提取的棋盘格最左上角点对应的世界坐标
  SV_S32 s32CornerCounts = stTempSize.width*stTempSize.height;
  //计算各坐标点
  for(SV_S32 i=0;i<s32CornerCounts;++i) {
    SV_F64 f64Yoffset = -(i/stTempSize.width)*stInput.f64Gsize,
        f64Xoffset = (i%stTempSize.width)*stInput.f64Gsize;//计算各角点XY坐标偏移量
    cv::Point3f stWord((stLeftTop.x+f64Xoffset)*stInput.f64MinOfInvHAndInvW,
        (stLeftTop.y+f64Yoffset)*stInput.f64MinOfInvHAndInvW,
        0.0);//计算并归一化各角点世界坐标
    pstWordPointsVects->push_back(stWord);
  }//end of loop
  return;
}//end of InnerSV_GenWordPointsFromInput


const static SV_VOID InnerSV_GenChannl8PointWordPs(const SV_CALI_U8P_PATERN_S& stPatern,
    const cv::Size &stVehicleSiz,const SV_S32& s32CameraCh,
    std::vector<cv::Point3f>* pstWordPointVector) 
{
  SV_F64 f64Y = static_cast<SV_F64>(stVehicleSiz.height)*0.5,
      f64X =static_cast<SV_F64>(stVehicleSiz.width)*0.5;
  SV_F64 f64InvMaxXY=1.0/(std::max(f64X,f64Y));
  SV_F64 f64Right,f64Left,f64Top,f64Bottom;
  if(s32CameraCh<SV_ENUM_CAMERA_FRONT) {
    f64Right = (f64Y+static_cast<SV_F64>(stPatern.s3U8PaternBoardSize))*f64InvMaxXY;
    f64Left = f64Y*f64InvMaxXY;
    f64Top = (f64X+static_cast<SV_F64>(stPatern.s3U8PaternBoardSize))*f64InvMaxXY;
    f64Bottom = f64X*f64InvMaxXY;
  }
  else {
    f64Right = (f64X+static_cast<SV_F64>(stPatern.s3U8PaternBoardSize))*f64InvMaxXY;
    f64Left = f64X*f64InvMaxXY;
    f64Top =  (f64Y+static_cast<SV_F64>(stPatern.s3U8PaternBoardSize))*f64InvMaxXY;
    f64Bottom = f64Y*f64InvMaxXY;
  }
  pstWordPointVector->push_back(cv::Point3f(-f64Right,f64Top,0));
  pstWordPointVector->push_back(cv::Point3f(-f64Left,f64Top,0));
  pstWordPointVector->push_back(cv::Point3f(f64Left,f64Top,0));
  pstWordPointVector->push_back(cv::Point3f(f64Right,f64Top,0));
  pstWordPointVector->push_back(cv::Point3f(f64Right,f64Top,0));
  pstWordPointVector->push_back(cv::Point3f(f64Left,f64Bottom,0));
  pstWordPointVector->push_back(cv::Point3f(-f64Left,f64Top,0));
  pstWordPointVector->push_back(cv::Point3f(-f64Right,f64Top,0));
  return;
}//end of InnerSV_GenChannl8PointWordPs


//将数据类型为CV_64FC1的Mat转换为SV_F64型数组
const static SV_VOID InnerSV_F64MatToF64Array(const cv::Mat& m,const SV_S32& s32ArraySize,SV_F64* paArr) {
 if (m.type() == CV_64FC1) {
   memcpy(paArr,m.ptr<SV_F64>(0),std::min(m.cols*m.rows,s32ArraySize)*sizeof(m.ptr<SV_F64>(0)[0]));
 }
 else if(m.type() == CV_64FC2) {
   memcpy(paArr,m.ptr<SV_F64>(0),std::min(m.cols*m.rows*2,s32ArraySize)*sizeof(m.ptr<SV_F64>(0)[0]));
 }
 else if(m.type() == CV_64FC3) {
   memcpy(paArr,m.ptr<SV_F64>(0),std::min(m.cols*m.rows*3,s32ArraySize)*sizeof(m.ptr<SV_F64>(0)[0]));
 }
 else {
   exit(-1);
 }
}//end of InnerSV_F64MatToF64Array

const static SV_S32 InnerSV_Calibrate_InnerProcess(const cv:: InputArrayOfArrays& stImagePoints,
    const cv:: InputArrayOfArrays& stObjectPoints,const cv::Size& stImgSize,
    const SV_BOOL& bCheckCond, SV_CAMERA_PARAMS_S* stCameraParams) {
  cv::Mat matK(3,3,CV_64F);
  cv::Mat mDistCoeffs=cv::Mat::zeros(1,4,CV_64F);
  cv::Mat _rvecs, _tvecs;
  cv::fisheye::calibrate(stObjectPoints, stImagePoints, stImgSize,
                                matK, mDistCoeffs, _rvecs, _tvecs,
                                 (cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC)+
                                 (SV_TRUE==bCheckCond?cv::fisheye::CALIB_CHECK_COND:0)+
                                 (cv::fisheye::CALIB_FIX_SKEW)+
                                 (cv::fisheye::CALIB_FIX_K1)+
                                 (cv::fisheye::CALIB_FIX_K2)+
                                 (cv::fisheye::CALIB_FIX_K3)+
                                 (cv::fisheye::CALIB_FIX_K4));
  if(SV_FALSE==checkRange(matK,true,0,0,1920) && checkRange(_tvecs.row(0),true,0,-10,10)&& checkRange(_rvecs.row(0),true,0,-10,10)) {
    return SV_ENUM_CALI_CALCFAILED;
  }
  stCameraParams->stImageSize={stImgSize.width,stImgSize.height};
  InnerSV_F64MatToF64Array(matK,sizeof(stCameraParams->af64CameraK)/sizeof(stCameraParams->af64CameraK[0]),stCameraParams->af64CameraK);
  InnerSV_F64MatToF64Array(mDistCoeffs,sizeof(stCameraParams->af64CameraDistort)/sizeof(stCameraParams->af64CameraDistort[0]),stCameraParams->af64CameraDistort);
  InnerSV_F64MatToF64Array(_rvecs.row(0),sizeof(stCameraParams->af64CameraRotateVect)/sizeof(stCameraParams->af64CameraRotateVect[0]),stCameraParams->af64CameraRotateVect);
  InnerSV_F64MatToF64Array(_tvecs.row(0),sizeof(stCameraParams->af64CameraTranslateVect)/sizeof(stCameraParams->af64CameraTranslateVect[0]),stCameraParams->af64CameraTranslateVect);
  return SV_ENUM_CALI_SUCCEED;
}//end of InnerSV_Calibrate_InnerProcess

const static SV_VOID InnerSV_EachCaliParamExportToCaliResult(const SV_CALI_INPUT_S& stCaliInput,
    const SV_CAMERA_PARAMS_S &stCameraParam,SV_S32& s32Ret,SV_CALI_RESULT_S* pstCaliResult) {
  //初始化标定结果
  memset(pstCaliResult,0 ,sizeof(*pstCaliResult));
  pstCaliResult->s32CaliMethod = SV_ENUM_CALIMETHOD_BUTT;
  pstCaliResult->s32Result = s32Ret;
  pstCaliResult->s32CameraChannl=stCaliInput.s32CameraChannl;
  //如果标定成功，输出标定模板
  if(SV_ENUM_CALI_SUCCEED ==s32Ret) {
    memcpy(&pstCaliResult->stCameraParam,&stCameraParam,sizeof(stCameraParam));
    pstCaliResult->s32CaliMethod =stCaliInput.s32CaliMethod;
    //保存不同标定方式标定模板
    if(SV_ENUM_CALIMETHOD_UCHESSBORD==stCaliInput.s32CaliMethod) {
      memcpy(&pstCaliResult->stCaliPatern.stChessboard,&stCaliInput.stCaliPatern.stChessboard, \
          std::min(sizeof(pstCaliResult->stCaliPatern.stChessboard),sizeof(stCaliInput.stCaliPatern.stChessboard)));
    }
    else if(SV_ENUM_CALIMETHOD_U8POINTS == stCaliInput.s32CaliMethod) {
      memcpy(&pstCaliResult->stCaliPatern.stU8Point,&stCaliInput.stCaliPatern.stU8Point, \
          std::min(sizeof(pstCaliResult->stCaliPatern.stU8Point),sizeof(stCaliInput.stCaliPatern.stU8Point)));
    }
    else;
  }
}
const static SV_VOID InneSV_CalibrateEachChannelUseChessBoardPatern(const SV_CALI_INPUT_S& stCaliInput,
    const SV_SIZE_S& vehicleSize,SV_CALI_RESULT_S* pstCaliResult) {
  //判断输入图像是否为空
  if(SV_FALSE == InnerSV_CheckIfSVImageEmpty(stCaliInput.stImage)) {
    pstCaliResult->s32Result=SV_ENUM_CALI_NOIMAGE;
    return;
  }
  ////将标定模板及车型尺寸转换为实际标定所需要的参考坐标点及棋盘格参数
  SV_CALI_UCHESSBOARD_PATERN_S stPatern = stCaliInput.stCaliPatern.stChessboard;
  SV_ORIGIN_CHESS_INPUT_S stOrginAndChessBoard = InnerSV_GnerateOriginChessBoardEach(stPatern,vehicleSize,stCaliInput.s32CameraChannl);

  std::vector< std::vector<cv::Point2f> > ImagePointsVect;
   if(SV_FALSE == InnerSV_DetectChessborad(stCaliInput.stImage,stOrginAndChessBoard,&ImagePointsVect)) {
     pstCaliResult->s32Result=SV_ENUM_CALI_NOCHESSBOARD;
     return;
   }
   //生成棋盘格世界坐标点
   std::vector<cv::Point3f> stWordPointsVect;
   cv::Size stSize(vehicleSize.s32Width,vehicleSize.s32Height);
   InnerSV_GenWordPointsFromOriginChessInput(stOrginAndChessBoard,&stWordPointsVect);
   //生成边缘区域棋盘格世界坐标点
   std::vector<cv::Point3f> stEdgeVector;
   cv::Size stEdgeSize = stOrginAndChessBoard.stEdge;
   cv::Size stCenterSize = stOrginAndChessBoard.stCenter;
   for(SV_S32 j=0;j<stEdgeSize.height;++j) {
     for(SV_S32 k=0;k<stEdgeSize.width;++k){
             stEdgeVector.push_back(stWordPointsVect[j*stCenterSize.width+k]);
         }
   }//end of outer for
   std::vector< std::vector<cv::Point3f> > stObjectPoints;//生成于棋盘格角点集对应的世界坐标集
   stObjectPoints.push_back(stWordPointsVect);
   for(SV_S32 i=1;i<ImagePointsVect.size();++i) {
     stObjectPoints.push_back(stEdgeVector);
   }
   //调用InnerSV_Calibrate_InnerProcess单步计算摄像头内外参数，此种方式下使能bCheckCond
   SV_CAMERA_PARAMS_S stCameraParam;
   SV_S32 s32Ret= InnerSV_Calibrate_InnerProcess(ImagePointsVect,
       stObjectPoints,cv::Size(stCaliInput.stImage.stImageSize.s32Width,stCaliInput.stImage.stImageSize.s32Height), \
       SV_TRUE, &stCameraParam);
   //输出标定结果
   InnerSV_EachCaliParamExportToCaliResult(stCaliInput,stCameraParam,s32Ret,pstCaliResult);
   return;
}//InneSV_CalibrateEachChannelUseChessBoardPartern


const static SV_VOID InnerSV_SplitImagePointsAndWordPointsToTwoPart( const SV_CALI_U8P_PATERN_S &stPatern,
    const std::vector<cv::Point3f> &stWordPointVect,
    std::vector< std::vector<cv::Point2f> > *pstImagePointsVect,
    std::vector< std::vector<cv::Point3f> > *pstObjPointsVect) 
{
  std::vector<cv::Point2f>stImagePointLeftVect,stImagePointRightLeft;
  std::vector<cv::Point3f>stWordVect;
  for(SV_S32 i=0;i<3;++i) {
    stImagePointLeftVect.push_back(cv::Point2f(stPatern.stf32ImagePoints[i].f32X,stPatern.stf32ImagePoints[i].f32Y));
    stImagePointRightLeft.push_back(cv::Point2f(stPatern.stf32ImagePoints[3-i].f32X,stPatern.stf32ImagePoints[3-i].f32Y));
    stWordVect.push_back(stWordPointVect[i]);
  }
  for(SV_S32 i=0;i<3;++i) {
    stImagePointLeftVect.push_back(cv::Point2f(stPatern.stf32ImagePoints[7-i].f32X,stPatern.stf32ImagePoints[7-i].f32Y));
    stImagePointRightLeft.push_back(cv::Point2f(stPatern.stf32ImagePoints[4+i].f32X,stPatern.stf32ImagePoints[4+i].f32Y));
    stWordVect.push_back(stWordPointVect[7-i]);
  }
  pstImagePointsVect->push_back(stImagePointLeftVect);
  pstImagePointsVect->push_back(stImagePointRightLeft);
  pstObjPointsVect->push_back(stWordVect);
  pstObjPointsVect->push_back(stWordVect);
  return;
}//end of InnerSV_SplitImagePointsAndWordPointsToTwoPart

const static SV_VOID InnerSV_CalibrateEachChannelUse8PoitPatern(const SV_CALI_INPUT_S& stCaliInput,
    const SV_SIZE_S& vehicleSize,SV_CALI_RESULT_S* pstCaliResult) {
  //判断输入图像是否为空
  if(SV_FALSE == InnerSV_CheckIfSVImageEmpty(stCaliInput.stImage)) {
    pstCaliResult->s32Result=SV_ENUM_CALI_NOIMAGE;
  }
  //生成8点式模板对应世界坐标点
  std::vector<cv::Point3f> stWordPointVect;
  SV_CALI_U8P_PATERN_S stPatern = stCaliInput.stCaliPatern.stU8Point;
  cv::Size stSize(vehicleSize.s32Width,vehicleSize.s32Height);
  InnerSV_GenChannl8PointWordPs(stPatern,stSize, \
      stCaliInput.s32CameraChannl, &stWordPointVect);
  //将8个世界坐标点及像素坐标点坐标点，平分为 0 1 2  5  6 7 及3 2 1 6 5 4 两组
  std::vector< std::vector<cv::Point2f> > stImagePointsVect;
  std::vector< std::vector<cv::Point3f> > stObjPointsVect;
  InnerSV_SplitImagePointsAndWordPointsToTwoPart(stPatern,stWordPointVect,&stImagePointsVect,&stObjPointsVect);
  //调用InnerSV_Calibrate_InnerProcess单步计算摄像头内外参数，此种方式下不使能bCheckCond
  SV_CAMERA_PARAMS_S stCameraParam;
  SV_S32 s32Ret= InnerSV_Calibrate_InnerProcess(stImagePointsVect,
      stObjPointsVect,cv::Size(stCaliInput.stImage.stImageSize.s32Width,stCaliInput.stImage.stImageSize.s32Height), \
      SV_FALSE, &stCameraParam);
  //输出标定结果
  InnerSV_EachCaliParamExportToCaliResult(stCaliInput,stCameraParam,s32Ret,pstCaliResult);
  return;
}//end of InnerSV_CalibrateEachChannelUse8PoitPatern


}//end of namespace svmcalibrate
}//end of namespace sv_avm
}//end of




