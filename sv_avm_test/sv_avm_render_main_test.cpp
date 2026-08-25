/*
 * sv_avm_render_main_test.cpp
 *
 * SV AVM render demo.
 * Static mode keeps the original JPG/NV12 dma-buf benchmark.
 * Live VI mode pulls MPP VI dma-buf frames and replaces missing channels with all-zero dma-bufs.
 */
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/dma-heap.h>
#include <set>
#include <string>
#include <vector>

#include <GLES2/gl2.h>
#include <glog/logging.h>
#include <opencv2/opencv.hpp>

#ifdef SV_AVM_ENABLE_MPP
#include "sys_api.h"
#include "vb_api.h"
#include "vi_api.h"
#endif

#include "common/svtype.hpp"
#include "include/sv_avmcommon.hpp"
#include "src/svrender/display/display.hpp"
#include "src/svrender/common/viewtransoform/viewtransform.hpp"
#include "src/svrender/vehicle/vehiclerender.hpp"
#include "src/svrender/camera/camerarender.hpp"
#include "src/svmparam/svmparam.hpp"

using namespace sm;
using namespace sm::sv_avm;

static SV_BOOL g_bExit = SV_FALSE;

struct SvRunConfig {
    SV_BOOL bZeroCopy = SV_TRUE;
    SV_BOOL bLiveVi = SV_FALSE;
    SV_S32 s32Frames = 10;       // <=0 means run until Ctrl+C
    SV_S32 s32SleepUs = 20000;
    SV_S32 s32GridSubdiv = 180;
    SV_S32 s32LiveViDev = 0;
    SV_S32 s32LiveViWidth = 1280;
    SV_S32 s32LiveViHeight = 720;
    SV_S32 s32LiveViTimeoutMs = 5;
    SV_S32 s32LiveViMipiLanes = 4;
    SV_S32 s32LiveViMbps = 800;
    // SV_TRUE forces the fallback images even when the camera is available.
    SV_BOOL bUseFallbackImage = SV_FALSE;
    // Fallback JPG dir used when the camera is unavailable; relative to repo root.
    std::string strFallbackImageDir = "sv_avm_test/res";
    // SV_TRUE forces offscreen rendering even when a display is available.
    SV_BOOL bForceOffscreen = SV_FALSE;
    // Offscreen output path used when no display is present; empty disables it.
    std::string strOffscreenOutputPath = "";
};

// config.json lives at the repo root and is read as ../config.json from build/,
// so paths inside it are resolved relative to the repo root as well.
static std::string ResolveRepoPath(const std::string& strRelative) {
    if (strRelative.empty() || strRelative[0] == '/') return strRelative;
    return std::string("../") + strRelative;
}

static void ResetImage(SV_IMAGE_S* pstImage) {
    memset(pstImage, 0, sizeof(*pstImage));
    pstImage->s32DmaFd = -1;
}

static std::string Trim(std::string v) {
    while (!v.empty() && (v.front() == ' ' || v.front() == '\t' || v.front() == '\n' || v.front() == '\r')) v.erase(0, 1);
    while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\n' || v.back() == '\r')) v.pop_back();
    return v;
}

static void LoadConfigJson(const char* s8Path, SvRunConfig* pstCfg) {
    FILE* fp = fopen(s8Path, "rb");
    if (!fp) {
        LOG(WARNING) << "Config not found, using defaults: " << s8Path;
        return;
    }
    std::string strBuf;
    char acTmp[4096];
    size_t szN;
    while ((szN = fread(acTmp, 1, sizeof(acTmp), fp)) > 0) strBuf.append(acTmp, szN);
    fclose(fp);

    std::string strClean;
    strClean.reserve(strBuf.size());
    for (size_t i = 0; i < strBuf.size(); ++i) {
        if (strBuf[i] == '/' && i + 1 < strBuf.size() && strBuf[i + 1] == '/') {
            while (i < strBuf.size() && strBuf[i] != '\n') ++i;
        }
        if (i < strBuf.size()) strClean.push_back(strBuf[i]);
    }

    auto findVal = [&](const char* s8Key, std::string* pStrOut) -> SV_BOOL {
        std::string strKey = std::string("\"") + s8Key + "\"";
        size_t pos = strClean.find(strKey);
        if (pos == std::string::npos) return SV_FALSE;
        pos = strClean.find(':', pos + strKey.size());
        if (pos == std::string::npos) return SV_FALSE;
        ++pos;
        while (pos < strClean.size() && (strClean[pos] == ' ' || strClean[pos] == '\t' || strClean[pos] == '\n' || strClean[pos] == '\r')) ++pos;
        if (pos >= strClean.size()) return SV_FALSE;
        size_t end = pos;
        if (strClean[pos] == '[') {
            end = strClean.find(']', pos);
            if (end == std::string::npos) return SV_FALSE;
            ++end;
        } else if (strClean[pos] == '"' || strClean[pos] == '\'') {
            char quote = strClean[pos++];
            end = strClean.find(quote, pos);
            if (end == std::string::npos) return SV_FALSE;
            *pStrOut = strClean.substr(pos, end - pos);
            return SV_TRUE;
        } else {
            while (end < strClean.size() && strClean[end] != ',' && strClean[end] != '}' && strClean[end] != '\n' && strClean[end] != '\r') ++end;
        }
        *pStrOut = Trim(strClean.substr(pos, end - pos));
        return SV_TRUE;
    };

    auto parseBool = [&](const char* key, SV_BOOL* out) {
        std::string v;
        if (findVal(key, &v)) *out = (v == "true" || v == "1") ? SV_TRUE : SV_FALSE;
    };
    auto parseInt = [&](const char* key, SV_S32* out, SV_BOOL bAllowZero = SV_FALSE) {
        std::string v;
        if (findVal(key, &v)) {
            SV_S32 n = atoi(v.c_str());
            if (n > 0 || (bAllowZero && n == 0)) *out = n;
        }
    };

    parseBool("zero_copy", &pstCfg->bZeroCopy);
    parseBool("live_vi", &pstCfg->bLiveVi);
    parseInt("frames", &pstCfg->s32Frames, SV_TRUE);
    parseInt("sleep_us", &pstCfg->s32SleepUs, SV_TRUE);
    parseInt("grid_subdiv", &pstCfg->s32GridSubdiv);
    parseInt("live_vi_dev", &pstCfg->s32LiveViDev, SV_TRUE);
    parseInt("live_vi_width", &pstCfg->s32LiveViWidth);
    parseInt("live_vi_height", &pstCfg->s32LiveViHeight);
    parseInt("live_vi_timeout_ms", &pstCfg->s32LiveViTimeoutMs, SV_TRUE);
    parseInt("live_vi_mipi_lanes", &pstCfg->s32LiveViMipiLanes);
    parseInt("live_vi_mbps", &pstCfg->s32LiveViMbps);

    auto parseStr = [&](const char* key, std::string* out) {
        std::string v;
        if (findVal(key, &v) && !v.empty()) *out = v;
    };
    parseBool("use_fallback_image", &pstCfg->bUseFallbackImage);
    parseBool("force_offscreen", &pstCfg->bForceOffscreen);
    parseStr("fallback_image_dir", &pstCfg->strFallbackImageDir);
    parseStr("offscreen_output_path", &pstCfg->strOffscreenOutputPath);
}

