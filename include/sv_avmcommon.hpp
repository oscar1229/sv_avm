/*
 * sv_avmcommon.hpp
 *
 */

#ifndef SV_SVMAVM_SV_AVMCOMMON_HPP_
#define SV_SVMAVM_SV_AVMCOMMON_HPP_
namespace sm {
namespace sv_avm {

 enum { //标定方式枚举
     SV_ENUM_CALIMETHOD_UCHESSBORD=0,//棋盘格式标定
     SV_ENUM_CALIMETHOD_U8POINTS,//8点式标定
     SV_ENUM_CALIMETHOD_BUTT,
   };

 enum {//标定结果枚举
   SV_ENUM_CALI_NOIMAGE,
   SV_ENUM_CALI_NOCHESSBOARD,//当前通道未提取到棋盘格，只限于棋盘格标定方式
   SV_ENUM_CALI_CALCFAILED,//当前通道参数计算错误，即计算的参数超出许可的参数范围
   SV_ENUM_CALI_FAILED,//标定失败，未提取到至少2块棋盘格，只限于棋盘格标定方式
   SV_ENUM_CALI_SUCCEED,//标定成功
   SV_ENUM_CALI_BUTT,
 };

     struct SV_CAMERA_PARAMS_S {
          SV_F64 af64CameraK[9];//摄像头内参矩阵
          SV_F64 af64CameraDistort[4];//鱼眼畸变参数
          SV_F64 af64CameraRotateVect[3];//相机旋转向量
          SV_F64 af64CameraTranslateVect[3];//相机平移向量
          SV_SIZE_S stImageSize;//摄像头图像尺寸，比如1920,1080 或1280,720
     };

     struct SV_CALI_UCHESSBOARD_PATERN_S {
         SV_S32 s32ChessPosition;//左右棋盘格铺设位置，即左右棋盘格中心点到后视棋盘格的垂直距离
         SV_SIZE_S stChessBoardCenter;//中央区域棋盘格规格
         SV_SIZE_S stChessBoardEdge;//边缘区域棋盘格规格
         SV_S32 s32GridSize;//棋盘格单块黑白格的宽度
     };

     struct SV_CALI_U8P_PATERN_S {
         SV_S32 s3U8PaternBoardSize;//正方形黑白格标定布的长
         SV_POINT2F32_S stf32ImagePoints[8];//选取的8个顶点的像素坐标
     };

     //摄像头纹理网格参数
     struct SV_BOWL_GRID_PARAM_S {      /* Parameters of grid */
         SV_F32 f32GroundRadiusScal;//车模矩形对角线一半的f32GroundRadiusScal倍长度为碗面地面区域半径
         SV_S32 s32Angles;     /*  碗面1+2象限角度个数，比如以1°为阶梯，则s32GridAngles为180*/
         SV_S32 s32NopZ;         /* Number of points in z axis */
         SV_F32 f32StepX;      /* Step in x axis which is used to define grid points in z axis.                         * Step in z axis: step_z[i] = (i * step_x)^2, i = 1, 2, ... - number of point */
         SV_F32 f32OverLayAngle;
     };
}
}


#endif /* SRC_COMMON_SV_AVMCOMMON_HPP_ */
