/*
 * dmabuf.cpp
 *
 *  Created on: Aug 11, 2020
 *      Author: zdz
 */
#include "dmabuf.hpp"
#include <glog/logging.h>
namespace vagoo
{
	namespace vg_avm
	{
		namespace vgrender
		{
			namespace camera
			{
				namespace dmabuf
				{

					static VG_S32 InnerVG_SVFMT_To_DramFmt(const VG_S32 &s32SvFmt, VG_S32 *ps32DramFmt);
					static VG_S32 InnerVG_GetPlaneNumberFromDramFmt(const VG_S32 &s32DramFmt);
					static EGLint *InnerVG_SetupDmaDramFmt(const VG_S32 &s32DramFmt, const VG_SIZE_S &stImgSize, EGLint *p_attribs);
					static EGLint *InnerVG_SetupDmaEachPlane(const VG_S32 &s32Plane, const VG_S32 &s32DmaFd, const VG_S32 &s32FdOffset,
															 const VG_S32 &s32Widthstep, EGLint *p_attribs);

					EGLImageKHR InnerVG_SetupDmafd(const VG_IMAGE_S &stImage, EGLDisplay dpy)
					{
						VG_S32 s32DramFmt =DRM_FORMAT_NV21;
					//	if (VG_FAILURE == InnerVG_SVFMT_To_DramFmt(stImage.s32ImageType, &s32DramFmt))
							//return NULL;
						int atti = 0;
						EGLint attribs0[30];
						//set the image's size
						attribs0[atti++] = EGL_WIDTH;
						attribs0[atti++] = stImage.stImageSize.s32Width;
						attribs0[atti++] = EGL_HEIGHT;
						attribs0[atti++] = stImage.stImageSize.s32Height;

						//set pixel format
						attribs0[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
						attribs0[atti++] = s32DramFmt;

						switch (s32DramFmt)
						{
						case DRM_FORMAT_ARGB8888:
						case DRM_FORMAT_RGBA8888:
						case DRM_FORMAT_BGRA8888:
							attribs0[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
							attribs0[atti++] = stImage.stM.s32DmaFd[0];
							attribs0[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
							attribs0[atti++] = 0;
							attribs0[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
							attribs0[atti++] = stImage.stImageSize.s32Width * 4;
							break;
						case DRM_FORMAT_NV21:
						case DRM_FORMAT_NV12:
							//set buffer fd, offset, pitch for y component
							attribs0[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
							attribs0[atti++] = stImage.stM.s32DmaFd[0];
							attribs0[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
							attribs0[atti++] = 0;
							attribs0[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
							attribs0[atti++] = stImage.stImageSize.s32Width;

							//set buffer fd, offset ,pitch
							attribs0[atti++] = EGL_DMA_BUF_PLANE1_FD_EXT;
							attribs0[atti++] = stImage.stM.s32Planes == 1 ? stImage.stM.s32DmaFd[0] : stImage.stM.s32DmaFd[1];
							attribs0[atti++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
							attribs0[atti++] = stImage.stM.s32Planes == 1 ? stImage.stImageSize.s32Width * stImage.stImageSize.s32Height : 0;
							attribs0[atti++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
							attribs0[atti++] = stImage.stImageSize.s32Width;
							break;
						default:
							//printf("format:%d NOT support\n", s32DramFmt);
							break;
						};

						//set color space and color range
						attribs0[atti++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
						attribs0[atti++] = EGL_ITU_REC709_EXT;
						attribs0[atti++] = EGL_SAMPLE_RANGE_HINT_EXT;
						attribs0[atti++] = EGL_YUV_FULL_RANGE_EXT;

						attribs0[atti++] = EGL_NONE;
                 
						EGLImageKHR ret = eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,//
															0, attribs0);//EGL_NO_CONTEXT
						if (EGL_SUCCESS == eglGetError())
							return ret;
						else
							return NULL;
					}

					static VG_S32 InnerVG_SVFMT_To_DramFmt(const VG_S32 &s32SvFmt, VG_S32 *ps32DramFmt)
					{
						VG_S32 s32DramFmt;
						switch (s32SvFmt)
						{

						case VG_IMAGE_TYPE_VYUY:
							s32DramFmt = DRM_FORMAT_VYUY;
							break;
						case VG_IMAGE_TYPE_YUYV:
							s32DramFmt = DRM_FORMAT_YUYV;
							break;
						case VG_IMAGE_TYPE_YVYU:
							s32DramFmt = DRM_FORMAT_YVYU;
							break;
						case VG_IMAGE_TYPE_YUV420P:
							s32DramFmt = DRM_FORMAT_YUV420;
							break;
						case VG_IMAGE_TYPE_YVU420P:
							s32DramFmt = DRM_FORMAT_YVU420;
							break;
						case VG_IMAGE_TYPE_YUV420SP:
							s32DramFmt = DRM_FORMAT_NV12;
							break;
						case VG_IMAGE_TYPE_YVU420SP:
							s32DramFmt = DRM_FORMAT_NV21;
							break;
						default:
						//	printf("Not support Video Fmt，should be yuv\n");
							return VG_FAILURE;
							break;
						}
						*ps32DramFmt = s32DramFmt;
						return VG_SUCCEED;
					}
					static VG_S32 InnerVG_GetPlaneNumberFromDramFmt(const VG_S32 &s32DramFmt)
					{
						VG_S32 s32Plane = 0;
						switch (s32DramFmt)
						{
						case DRM_FORMAT_UYVY:
						case DRM_FORMAT_VYUY:
						case DRM_FORMAT_YUYV:
						case DRM_FORMAT_YVYU:
						default:
							s32Plane = 1;
							break;
						case DRM_FORMAT_NV12:
						case DRM_FORMAT_NV21:
							s32Plane = 2;
							break;
						case DRM_FORMAT_YUV420:
						case DRM_FORMAT_YVU420:
							s32Plane = 3;
							break;
						}
						return s32Plane;
					}

				} // namespace dmabuf
			}	  // namespace camera
		}		  // namespace vgrender
	}			  // namespace vg_avm
} // namespace vagoo