static void ApplyArgs(int argc, char* argv[], SvRunConfig* pstCfg) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--live-vi") == 0) {
            pstCfg->bLiveVi = SV_TRUE;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            pstCfg->s32Frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--vi-timeout-ms") == 0 && i + 1 < argc) {
            pstCfg->s32LiveViTimeoutMs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [--live-vi] [--frames N] [--vi-timeout-ms N]\n", argv[0]);
            exit(0);
        }
    }
}

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) g_bExit = SV_TRUE;
}

static SV_BOOL LoadParamsFromXml(const char* s8XmlFile,
    SV_SIZE_S* pstVehicleSize,
    std::vector<SV_CAMERA_PARAMS_S>* pstCameraParamsVect) {
    svmparam::InnerSV_SvmParamClass clSvmParam;
    SV_BOOL bNeedCaliPatern = SV_FALSE;
    if (svmparam::ISV_ENUM_SUCCEED != clSvmParam.InnerSV_s32ReadFromXml(s8XmlFile, bNeedCaliPatern)) {
        LOG(ERROR) << "Read XML failed: " << s8XmlFile;
        clSvmParam.InnerSV_Init();
    }
    *pstVehicleSize = clSvmParam.InnerSV_stGetVehicleSize();
    pstCameraParamsVect->clear();
    for (SV_S32 i = 0; i <= SV_ENUM_CAMERA_BACK; ++i) {
        pstCameraParamsVect->push_back(clSvmParam.InnerSV_stGetCameraParamsEachChannl(i));
    }
    return SV_TRUE;
}

static SV_BOOL LoadJPGImage(const char* s8JPGFileName, SV_IMAGE_S* pstImage) {
    ResetImage(pstImage);
    cv::Mat mImg = cv::imread(s8JPGFileName);
    if (mImg.empty() || 3 != mImg.channels()) {
        LOG(ERROR) << "Failed to load/validate image: " << s8JPGFileName;
        return SV_FALSE;
    }
    cvtColor(mImg, mImg, cv::COLOR_BGR2RGB);
    SV_S32 s32Len = 3 * mImg.cols * mImg.rows;
    SV_S8* pTmp = reinterpret_cast<SV_S8*>(malloc(s32Len));
    if (NULL == pTmp) return SV_FALSE;
    memcpy(pTmp, mImg.ptr<SV_S8>(0), s32Len);
    pstImage->dataPtr = pTmp;
    pstImage->s32ImageType = SV_IMAGE_TYPE_BGR;
    pstImage->stImageSize.s32Width = mImg.cols;
    pstImage->stImageSize.s32Height = mImg.rows;
    return SV_TRUE;
}

static SV_S32 DmaHeapAlloc(size_t szLen) {
    int s32Heap = open("/dev/dma_heap/linux,cma", O_RDWR | O_CLOEXEC);
    if (s32Heap < 0) s32Heap = open("/dev/dma_heap/system", O_RDWR | O_CLOEXEC);
    if (s32Heap < 0) {
        LOG(ERROR) << "open /dev/dma_heap failed: " << strerror(errno);
        return -1;
    }
    struct dma_heap_allocation_data stAlloc;
    memset(&stAlloc, 0, sizeof(stAlloc));
    stAlloc.len = szLen;
    stAlloc.fd_flags = O_RDWR | O_CLOEXEC;
    if (ioctl(s32Heap, DMA_HEAP_IOCTL_ALLOC, &stAlloc) != 0) {
        LOG(ERROR) << "DMA_HEAP_IOCTL_ALLOC failed: " << strerror(errno);
        close(s32Heap);
        return -1;
    }
    close(s32Heap);
    return (SV_S32)stAlloc.fd;
}

static SV_BOOL ZeroDmaBuffer(SV_S32 s32Fd, size_t szLen) {
    SV_U8* pMap = (SV_U8*)mmap(NULL, szLen, PROT_READ | PROT_WRITE, MAP_SHARED, s32Fd, 0);
    if (MAP_FAILED == pMap) {
        LOG(ERROR) << "mmap dma_buf failed: " << strerror(errno);
        return SV_FALSE;
    }
    memset(pMap, 0, szLen);
    msync(pMap, szLen, MS_SYNC);
    munmap(pMap, szLen);
    return SV_TRUE;
}

