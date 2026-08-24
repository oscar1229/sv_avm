/*
 * svmcalibrate.hpp
 *
 *
 * 有关环视标定操作相关的结构体及功能函数
 *
 */

#ifndef SV_AVM_SVMCALIBRATE_HPP_
#define SV_AVM_SVMCALIBRATE_HPP_
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include<glog/logging.h> //谷歌日志库

#include "common/svtype.hpp"//POD数据自定义及SV公共数据结构
#include "include/sv_avmcommon.hpp"

#ifdef SV_AVM_DEBUG
#define SV_AVM_CALI_DEBUG 1
#endif

namespace sm {
namespace sv_avm {
namespace svmcalibrate {
//标定操作有关摄像头通道，图像及标定模板输入的结构体
struct SV_CALI_INPUT_S {
  SV_IMAGE_S stImage;//摄像头图像
  SV_S32 s32CameraChannl; //摄像头通道
  //标定方式，可选：SV_ENUM_CALIMETHOD_UCHESSBORD
  //             SV_ENUM_CALIMETHOD_U8POINTS
  SV_S32 s32CaliMethod;
  union {
    SV_CALI_UCHESSBOARD_PATERN_S stChessboard;//棋盘格标定模板
    SV_CALI_U8P_PATERN_S stU8Point;//8点式标定模板
  }stCaliPatern;
};

//标定结果结构体
struct SV_CALI_RESULT_S {
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
//@brief 标定并保存标定结果到s8SavedXmlFile
//@param in stCaliInVect ，各通道标定输入参数向量
//       in stVehicleSize,车辆尺寸参数
//       in 标定结果保存Xml文件
//       out pstCaliResultVect，各通道摄像头SVM标定结果
//@return 保存失败，返回SV_FALSE
SV_BOOL SV_Calibrate(const std::vector<SV_CALI_INPUT_S>& stCaliInVect,
    const SV_SIZE_S& stVehicleSize,const SV_S8* s8SavedXmlFile,
    std::vector<SV_CALI_RESULT_S>* pstCaliResultVect);

}//end of namespace svmcalibrate
}//end of sv_avm
}//endof sm

#endif /* SV_AVM_SVMCALIBRATE_HPP_ */
