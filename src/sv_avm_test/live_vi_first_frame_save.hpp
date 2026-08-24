#ifndef SV_AVM_TEST_LIVE_VI_FIRST_FRAME_SAVE_HPP_
#define SV_AVM_TEST_LIVE_VI_FIRST_FRAME_SAVE_HPP_

#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include <opencv2/opencv.hpp>

#include "common/svtype.hpp"
namespace sm {
namespace sv_avm {


static SV_BOOL SaveUyvyBufferAsJPG(const char* s8FileName,
    const SV_U8* pu8Uyvy,
    SV_S32 s32Width,
    SV_S32 s32Height,
    SV_U32 u32Stride) {
    if (s8FileName == NULL || pu8Uyvy == NULL || s32Width <= 0 || s32Height <= 0) {
        return SV_FALSE;
    }
    if (u32Stride < (SV_U32)s32Width * 2U) {
        return SV_FALSE;
    }

    cv::Mat mUyvyPacked(s32Height, s32Width, CV_8UC2);
    for (SV_S32 y = 0; y < s32Height; ++y) {
        memcpy(mUyvyPacked.ptr<SV_U8>(y), pu8Uyvy + (size_t)y * u32Stride, (size_t)s32Width * 2U);
    }

    cv::Mat mBgr;
    cv::cvtColor(mUyvyPacked, mBgr, cv::COLOR_YUV2BGR_UYVY);
    return cv::imwrite(s8FileName, mBgr) ? SV_TRUE : SV_FALSE;
}

static SV_BOOL SaveUyvyDmaImageAsJPG(const char* s8FileName, const SV_IMAGE_S& stImage) {
    if (stImage.s32ImageType != SV_IMAGE_TYPE_UYVY ||
        stImage.stImageSize.s32Width <= 0 ||
        stImage.stImageSize.s32Height <= 0) {
        return SV_FALSE;
    }

    const SV_S32 s32Width = stImage.stImageSize.s32Width;
    const SV_S32 s32Height = stImage.stImageSize.s32Height;
    const SV_U32 u32Stride = stImage.u32Stride[0] > 0 ?
        stImage.u32Stride[0] : (SV_U32)s32Width * 2U;

    if (stImage.dataPtr != NULL) {
        return SaveUyvyBufferAsJPG(s8FileName,
            reinterpret_cast<const SV_U8*>(stImage.dataPtr),
            s32Width, s32Height, u32Stride);
    }

    if (stImage.s32DmaFd <= 0) {
        return SV_FALSE;
    }

    const size_t szMapLen = (size_t)u32Stride * (size_t)s32Height;
    SV_U8* pu8Map = reinterpret_cast<SV_U8*>(
        mmap(NULL, szMapLen, PROT_READ, MAP_SHARED, stImage.s32DmaFd, 0));
    if (MAP_FAILED == pu8Map) {
        return SV_FALSE;
    }

    SV_BOOL bOk = SaveUyvyBufferAsJPG(s8FileName, pu8Map, s32Width, s32Height, u32Stride);
    munmap(pu8Map, szMapLen);
    return bOk;
}

}
}

#endif