static SV_BOOL CreateZeroDmaFrame(SV_S32 s32W, SV_S32 s32H, SV_S32 s32Type, SV_IMAGE_S* pstImage) {
    ResetImage(pstImage);
    if (s32W <= 0 || s32H <= 0) return SV_FALSE;
    size_t szLen = 0;
    if (s32Type == SV_IMAGE_TYPE_UYVY) szLen = (size_t)s32W * s32H * 2;
    else if (s32Type == SV_IMAGE_TYPE_NV12) szLen = (size_t)s32W * s32H * 3 / 2;
    else return SV_FALSE;

    SV_S32 s32Fd = DmaHeapAlloc(szLen);
    if (s32Fd < 0) return SV_FALSE;
    if (SV_FALSE == ZeroDmaBuffer(s32Fd, szLen)) {
        close(s32Fd);
        return SV_FALSE;
    }

    pstImage->dataPtr = NULL;
    pstImage->s32ImageType = s32Type;
    pstImage->stImageSize.s32Width = s32W;
    pstImage->stImageSize.s32Height = s32H;
    pstImage->s32DmaFd = s32Fd;
    if (s32Type == SV_IMAGE_TYPE_UYVY) {
        pstImage->u32Stride[0] = (SV_U32)s32W * 2U;
    } else {
        pstImage->u32Stride[0] = (SV_U32)s32W;
        pstImage->u32Stride[1] = (SV_U32)s32W;
        pstImage->u32PlaneOffset[1] = (SV_U32)s32W * (SV_U32)s32H;
    }
    return SV_TRUE;
}

static SV_BOOL LoadJPGImageNV12Dma(const char* s8JPGFileName, SV_IMAGE_S* pstImage) {
    ResetImage(pstImage);
    cv::Mat mBgr = cv::imread(s8JPGFileName);
    if (mBgr.empty() || 3 != mBgr.channels()) {
        LOG(ERROR) << "Failed to load/validate image: " << s8JPGFileName;
        return SV_FALSE;
    }
    const SV_S32 s32W = mBgr.cols;
    const SV_S32 s32H = mBgr.rows;
    if ((s32W & 1) || (s32H & 1)) return SV_FALSE;

    cv::Mat mI420;
    cvtColor(mBgr, mI420, cv::COLOR_BGR2YUV_I420);
    const size_t szY = (size_t)s32W * s32H;
    const size_t szUV = szY / 2;
    const size_t szTotal = szY + szUV;

    SV_S32 s32Fd = DmaHeapAlloc(szTotal);
    if (s32Fd < 0) return SV_FALSE;
    SV_U8* pMap = (SV_U8*)mmap(NULL, szTotal, PROT_READ | PROT_WRITE, MAP_SHARED, s32Fd, 0);
    if (MAP_FAILED == pMap) {
        close(s32Fd);
        return SV_FALSE;
    }
    const SV_U8* pI420 = mI420.ptr<SV_U8>(0);
    memcpy(pMap, pI420, szY);
    const SV_U8* pU = pI420 + szY;
    const SV_U8* pV = pU + szY / 4;
    SV_U8* pUV = pMap + szY;
    for (size_t i = 0; i < szY / 4; ++i) {
        pUV[2 * i + 0] = pU[i];
        pUV[2 * i + 1] = pV[i];
    }
    msync(pMap, szTotal, MS_SYNC);
    munmap(pMap, szTotal);

    pstImage->dataPtr = NULL;
    pstImage->s32ImageType = SV_IMAGE_TYPE_NV12;
    pstImage->stImageSize.s32Width = s32W;
    pstImage->stImageSize.s32Height = s32H;
    pstImage->s32DmaFd = s32Fd;
    pstImage->u32Stride[0] = (SV_U32)s32W;
    pstImage->u32Stride[1] = (SV_U32)s32W;
    pstImage->u32PlaneOffset[1] = (SV_U32)(s32W * s32H);
    return SV_TRUE;
}

// Loads a JPG as a UYVY dma-buf so fallback frames match the pixel format the
// live VI path produces, keeping the render path identical for both sources.
static SV_BOOL LoadJPGImageUYVYDma(const char* s8JPGFileName, SV_IMAGE_S* pstImage) {
    ResetImage(pstImage);
    cv::Mat mBgr = cv::imread(s8JPGFileName);
    if (mBgr.empty() || 3 != mBgr.channels()) {
        LOG(ERROR) << "Failed to load/validate image: " << s8JPGFileName;
        return SV_FALSE;
    }
    const SV_S32 s32W = mBgr.cols;
    const SV_S32 s32H = mBgr.rows;
    if ((s32W & 1) || (s32H & 1)) {
        LOG(ERROR) << "UYVY needs even dimensions, got " << s32W << "x" << s32H << ": " << s8JPGFileName;
        return SV_FALSE;
    }

    cv::Mat mI420;
    cvtColor(mBgr, mI420, cv::COLOR_BGR2YUV_I420);
    const size_t szY = (size_t)s32W * s32H;
    const size_t szTotal = szY * 2;   // UYVY packs 2 bytes per pixel

    SV_S32 s32Fd = DmaHeapAlloc(szTotal);
    if (s32Fd < 0) return SV_FALSE;
    SV_U8* pMap = (SV_U8*)mmap(NULL, szTotal, PROT_READ | PROT_WRITE, MAP_SHARED, s32Fd, 0);
    if (MAP_FAILED == pMap) {
        close(s32Fd);
        return SV_FALSE;
    }

    const SV_U8* pY = mI420.ptr<SV_U8>(0);
    const SV_U8* pU = pY + szY;
    const SV_U8* pV = pU + szY / 4;
    const SV_S32 s32ChromaW = s32W / 2;
    for (SV_S32 y = 0; y < s32H; ++y) {
        const SV_U8* pYRow = pY + (size_t)y * s32W;
        const SV_U8* pURow = pU + (size_t)(y / 2) * s32ChromaW;
        const SV_U8* pVRow = pV + (size_t)(y / 2) * s32ChromaW;
        SV_U8* pDst = pMap + (size_t)y * s32W * 2;
        for (SV_S32 x = 0; x < s32ChromaW; ++x) {
            pDst[4 * x + 0] = pURow[x];
            pDst[4 * x + 1] = pYRow[2 * x];
            pDst[4 * x + 2] = pVRow[x];
            pDst[4 * x + 3] = pYRow[2 * x + 1];
        }
    }
    msync(pMap, szTotal, MS_SYNC);
    munmap(pMap, szTotal);

    pstImage->dataPtr = NULL;
    pstImage->s32ImageType = SV_IMAGE_TYPE_UYVY;
    pstImage->stImageSize.s32Width = s32W;
    pstImage->stImageSize.s32Height = s32H;
    pstImage->s32DmaFd = s32Fd;
    pstImage->u32Stride[0] = (SV_U32)s32W * 2U;
    return SV_TRUE;
}

