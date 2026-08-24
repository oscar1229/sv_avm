/**********************************************************************************
 * sv_type.hpp
 *
 *
 *POD数据类型重定义,及公用数据类型结构体定义
 *公用枚举，包括摄像头通道号枚举、图像制式枚举
 *公用数据类型包括：SV_SIZE_S、
                 SV_POINT2S32_S、SV_POINT2F32_S、SV_POINT2F64_S、
                 SV_POINT3S32_S、SV_POINT3F32_S、SV_POINT3F64_S、
                 SV_RECT_S，SV_IMAGE_S
 *
 *
 ********************************************************************************/

#ifndef COMMON_SVTYPE_HPP_
#define COMMON_SVTYPE_HPP_
#pragma once
namespace sm {
#define SV_TRUE  1
#define SV_FALSE 0

#define SV_SUCESSED 0
#define SV_FAILURED -1
//POD数据类型重定义
typedef char SV_S8;
typedef unsigned char SV_U8;
typedef short SV_S16;
typedef unsigned short SV_U16;
typedef int  SV_S32;
typedef unsigned int SV_U32;
typedef long long int SV_S64;
typedef unsigned long long SV_U64;
typedef float  SV_F32;
typedef double SV_F64;
typedef bool  SV_BOOL;
typedef void SV_VOID;


//摄像头通道号枚举
enum {
  SV_ENUM_CAMERA_LEFT =0,
  SV_ENUM_CAMERA_RIGHT,
  SV_ENUM_CAMERA_FRONT,
  SV_ENUM_CAMERA_BACK,
  SV_ENUM_CAMERA_BUTT,
};

//图像制式枚举
enum {
  SV_IMAGE_TYPE_UYVY, //YUV422 packed: U0 Y0 V0 Y1
  SV_IMAGE_TYPE_YUV420P,
  SV_IMAGE_TYPE_BGR, //RGB类型
  SV_IMAGE_TYPE_BGRA,//ARGB类型
  SV_IMAGE_TYPE_NV12,//YUV420SP(NV12):Y平面 + UV交织平面,用于dma_buf零拷贝导入
  SV_IMAGE_TYPE_BUTT,
};
//尺寸规格
struct SV_SIZE_S {
	SV_S32 s32Width;//宽
	SV_S32 s32Height;//长或高
};

struct SV_SIZEF_S {
  SV_F32 f32Width;
  SV_F32 f32Height;
};

//二维整型坐标点
struct SV_POINT2S32_S {
	SV_S32 s32X;
	SV_S32 s32Y;
};
//三维整型坐标点
struct SV_POINT3S32_S
{
	SV_S32 s32X;
	SV_S32 s32Y;
	SV_S32 s32Z;
};

//二维浮点型坐标点
struct SV_POINT2F32_S {
	SV_F32 f32X;
	SV_F32 f32Y;
};
//三维浮点型坐标点
struct SV_POINT3F32_S {
    SV_F32 f32X;
    SV_F32 f32Y;
    SV_F32 f32Z;
};

//二维double型坐标点
struct SV_POINT2F64_S {
    SV_F64 f64X;
	SV_F64 f64Y;
};
//三维double型坐标点
struct SV_POINT3F64_S
{
    SV_F64 f64X;
	SV_F64 f64Y;
	SV_F64 f64Z;
};

//长方形
struct SV_RECT_S {
	SV_POINT2S32_S stStartPoint;
	SV_SIZE_S stRectSize;
};


//图像结构体
struct SV_IMAGE_S
{
  SV_VOID* dataPtr;//数据域指针
  SV_S32 s32ImageType;//图像类型，可选值为SV_IMAGE_TYPE_UYVY，SV_IMAGE_TYPE_BGR，SV_IMAGE_TYPE_BGRA，SV_IMAGE_TYPE_YUV420P
  SV_SIZE_S stImageSize;
  SV_U32 u32Offset;
  SV_S32 s32BufIdx;
  SV_U64 u64Pts;
  SV_S32 s32DmaFd;//dma_buf文件描述符,用于零拷贝导入EGLImage;<=0表示无效(走普通拷贝上传)
  SV_U32 u32Stride[2];//dma_buf每个plane的pitch(bytes);0表示按格式默认值推导
  SV_U32 u32PlaneOffset[2];//dma_buf每个plane的offset(bytes);NV12 plane1通常为Y平面大小
};

enum {
    SV_ENUM_STREAM_VIDEO,
    SV_ENUM_STREAM_AUDIO,
    SV_ENUM_STREAM_DATA,
    SV_ENUM_STREAM_BUTT
};



struct SV_MEDIA_STREAM_S {
  SV_VOID *pStreamAddr;
  SV_U32 u32Len;
  SV_S64 s64Pts;
  SV_U8  u8Chn;
  SV_U8  u8StreamType;
  SV_BOOL bKeyFlag;
  SV_S32 s32BitRate;
  SV_U8 u8Fps;
  SV_SIZE_S stPicSize;
};

//获取摄像头通道数量
inline const SV_S32 s32GetCameraChannelNumber()
{
  return static_cast<SV_S32>(SV_ENUM_CAMERA_BUTT);
}

}


#endif /* COMMON_SVTYPE_HPP_ */
