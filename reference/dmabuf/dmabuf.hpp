/*
 * dmabuf.hpp
 *
 *  Created on: Aug 11, 2020
 *      Author: Beck Chow
 */

#ifndef SRC_SVRENDER_CAMERA_DMABUF_DMABUF_HPP_
#define SRC_SVRENDER_CAMERA_DMABUF_DMABUF_HPP_
#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#include "../../../vg_avmcommon.hpp"
#include <drm/drm_fourcc.h>

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

                    //@brief Create EGLImage Based on dma_fd of stIMage
                    //@param in  stImage frameImage
                    //       in  dpy
                    //@return EGLImageKHR
                    //@remarks if Create EGLImage failed,return NULL
                    EGLImageKHR InnerVG_SetupDmafd(const VG_IMAGE_S &stImage, EGLDisplay dpy);

                } // namespace dmabuf
            }     // namespace camera
        }         // namespace vgrender
    }             // namespace vg_avm
} // namespace vagoo

#endif /* SRC_SVRENDER_CAMERA_DMABUF_DMABUF_HPP_ */