// Fills stOwnedFrames with 4 UYVY fallback frames from imagech0..3.jpg,
// substituting an all-zero frame for any image that fails to load.
static void LoadFallbackUYVYFrames(const SvRunConfig& stCfg, std::vector<SV_IMAGE_S>* pstFrames) {
    const std::string strDir = ResolveRepoPath(stCfg.strFallbackImageDir);
    LOG(INFO) << "Loading fallback images from: " << strDir;
    for (SV_S32 i = 0; i < 4; ++i) {
        char acPath[512];
        snprintf(acPath, sizeof(acPath), "%s/imagech%d.jpg", strDir.c_str(), i);
        SV_IMAGE_S stImage;
        ResetImage(&stImage);
        if (SV_TRUE == LoadJPGImageUYVYDma(acPath, &stImage)) {
            LOG(INFO) << "Fallback ch" << i << " loaded: " << acPath
                      << " (" << stImage.stImageSize.s32Width << "x" << stImage.stImageSize.s32Height << ")";
            pstFrames->push_back(stImage);
            continue;
        }
        LOG(WARNING) << "Fallback ch" << i << " unavailable (" << acPath << "), using all-zero frame";
        SV_IMAGE_S stZero;
        ResetImage(&stZero);
        if (SV_TRUE == CreateZeroDmaFrame(stCfg.s32LiveViWidth, stCfg.s32LiveViHeight, SV_IMAGE_TYPE_UYVY, &stZero)) {
            pstFrames->push_back(stZero);
        } else {
            LOG(ERROR) << "Failed to create all-zero fallback frame for channel " << i;
        }
    }
}

static std::vector<SV_IMAGE_S> LoadCameraFrames(SV_BOOL bZeroCopy) {
    std::vector<SV_IMAGE_S> stImageVect;
    const char* as8FileNames[4] = {"../sv_avm_test/res/imagech0.jpg", "../sv_avm_test/res/imagech1.jpg", "../sv_avm_test/res/imagech2.jpg", "../sv_avm_test/res/imagech3.jpg"};
    LOG(INFO) << "Image load mode: " << (bZeroCopy ? "NV12 dma_buf (zero-copy)" : "RGB (copy upload)");
    for (SV_S32 i = 0; i < 4; ++i) {
        SV_IMAGE_S stImage;
        ResetImage(&stImage);
        SV_BOOL bOk = bZeroCopy ? LoadJPGImageNV12Dma(as8FileNames[i], &stImage) : LoadJPGImage(as8FileNames[i], &stImage);
        if (SV_TRUE == bOk) stImageVect.push_back(stImage);
        else LOG(WARNING) << "Failed to load image " << i << ": " << as8FileNames[i];
    }
    return stImageVect;
}

static void ReleaseCameraFrames(std::vector<SV_IMAGE_S>& stImageVect) {
    for (size_t i = 0; i < stImageVect.size(); ++i) {
        if (stImageVect[i].dataPtr != NULL) free(stImageVect[i].dataPtr);
        if (stImageVect[i].s32DmaFd > 0) close(stImageVect[i].s32DmaFd);
    }
    stImageVect.clear();
}

#ifdef SV_AVM_ENABLE_MPP
class MppViFrameSource {
public:
    MppViFrameSource() : bSysInited(SV_FALSE), bVbInited(SV_FALSE), bViInited(SV_FALSE),
        bDevEnabled(SV_FALSE), bTaskRun(SV_FALSE), s32Dev(0), s32TimeoutMs(5), s32Width(1280), s32Height(720) {
        memset(abChnConfigured, 0, sizeof(abChnConfigured));
        memset(abChnEnabled, 0, sizeof(abChnEnabled));
        memset(abThreadStarted, 0, sizeof(abThreadStarted));
        memset(abPendingValid, 0, sizeof(abPendingValid));
        memset(abLatestValid, 0, sizeof(abLatestValid));
        memset(as32MissLogCount, 0, sizeof(as32MissLogCount));
        memset(astPending, 0, sizeof(astPending));
        memset(astLatest, 0, sizeof(astLatest));
        for (SV_S32 ch = 0; ch < 4; ++ch) {
            pthread_mutex_init(&astLatestLock[ch], NULL);
            astThreadArg[ch].pstSelf = this;
            astThreadArg[ch].s32Chn = ch;
        }
    }

    ~MppViFrameSource() {
        Close();
        for (SV_S32 ch = 0; ch < 4; ++ch) {
            pthread_mutex_destroy(&astLatestLock[ch]);
        }
    }

    SV_BOOL Open(const SvRunConfig& cfg) {
        s32Dev = cfg.s32LiveViDev;
        s32TimeoutMs = cfg.s32LiveViTimeoutMs;
        s32Width = cfg.s32LiveViWidth;
        s32Height = cfg.s32LiveViHeight;

        S32 ret = SYS_Init();
        if (ret != 0) { LOG(ERROR) << "SYS_Init failed: " << ret; return SV_FALSE; }
        bSysInited = SV_TRUE;
        ret = VB_Init();
        if (ret != 0) { LOG(ERROR) << "VB_Init failed: " << ret; Close(); return SV_FALSE; }
        bVbInited = SV_TRUE;
        ret = VI_Init();
        if (ret != 0) { LOG(ERROR) << "VI_Init failed: " << ret; Close(); return SV_FALSE; }
        bViInited = SV_TRUE;

        ViDevAttrS stDevAttr;
        memset(&stDevAttr, 0, sizeof(stDevAttr));
        stDevAttr.eWorkMode = VI_WORK_MODE_ONLINE;
        stDevAttr.u32Width = (U32)s32Width;
        stDevAttr.u32Height = (U32)s32Height;
        stDevAttr.u32MipiLaneNum = (U32)cfg.s32LiveViMipiLanes;
        stDevAttr.u32mbps = (U32)cfg.s32LiveViMbps;
        stDevAttr.bCapture2Preview = 0;
        ret = VI_SetDevAttr(s32Dev, &stDevAttr);
        if (ret != 0) { LOG(ERROR) << "VI_SetDevAttr failed: " << ret; Close(); return SV_FALSE; }

        for (SV_S32 ch = 0; ch < 4; ++ch) {
            ViChnAttrS stChnAttr;
            memset(&stChnAttr, 0, sizeof(stChnAttr));
            stChnAttr.eChnType = VI_CHN_TYPE_PHYSICAL;
            stChnAttr.ePixelFormat = MPP_PIXEL_FORMAT_UYVY;
            stChnAttr.u32Width = (U32)s32Width;
            stChnAttr.u32Height = (U32)s32Height;
            stChnAttr.eStrideAlign = VI_STRIDE_ALIGN_DEFAULT;
            stChnAttr.u32Depth = 2;
            ret = VI_SetChnAttr(s32Dev, ch, &stChnAttr);
            if (ret != 0) {
                LOG(WARNING) << "VI_SetChnAttr ch" << ch << " failed: " << ret << ", channel will use zero frame";
                continue;
            }
            abChnConfigured[ch] = SV_TRUE;
        }

        ret = VI_EnableDev(s32Dev);
        if (ret != 0) { LOG(ERROR) << "VI_EnableDev failed: " << ret; Close(); return SV_FALSE; }
        bDevEnabled = SV_TRUE;

        SV_S32 s32Enabled = 0;
        for (SV_S32 ch = 0; ch < 4; ++ch) {
            if (abChnConfigured[ch] != SV_TRUE) continue;
            ret = VI_EnableChn(s32Dev, ch);
            if (ret != 0) {
                LOG(WARNING) << "VI_EnableChn ch" << ch << " failed: " << ret << ", channel will use zero frame";
                continue;
            }
            abChnEnabled[ch] = SV_TRUE;
            ++s32Enabled;
            LOG(INFO) << "VI ch" << ch << " enabled: " << s32Width << "x" << s32Height << " UYVY depth=2";
        }
        if (s32Enabled == 0) {
            LOG(ERROR) << "No VI channel enabled";
            Close();
            return SV_FALSE;
        }

        bTaskRun = SV_TRUE;
        for (SV_S32 ch = 0; ch < 4; ++ch) {
            if (abChnEnabled[ch] != SV_TRUE) continue;
            ret = pthread_create(&ahThread[ch], NULL, CaptureThreadEntry, &astThreadArg[ch]);
            if (ret != 0) {
                LOG(WARNING) << "pthread_create ch" << ch << " failed: " << ret << ", channel will use zero frame";
                abChnEnabled[ch] = SV_FALSE;
                VI_DisableChn(s32Dev, ch);
                continue;
            }
            abThreadStarted[ch] = SV_TRUE;
        }
        return SV_TRUE;
    }

    SV_BOOL CaptureFrames(std::vector<SV_IMAGE_S>* pstImages) {
        ReleasePending();
        if (pstImages == NULL || pstImages->size() < 4) return SV_FALSE;
        SV_BOOL bAny = SV_FALSE;
        for (SV_S32 ch = 0; ch < 4; ++ch) {
            if (abChnEnabled[ch] != SV_TRUE) continue;

            pthread_mutex_lock(&astLatestLock[ch]);
            if (abLatestValid[ch] != SV_TRUE) {
                pthread_mutex_unlock(&astLatestLock[ch]);
                continue;
            }

            S32 ret = VB_RefAdd(astLatest[ch].ulBufferId);
            if (ret != 0) {
                pthread_mutex_unlock(&astLatestLock[ch]);
                LOG(WARNING) << "VB_RefAdd ch" << ch << " failed: " << ret << ", using all-zero dma_buf";
                continue;
            }

            VideoFrameInfo stFrame = astLatest[ch];
            pthread_mutex_unlock(&astLatestLock[ch]);

            SV_S32 s32Fd = (SV_S32)stFrame.stVFrame.u32Fd[0];
            if (s32Fd <= 0 && stFrame.ulBufferId != 0) {
                (void)VB_GetDmaBufFd(stFrame.ulBufferId, &s32Fd);
            }
            if (s32Fd <= 0) {
                LOG(WARNING) << "VI ch" << ch << " latest frame has no dma-buf fd, using all-zero dma_buf";
                VI_ReleaseChnFrame(s32Dev, ch, &stFrame);
                continue;
            }

            SV_IMAGE_S stImage;
            ResetImage(&stImage);
            stImage.dataPtr = NULL;
            stImage.s32ImageType = SV_IMAGE_TYPE_UYVY;
            stImage.stImageSize.s32Width = stFrame.stCommFrameInfo.u32Width ? (SV_S32)stFrame.stCommFrameInfo.u32Width : s32Width;
            stImage.stImageSize.s32Height = stFrame.stCommFrameInfo.u32Height ? (SV_S32)stFrame.stCommFrameInfo.u32Height : s32Height;
            stImage.s32DmaFd = s32Fd;
            stImage.s32BufIdx = (SV_S32)stFrame.u32Idx;
            stImage.u64Pts = (SV_U64)stFrame.stVFrame.u64PTS;
            stImage.u32Stride[0] = stFrame.stVFrame.u32PlaneStride[0] ? stFrame.stVFrame.u32PlaneStride[0] : (U32)(stImage.stImageSize.s32Width * 2);
            stImage.u32PlaneOffset[0] = 0;
            (*pstImages)[ch] = stImage;

            astPending[ch] = stFrame;
            abPendingValid[ch] = SV_TRUE;
            bAny = SV_TRUE;
        }
        return bAny;
    }

    void ReleasePending() {
        for (SV_S32 ch = 0; ch < 4; ++ch) {
            if (abPendingValid[ch] == SV_TRUE) {
                VI_ReleaseChnFrame(s32Dev, ch, &astPending[ch]);
                abPendingValid[ch] = SV_FALSE;
                memset(&astPending[ch], 0, sizeof(astPending[ch]));
            }
        }
    }

    void Close() {
        bTaskRun = SV_FALSE;
        for (SV_S32 ch = 0; ch < 4; ++ch) {
            if (abThreadStarted[ch] == SV_TRUE) {
                pthread_join(ahThread[ch], NULL);
                abThreadStarted[ch] = SV_FALSE;
            }
        }

        ReleasePending();
        for (SV_S32 ch = 0; ch < 4; ++ch) {
            pthread_mutex_lock(&astLatestLock[ch]);
            if (abLatestValid[ch] == SV_TRUE) {
                VI_ReleaseChnFrame(s32Dev, ch, &astLatest[ch]);
                abLatestValid[ch] = SV_FALSE;
                memset(&astLatest[ch], 0, sizeof(astLatest[ch]));
            }
            pthread_mutex_unlock(&astLatestLock[ch]);
        }

        for (SV_S32 ch = 0; ch < 4; ++ch) {
            if (abChnEnabled[ch] == SV_TRUE) {
                VI_DisableChn(s32Dev, ch);
                abChnEnabled[ch] = SV_FALSE;
            }
        }
        if (bDevEnabled) { VI_DisableDev(s32Dev); bDevEnabled = SV_FALSE; }
        if (bViInited) { VI_DeInit(); bViInited = SV_FALSE; }
        if (bVbInited) { VB_Exit(); bVbInited = SV_FALSE; }
        if (bSysInited) { SYS_Exit(); bSysInited = SV_FALSE; }
    }

private:
    struct CaptureThreadArg {
        MppViFrameSource* pstSelf;
        SV_S32 s32Chn;
    };

    static void* CaptureThreadEntry(void* arg) {
        CaptureThreadArg* pstArg = reinterpret_cast<CaptureThreadArg*>(arg);
        if (pstArg != NULL && pstArg->pstSelf != NULL) {
            pstArg->pstSelf->CaptureThreadLoop(pstArg->s32Chn);
        }
        return NULL;
    }

    void CaptureThreadLoop(SV_S32 ch) {
        while (bTaskRun == SV_TRUE) {
            VideoFrameInfo stFrame;
            memset(&stFrame, 0, sizeof(stFrame));
            S32 ret = VI_GetChnFrame(s32Dev, ch, &stFrame, s32TimeoutMs);
            if (ret != 0) {
                if ((as32MissLogCount[ch]++ % 120) == 0)
                    LOG(WARNING) << "VI_GetChnFrame ch" << ch << " failed in capture thread: " << ret;
                continue;
            }

            SV_S32 s32Fd = (SV_S32)stFrame.stVFrame.u32Fd[0];
            if (s32Fd <= 0 && stFrame.ulBufferId != 0) {
                (void)VB_GetDmaBufFd(stFrame.ulBufferId, &s32Fd);
            }
            if (s32Fd <= 0) {
                LOG(WARNING) << "VI ch" << ch << " captured frame has no dma-buf fd";
                VI_ReleaseChnFrame(s32Dev, ch, &stFrame);
                continue;
            }

            pthread_mutex_lock(&astLatestLock[ch]);
            if (abLatestValid[ch] == SV_TRUE) {
                VI_ReleaseChnFrame(s32Dev, ch, &astLatest[ch]);
            }
            astLatest[ch] = stFrame;
            abLatestValid[ch] = SV_TRUE;
            pthread_mutex_unlock(&astLatestLock[ch]);
        }
    }

    SV_BOOL bSysInited;
    SV_BOOL bVbInited;
    SV_BOOL bViInited;
    SV_BOOL bDevEnabled;
    SV_BOOL bTaskRun;
    SV_BOOL abChnConfigured[4];
    SV_BOOL abChnEnabled[4];
    SV_BOOL abThreadStarted[4];
    SV_BOOL abPendingValid[4];
    SV_BOOL abLatestValid[4];
    SV_S32 as32MissLogCount[4];
    pthread_t ahThread[4];
    pthread_mutex_t astLatestLock[4];
    CaptureThreadArg astThreadArg[4];
    VideoFrameInfo astPending[4];
    VideoFrameInfo astLatest[4];
    SV_S32 s32Dev;
    SV_S32 s32TimeoutMs;
    SV_S32 s32Width;
    SV_S32 s32Height;
};
#else
class MppViFrameSource {
public:
    SV_BOOL Open(const SvRunConfig&) { LOG(ERROR) << "SV_AVM_ENABLE_MPP is not enabled in this build"; return SV_FALSE; }
    SV_BOOL CaptureFrames(std::vector<SV_IMAGE_S>*) { return SV_FALSE; }
    void ReleasePending() {}
    void Close() {}
};
#endif

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    SvRunConfig stCfg;
    LoadConfigJson("../config.json", &stCfg);
    ApplyArgs(argc, argv, &stCfg);
    if (stCfg.bLiveVi) stCfg.bZeroCopy = SV_TRUE;

    LOG(INFO) << "Config: zero_copy=" << (stCfg.bZeroCopy ? "true" : "false")
              << ", live_vi=" << (stCfg.bLiveVi ? "true" : "false")
              << ", frames=" << stCfg.s32Frames
              << ", sleep_us=" << stCfg.s32SleepUs
              << ", grid_subdiv=" << stCfg.s32GridSubdiv
              << ", live_vi=" << stCfg.s32LiveViWidth << "x" << stCfg.s32LiveViHeight
              << " timeout=" << stCfg.s32LiveViTimeoutMs << "ms"
              << ", use_fallback_image=" << (stCfg.bUseFallbackImage ? "true" : "false")
              << ", force_offscreen=" << (stCfg.bForceOffscreen ? "true" : "false")
              << ", fallback_image_dir=" << stCfg.strFallbackImageDir
              << ", offscreen_output_path="
              << (stCfg.strOffscreenOutputPath.empty() ? "(disabled)" : stCfg.strOffscreenOutputPath);

    const char* s8XmlFile = "../sv_avm_test/_aParam.xml";
    const char* s8DaeFile = "../sv_avm_test/res/concept_BUS cycles.dae";
    SV_F32 f32Translucency = 0.8;
    SV_BOWL_GRID_PARAM_S stGridParam = {1.7, (SV_S32)stCfg.s32GridSubdiv, 100, 0.05, 0.7};

    SV_SIZE_S stVehicleSize;
    std::vector<SV_CAMERA_PARAMS_S> stCameraParamsVector;
    LoadParamsFromXml(s8XmlFile, &stVehicleSize, &stCameraParamsVector);

    // Must precede InnerSV_CreateDisplay: it decides whether a missing display
    // falls back to offscreen rendering instead of aborting.
    if (!stCfg.strOffscreenOutputPath.empty()) {
        svrender::display::InnerSV_SetOffscreenConfig(
            ResolveRepoPath(stCfg.strOffscreenOutputPath).c_str(),
            stCfg.s32LiveViWidth, stCfg.s32LiveViHeight);
        svrender::display::InnerSV_SetForceOffscreen(stCfg.bForceOffscreen);
    } else if (SV_TRUE == stCfg.bForceOffscreen) {
        LOG(ERROR) << "force_offscreen=true but offscreen_output_path is empty; ignoring force_offscreen";
    }

    svrender::display::InnerSV_CreateDisplay(NULL, NULL);
    const SV_BOOL bOffscreen = svrender::display::InnerSV_bIsOffscreenMode();
    SV_SIZE_S stSize = svrender::display::InnerSV_GetDisplayFrameSize();
    LOG(INFO) << "Render target: " << (bOffscreen ? "offscreen" : "display")
              << ", size: " << stSize.s32Width << "x" << stSize.s32Height;

    svrender::mvp::InnerSV_MvCalss stMvClass;
    stMvClass.Initialized();

    svrender::vehicle::InnerSV_VehicleRenderClass stVehicleRenderClass(&stMvClass);
    stVehicleRenderClass.Init(s8DaeFile, stVehicleSize, f32Translucency);

    svrender::camera::InnerSv_CameraRenderClass stCameraRenderClass(&stMvClass);
    stCameraRenderClass.Init(stCameraParamsVector, stVehicleSize, stGridParam);

    std::vector<SV_IMAGE_S> stOwnedFrames;
    std::vector<SV_IMAGE_S> stImageVect;
    MppViFrameSource stLiveSource;

    SV_BOOL bCameraLive = SV_FALSE;
    if (stCfg.bLiveVi) {
        // Open the camera first: its availability decides whether per-frame
        // channels fall back to all-zero buffers or to static JPG images.
        // use_fallback_image skips the camera entirely and forces the images.
        if (SV_TRUE == stCfg.bUseFallbackImage) {
            LOG(INFO) << "use_fallback_image=true; skipping camera and using static images";
        } else {
            bCameraLive = stLiveSource.Open(stCfg);
        }
        if (SV_TRUE == bCameraLive) {
            LOG(INFO) << "Preparing 4 all-zero UYVY dma-buf fallback frames";
            for (SV_S32 i = 0; i < 4; ++i) {
                SV_IMAGE_S stZero;
                if (SV_TRUE != CreateZeroDmaFrame(stCfg.s32LiveViWidth, stCfg.s32LiveViHeight, SV_IMAGE_TYPE_UYVY, &stZero)) {
                    LOG(ERROR) << "Failed to create zero dma-buf fallback frame for channel " << i;
                    ReleaseCameraFrames(stOwnedFrames);
                    svrender::display::InnerSV_DeleteDisplay(0);
                    google::ShutdownGoogleLogging();
                    return 1;
                }
                stOwnedFrames.push_back(stZero);
            }
        } else {
            if (SV_TRUE != stCfg.bUseFallbackImage) {
                LOG(WARNING) << "Live VI open failed; rendering static fallback images instead";
            }
            LoadFallbackUYVYFrames(stCfg, &stOwnedFrames);
            if (stOwnedFrames.size() != 4) {
                LOG(ERROR) << "Expected 4 fallback frames, got " << stOwnedFrames.size();
                ReleaseCameraFrames(stOwnedFrames);
                svrender::display::InnerSV_DeleteDisplay(0);
                google::ShutdownGoogleLogging();
                return 1;
            }
        }
        stImageVect = stOwnedFrames;
    } else {
        stOwnedFrames = LoadCameraFrames(stCfg.bZeroCopy);
        stImageVect = stOwnedFrames;
        if (stImageVect.size() != 4) {
            LOG(WARNING) << "Expected 4 camera images, got " << stImageVect.size();
        }
    }

    const SV_S32 s32ScreenW = stSize.s32Width;
    const SV_S32 s32ScreenH = stSize.s32Height;
    const SV_S32 s32View3DW = s32ScreenH;
    const SV_S32 s32View2DX = s32View3DW;
    const SV_S32 s32View2DW = s32ScreenW - s32View3DW;
    const SV_RECT_S stView3D = {{0, 0}, {s32View3DW, s32ScreenH}};
    const SV_RECT_S stView2D = {{s32View2DX, 0}, {s32View2DW, s32ScreenH}};

    std::vector<SV_F64> vTexMs, vSubmitMs, vGpuWaitMs, vSwapMs, vFrameMs;
    #define SV_NOW(tv) gettimeofday(&(tv), NULL)
    #define SV_MS(a,b) (((b).tv_sec-(a).tv_sec)*1000.0 + ((b).tv_usec-(a).tv_usec)/1000.0)

    struct timeval stTvStart, stTvEnd;
    struct timeval stFpsReportStart;
    gettimeofday(&stTvStart, NULL);
    stFpsReportStart = stTvStart;
    SV_S32 s32FrameCount = 0;
    SV_S32 s32FpsReportFrame = 0;
    SV_F64 f64WinTexSum = 0.0, f64WinSubmitSum = 0.0, f64WinGpuWaitSum = 0.0;
    SV_F64 f64WinSwapSum = 0.0, f64WinRenderSum = 0.0;
    while (!g_bExit && (stCfg.s32Frames <= 0 || s32FrameCount < stCfg.s32Frames)) {
        if (SV_TRUE == bCameraLive) {
            stImageVect = stOwnedFrames; // default every channel to the all-zero dma-buf for this frame
            stLiveSource.CaptureFrames(&stImageVect);
        }

        struct timeval t0, t1, t2, t3, t4, t5;
        SV_NOW(t0);
        svrender::display::InnerSV_DisplayClear();

        stCameraRenderClass.GenCameraTextrue(stImageVect);
        SV_NOW(t1);

        stCameraRenderClass.Render(svrender::mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_3D, stView3D);
        stVehicleRenderClass.Render(svrender::mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_3D, stView3D);
        stCameraRenderClass.Render(svrender::mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_2D, stView2D);
        stVehicleRenderClass.Render(svrender::mvp::InnerSV_MvCalss::SV_ENUM_VIEWMODE_2D, stView2D);
        SV_NOW(t2);

        glFinish();
        SV_NOW(t3);

        svrender::display::InnerSV_DisplaySwap();
        SV_NOW(t4);

        if (SV_TRUE == bCameraLive) stLiveSource.ReleasePending();
        if (stCfg.s32SleepUs > 0) usleep(stCfg.s32SleepUs);
        SV_NOW(t5);

        SV_F64 f64Tex=SV_MS(t0,t1), f64Submit=SV_MS(t1,t2), f64GpuWait=SV_MS(t2,t3),
               f64Swap=SV_MS(t3,t4), f64Frame=SV_MS(t0,t4);
        vTexMs.push_back(f64Tex); vSubmitMs.push_back(f64Submit); vGpuWaitMs.push_back(f64GpuWait);
        vSwapMs.push_back(f64Swap); vFrameMs.push_back(f64Frame);
        f64WinTexSum += f64Tex;
        f64WinSubmitSum += f64Submit;
        f64WinGpuWaitSum += f64GpuWait;
        f64WinSwapSum += f64Swap;
        f64WinRenderSum += f64Frame;
        ++s32FrameCount;
        if ((s32FrameCount % 100) == 0) {
            struct timeval stFpsNow;
            gettimeofday(&stFpsNow, NULL);
            SV_S32 s32WinFrames = s32FrameCount - s32FpsReportFrame;
            SV_F64 f64WinSec = (stFpsNow.tv_sec - stFpsReportStart.tv_sec) +
                (stFpsNow.tv_usec - stFpsReportStart.tv_usec) / 1000000.0;
            SV_F64 f64WinFps = (f64WinSec > 0.0) ? (s32WinFrames / f64WinSec) : 0.0;
            SV_F64 f64InvWinFrames = (s32WinFrames > 0) ? (1.0 / s32WinFrames) : 0.0;
            SV_F64 f64WinRenderAvg = f64WinRenderSum * f64InvWinFrames;
            SV_F64 f64WinRenderFps = (f64WinRenderAvg > 0.0) ? (1000.0 / f64WinRenderAvg) : 0.0;
            LOG(INFO) << "Window stats: frames=" << s32FpsReportFrame << "-" << (s32FrameCount - 1)
                      << ", count=" << s32WinFrames
                      << ", elapsed=" << f64WinSec << " s"
                      << ", wall_fps=" << f64WinFps
                      << ", render_fps=" << f64WinRenderFps
                      << ", tex_avg=" << (f64WinTexSum * f64InvWinFrames) << " ms"
                      << ", submit_avg=" << (f64WinSubmitSum * f64InvWinFrames) << " ms"
                      << ", gpu_wait_avg=" << (f64WinGpuWaitSum * f64InvWinFrames) << " ms"
                      << ", swap_avg=" << (f64WinSwapSum * f64InvWinFrames) << " ms"
                      << ", render_total_avg=" << f64WinRenderAvg << " ms";
            stFpsReportStart = stFpsNow;
            s32FpsReportFrame = s32FrameCount;
            f64WinTexSum = 0.0;
            f64WinSubmitSum = 0.0;
            f64WinGpuWaitSum = 0.0;
            f64WinSwapSum = 0.0;
            f64WinRenderSum = 0.0;
        }
    }
    gettimeofday(&stTvEnd, NULL);

    SV_F64 f64ElapsedSec = (stTvEnd.tv_sec - stTvStart.tv_sec) + (stTvEnd.tv_usec - stTvStart.tv_usec) / 1000000.0;
    SV_F64 f64WallFps = (f64ElapsedSec > 0) ? (s32FrameCount / f64ElapsedSec) : 0.0;
    SV_F64 f64RenderMsSum = 0.0;
    for (SV_F64 x : vFrameMs) f64RenderMsSum += x;
    SV_F64 f64RenderFps = (f64RenderMsSum > 0.0) ? (1000.0 * vFrameMs.size() / f64RenderMsSum) : 0.0;
    LOG(INFO) << "Rendered " << s32FrameCount << " frames in " << f64ElapsedSec
              << " s, wall_fps=" << f64WallFps
              << ", render_fps=" << f64RenderFps;

    struct StatItem { const char* name; std::vector<SV_F64>* v; };
    StatItem items[] = {
        {"tex", &vTexMs}, {"submit", &vSubmitMs}, {"gpu_wait", &vGpuWaitMs},
        {"swap", &vSwapMs}, {"render_total", &vFrameMs},
    };
    LOG(INFO) << "================ timing stats ================";
    for (auto& it : items) {
        const std::vector<SV_F64>& v = *it.v;
        if (v.empty()) continue;
        SV_F64 sum=0, mn=v[0], mx=v[0];
        for (SV_F64 x : v) { sum+=x; if(x<mn)mn=x; if(x>mx)mx=x; }
        LOG(INFO) << "  " << it.name << ": avg=" << (sum/v.size()) << " ms, min=" << mn << " ms, max=" << mx << " ms";
    }

    #undef SV_NOW
    #undef SV_MS

    // Save the last rendered frame once the loop ends (frame budget reached or
    // Ctrl+C). Offscreen mode skips eglSwapBuffers, so the buffer is still readable.
    if (SV_TRUE == svrender::display::InnerSV_bIsOffscreenMode()) {
        (void)svrender::display::InnerSV_bSaveOffscreenFrame();
    }

    stLiveSource.Close();
    ReleaseCameraFrames(stOwnedFrames);
    svrender::display::InnerSV_DeleteDisplay(0);
    google::ShutdownGoogleLogging();
    return 0;
}
