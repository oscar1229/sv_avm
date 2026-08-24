/*
 * camerarender.cpp
 *
 */
#include "camerarender.hpp"
#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <drm/drm_fourcc.h>
#include <iomanip>
#include <sstream>

#include "src/svrender/common/viewtransoform/viewtransform.hpp"
#include "src/svrender/common/shader/shader.hpp"
#include "src/svrender/display/display.hpp"
#include "mesh/mesh.hpp"

namespace sm {
namespace sv_avm {
namespace svrender {
namespace camera {
namespace _local {
//零拷贝路径用到的EGL/GL扩展函数指针(运行时获取,避免链接期强依赖)
static PFNEGLCREATEIMAGEKHRPROC            pEglCreateImageKHR    = NULL;
static PFNEGLDESTROYIMAGEKHRPROC           pEglDestroyImageKHR   = NULL;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC pGlImageTargetTex2DOES= NULL;
static SV_BOOL bZeroCopyProcsLoaded = SV_FALSE;
//@brief 载入零拷贝所需扩展函数指针;全部可取到返回SV_TRUE
static SV_BOOL InnerSV_LoadZeroCopyProcs() {
  if(bZeroCopyProcsLoaded) return SV_TRUE;
  pEglCreateImageKHR     = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  pEglDestroyImageKHR    = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  pGlImageTargetTex2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
  bZeroCopyProcsLoaded = (pEglCreateImageKHR && pEglDestroyImageKHR && pGlImageTargetTex2DOES) ? SV_TRUE : SV_FALSE;
  return bZeroCopyProcsLoaded;
}

static SV_U32 InnerSV_DefaultPlanePitch(const SV_IMAGE_S& stImage, SV_S32 s32Plane) {
  if(s32Plane >= 0 && s32Plane < 2 && stImage.u32Stride[s32Plane] > 0)
    return stImage.u32Stride[s32Plane];
  switch(stImage.s32ImageType) {
    case SV_IMAGE_TYPE_UYVY:
      return (SV_U32)stImage.stImageSize.s32Width * 2U;
    case SV_IMAGE_TYPE_NV12:
      return (SV_U32)stImage.stImageSize.s32Width;
    default:
      return (SV_U32)stImage.stImageSize.s32Width;
  }
}

static SV_U32 InnerSV_DefaultPlaneOffset(const SV_IMAGE_S& stImage, SV_S32 s32Plane) {
  if(s32Plane <= 0)
    return stImage.u32PlaneOffset[0];
  if(stImage.u32PlaneOffset[s32Plane] > 0)
    return stImage.u32PlaneOffset[s32Plane];
  if(stImage.s32ImageType == SV_IMAGE_TYPE_NV12)
    return InnerSV_DefaultPlanePitch(stImage, 0) * (SV_U32)stImage.stImageSize.s32Height;
  return 0;
}

static SV_BOOL InnerSV_GetDmaBufImportInfo(const SV_IMAGE_S& stImage,
    EGLint* ps32Fourcc, SV_S32* ps32PlaneNum, EGLint* ps32ColorSpace) {
  if(ps32Fourcc == NULL || ps32PlaneNum == NULL || ps32ColorSpace == NULL)
    return SV_FALSE;
  switch(stImage.s32ImageType) {
    case SV_IMAGE_TYPE_NV12:
      *ps32Fourcc = (EGLint)DRM_FORMAT_NV12;
      *ps32PlaneNum = 2;
      *ps32ColorSpace = EGL_ITU_REC601_EXT;
      return SV_TRUE;
    case SV_IMAGE_TYPE_UYVY:
      *ps32Fourcc = (EGLint)DRM_FORMAT_UYVY;
      *ps32PlaneNum = 1;
      *ps32ColorSpace = EGL_ITU_REC709_EXT;
      return SV_TRUE;
    default:
      return SV_FALSE;
  }
}
//@brief 生成摄像头的顶点数组向量
//@param in stCameraParamsVector 摄像头内参向量
//       in stVehicleSize 车型尺寸参数
//       in stGridParam 碗面网格参数
//       out stCamerasMeshVect 所有摄像头的顶点数组向量
//       out s32MeshSizeVect 各摄像头的mesh元素个数
SV_VOID InnerSV_CreateMesh(const std::vector<SV_CAMERA_PARAMS_S> &stCameraParamsVector, \
    const SV_SIZE_S& stVehicleSize,const SV_BOWL_GRID_PARAM_S& stGridParam,
    std::vector<std::vector<mesh::SV_MESH_S> >* pstCamerasMeshVect,
    std::vector<SV_S32>* s32MeshSizeVect);
//@brief 生成摄像头的2D纹理对象向量
//@param in s32CameraNumber 摄像头通道数
//       out pu32TexObjVect 摄像头的2D纹理对象向量
SV_VOID Textrue2DInit(const SV_S32& s32CameraNumber,std::vector<SV_U32>* pu32TexObjVect);
//@brief 创建所有摄像头的opengl 顶点缓冲区对象
//@param in stCamerasMeshVect 摄像头的顶点数组向量
//       out pu32VAOVect 顶点缓冲区对象向量
SV_VOID InnerSv_CreateVaoVect(std::vector<std::vector<mesh::SV_MESH_S> >& stCamerasMeshVect,
    std::vector<SV_U32>* pu32VAOVect);
//@brief 摄像头单视图顶点缓冲区对象向量
//@param out pu32VaoVect
//@remark 左右前为原像，后视为镜像
//                 3   4
//                1  2
//         1 2 3 构成三角形1；2 4 3 构成三角形2
SV_VOID InnerSv_CreateCameraOriginVaoVect(std::vector<SV_U32>* pu32VaoVect);
//@brief 实际的加载顶点对象绘图过程
//@param in u32Vao 顶点对象缓存
//       in 用于生成u32Vao的SV_MESH_S向量的Size
//       in u32TextObject 纹理对象
//       in u32MvpUniform OpenGL着色器全局量
//       in mvp mvp矩阵
//       in stImage 帧图像数据
static SV_VOID InnerSV_RenderProcess(const SV_U32& u32Vao,const SV_S32 &s32MeshSize,
    const SV_U32& u32TextObject,const SV_BOOL& bOES=SV_FALSE);
//@brief 计算实际网格量占理论上限的百分比
//@param in s64Actual 实际值 in s64Theory 理论上限
//@return 百分比,s64Theory为0时返回0
static inline SV_F32 f32MeshUsage(const SV_S64& s64Actual,const SV_S64& s64Theory) {
  return s64Theory>0 ? (100.0f*static_cast<SV_F32>(s64Actual)/static_cast<SV_F32>(s64Theory)) : 0.0f;
}
}

SV_BOOL InnerSv_CameraRenderClass::Init(const std::vector<SV_CAMERA_PARAMS_S> &stCameraParamsVector, \
    const SV_SIZE_S& stVehicleSize ,const SV_BOWL_GRID_PARAM_S& stGridParam) {
  SV_BOOL bRet = ProgramInit();
  if(SV_FALSE== bRet) {
    return bRet;
  }
  CameraTextInit(stCameraParamsVector,stVehicleSize,stGridParam);
  return SV_TRUE;
}
SV_VOID InnerSv_CameraRenderClass::GenCameraTextrue(const std::vector<SV_IMAGE_S> &img) {
  if(img.size()< u32VAOVect.size())
    return;
  //若所有通道都带有效dma_fd,优先走零拷贝路径(NV12 dma_buf -> EGLImage -> external纹理)
  SV_BOOL bAllHaveDmaFd = (img.size()>0) ? SV_TRUE : SV_FALSE;
  for(size_t i=0;i<img.size();++i) {
    if(img[i].s32DmaFd <= 0) { bAllHaveDmaFd = SV_FALSE; break; }
  }
  if(SV_TRUE == bAllHaveDmaFd && SV_TRUE == GenCameraTextrueZeroCopy(img)) {
    bUseOES = SV_TRUE;//本帧用external纹理,Render走OES program
    return;
  }
  bUseOES = SV_FALSE;//回退普通拷贝上传路径
  //首次进入时初始化各纹理的"已分配尺寸"记录为0(表示尚未分配存储)
  if(stTexAllocSize.size() < img.size()) {
    stTexAllocSize.resize(img.size(), SV_SIZE_S{0, 0});
  }
  for(SV_S32 i=0;i<img.size();++i) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, u32TexObjVect[i]);
    //glUniform1i(u32MvpUniform, 0);
  #ifdef CAMERA_USED
    glTexDirectVIVMap(GL_TEXTURE_2D,img[i].stImageSize.s32Width, img[i].stImageSize.s32Height,  GL_VIV_YV12, (GLvoid **)&img[i].dataPtr, (const GLuint *)(&img[i].u32Offset));
    glTexDirectInvalidateVIV(GL_TEXTURE_2D);
  #else
    const SV_S32 s32W = img[i].stImageSize.s32Width;
    const SV_S32 s32H = img[i].stImageSize.s32Height;
    //RGB(3字节/像素)行无4字节对齐,设为1避免驱动走对齐慢路径
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if(stTexAllocSize[i].s32Width != s32W || stTexAllocSize[i].s32Height != s32H) {
      //尺寸首次确定或发生变化:分配纹理存储; dataPtr为空时分配一张空纹理作为保护回退
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, s32W, s32H, 0, GL_RGB, GL_UNSIGNED_BYTE, img[i].dataPtr);
      stTexAllocSize[i].s32Width = s32W;
      stTexAllocSize[i].s32Height = s32H;
    } else if(img[i].dataPtr != NULL) {
      //尺寸不变:复用已分配存储,仅更新像素数据,避免每帧重分配
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, s32W, s32H, GL_RGB, GL_UNSIGNED_BYTE, img[i].dataPtr);
    }
  #endif
    glBindTexture(GL_TEXTURE_2D,0);
  }

}

//零拷贝路径:把每个通道的NV12 dma_buf导入为EGLImage并绑定到external纹理
//dma_fd未变化的通道复用已有EGLImage,避免每帧重建
SV_BOOL InnerSv_CameraRenderClass::GenCameraTextrueZeroCopy(const std::vector<SV_IMAGE_S> &img) {
  if(SV_FALSE == _local::InnerSV_LoadZeroCopyProcs()) // 加载拓展函数指针
    return SV_FALSE;
  EGLDisplay dpy = reinterpret_cast<EGLDisplay>(display::InnerSV_GetEglDisplay()); // 取EGLDisplay
  if(EGL_NO_DISPLAY == dpy)
    return SV_FALSE;
  const size_t n = img.size();
  if(u32TexObjOESVect.size() < n || s32EglImageFdVect.size() < n || pEglImageVect.size() < n)
    return SV_FALSE;

  for(size_t i=0;i<n;++i) {
    const SV_S32 s32W = img[i].stImageSize.s32Width;
    const SV_S32 s32H = img[i].stImageSize.s32Height;
    const SV_U32 u32Offset0 = _local::InnerSV_DefaultPlaneOffset(img[i], 0);
    const SV_U32 u32Offset1 = _local::InnerSV_DefaultPlaneOffset(img[i], 1);
    const SV_U32 u32Pitch0  = _local::InnerSV_DefaultPlanePitch(img[i], 0);
    const SV_U32 u32Pitch1  = _local::InnerSV_DefaultPlanePitch(img[i], 1);
    EGLint s32Fourcc = 0;
    EGLint s32ColorSpace = EGL_ITU_REC601_EXT;
    SV_S32 s32PlaneNum = 0;

    if(img[i].s32DmaFd <= 0 || s32W <= 0 || s32H <= 0)
      return SV_FALSE;
    if(SV_FALSE == _local::InnerSV_GetDmaBufImportInfo(img[i], &s32Fourcc, &s32PlaneNum, &s32ColorSpace))
      return SV_FALSE;

    //dma_buf属性未变则复用已导入的EGLImage,无需重建
    if(s32EglImageFdVect[i] == img[i].s32DmaFd &&
       s32EglImageTypeVect[i] == img[i].s32ImageType &&
       stEglImageSizeVect[i].s32Width == s32W &&
       stEglImageSizeVect[i].s32Height == s32H &&
       u32EglImageStride0Vect[i] == u32Pitch0 &&
       u32EglImageStride1Vect[i] == u32Pitch1 &&
       u32EglImageOffset0Vect[i] == u32Offset0 &&
       u32EglImageOffset1Vect[i] == u32Offset1 &&
       pEglImageVect[i] != NULL)
      continue;

    //fd或布局变化:先销毁旧EGLImage
    if(pEglImageVect[i] != NULL) {
      _local::pEglDestroyImageKHR(dpy, (EGLImageKHR)pEglImageVect[i]);
      pEglImageVect[i] = NULL;
    }

    EGLint attribs[32];
    SV_S32 a = 0;
    attribs[a++] = EGL_WIDTH;  attribs[a++] = s32W;
    attribs[a++] = EGL_HEIGHT; attribs[a++] = s32H;
    attribs[a++] = EGL_LINUX_DRM_FOURCC_EXT; attribs[a++] = s32Fourcc;
    attribs[a++] = EGL_DMA_BUF_PLANE0_FD_EXT;     attribs[a++] = img[i].s32DmaFd;
    attribs[a++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT; attribs[a++] = (EGLint)u32Offset0;
    attribs[a++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;  attribs[a++] = (EGLint)u32Pitch0;
    if(s32PlaneNum > 1) {
      attribs[a++] = EGL_DMA_BUF_PLANE1_FD_EXT;     attribs[a++] = img[i].s32DmaFd;
      attribs[a++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT; attribs[a++] = (EGLint)u32Offset1;
      attribs[a++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;  attribs[a++] = (EGLint)u32Pitch1;
    }
    attribs[a++] = EGL_SAMPLE_RANGE_HINT_EXT;     attribs[a++] = EGL_YUV_NARROW_RANGE_EXT;
    attribs[a++] = EGL_YUV_COLOR_SPACE_HINT_EXT;  attribs[a++] = s32ColorSpace;
    attribs[a++] = EGL_NONE;

    //导入dma_buf为EGLImage;像素仍在原dma_buf里,这里只建立GPU可采样视图
    EGLImageKHR image = _local::pEglCreateImageKHR(dpy, EGL_NO_CONTEXT,
        EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)0, attribs);
    if(EGL_NO_IMAGE_KHR == image) {
      LOG(ERROR) << "eglCreateImageKHR failed, ch=" << i
                 << " fd=" << img[i].s32DmaFd
                 << " type=" << img[i].s32ImageType
                 << " size=" << s32W << "x" << s32H
                 << " pitch0=" << u32Pitch0
                 << " pitch1=" << u32Pitch1
                 << " eglErr=0x" << std::hex << eglGetError() << std::dec;
      return SV_FALSE;
    }
    //绑定到external纹理
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, u32TexObjOESVect[i]);
    _local::pGlImageTargetTex2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)image);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    pEglImageVect[i] = (void*)image;
    s32EglImageFdVect[i] = img[i].s32DmaFd;
    s32EglImageTypeVect[i] = img[i].s32ImageType;
    stEglImageSizeVect[i] = img[i].stImageSize;
    u32EglImageStride0Vect[i] = u32Pitch0;
    u32EglImageStride1Vect[i] = u32Pitch1;
    u32EglImageOffset0Vect[i] = u32Offset0;
    u32EglImageOffset1Vect[i] = u32Offset1;
  }
  return SV_TRUE;
}
SV_VOID InnerSv_CameraRenderClass::Render(const SV_S32& s32ViewMode,const SV_RECT_S& stViewPoint) {
  glm::mat4 mvp = this->gpclMvClass->GetCameraMvpMatrix(s32ViewMode);
  //根据本帧纹理类型选择着色器程序:零拷贝走OES程序(采样external纹理),否则走普通程序
  const SV_BOOL bOES = (SV_TRUE==bUseOES && clProgramOES.GetHandle()!=0) ? SV_TRUE : SV_FALSE;
  glUseProgram(bOES ? this->clProgramOES.GetHandle() : this->clProgram.GetHandle());
  //glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glUniformMatrix4fv(bOES ? u32MvpUniformOES : u32MvpUniform, 1, GL_FALSE, glm::value_ptr(mvp));
  glEnable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glViewport(stViewPoint.stStartPoint.s32X,stViewPoint.stStartPoint.s32Y,stViewPoint.stRectSize.s32Width,stViewPoint.stRectSize.s32Height);//视口1
  for(SV_S32 i=0;i< u32VAOVect.size();++i) {
   _local::InnerSV_RenderProcess(u32VAOVect[i],s32MeshSize[i],
       bOES ? u32TexObjOESVect[i] : u32TexObjVect[i], bOES);
  }
  return;
}


SV_VOID InnerSv_CameraRenderClass::RenderSingleChl(const SV_S32 &s32Chnl,const SV_RECT_S& stViewPoint) {
  glUseProgram(this->clProgram.GetHandle());
  glUniformMatrix4fv(u32MvpUniform, 1, GL_FALSE, glm::value_ptr(kstCameraOriginMvp));
  glDisable(GL_DEPTH_TEST);
  glViewport(stViewPoint.stStartPoint.s32X,stViewPoint.stStartPoint.s32Y,stViewPoint.stRectSize.s32Width,stViewPoint.stRectSize.s32Height);//视口1
  _local::InnerSV_RenderProcess(u32CameraOrigiVaoVect[s32Chnl],6,
        u32TexObjVect[s32Chnl]);
  glUseProgram(0);
  return;
}

SV_BOOL InnerSv_CameraRenderClass::ProgramInit(SV_VOID) {
  std::string s8_v_shader_str = glshader::s_v_shader_glm;
   std::string s8_f_shader_str = glshader::s_f_shader_ec;
  // return clProgram.LoadShaders(s8_v_shader_str,s8_f_shader_str);
   if(SV_FALSE == clProgram.LoadShaders(s8_v_shader_str,s8_f_shader_str)) {
     return SV_FALSE;
   }
   u32MvpUniform = glGetUniformLocation(clProgram.GetHandle(), "mvp");
   //零拷贝路径的着色器程序:复用同一顶点着色器,片元着色器换成采样external纹理的版本
   //加载失败不影响普通路径(仅零拷贝路径不可用),故不return失败
   std::string s8_f_shader_oes_str = glshader::s_f_shader_ec_oes;
   if(SV_TRUE == clProgramOES.LoadShaders(s8_v_shader_str,s8_f_shader_oes_str)) {
     u32MvpUniformOES = glGetUniformLocation(clProgramOES.GetHandle(), "mvp");
   }
   return SV_TRUE;
}

SV_VOID InnerSv_CameraRenderClass::CameraTextInit(const std::vector<SV_CAMERA_PARAMS_S> &stCameraParamsVector, \
    const SV_SIZE_S& stVehicleSize,const SV_BOWL_GRID_PARAM_S& stGridParam) 
{
  std::vector<std::vector<mesh::SV_MESH_S> > stCamerasMeshVect;
  stCamerasMeshVect.clear();
  //生成所有摄像头的顶点数组向量
  _local::InnerSV_CreateMesh(stCameraParamsVector,stVehicleSize,stGridParam,&stCamerasMeshVect,&s32MeshSize);
  //创建所有摄像头的opengl 顶点缓冲区对象
  u32VAOVect.clear();
  _local::InnerSv_CreateVaoVect(stCamerasMeshVect,&u32VAOVect);
  _local::InnerSv_CreateCameraOriginVaoVect(&u32CameraOrigiVaoVect);
  _local::Textrue2DInit(std::max(static_cast<SV_S32>(stCamerasMeshVect.size()),s32GetCameraChannelNumber()),&u32TexObjVect);
  //为零拷贝路径创建对应数量的external(OES)纹理对象,并初始化EGLImage跟踪向量
  SV_S32 s32TexNum = static_cast<SV_S32>(u32TexObjVect.size());
  u32TexObjOESVect.clear();
  for(SV_S32 i=0;i<s32TexNum;++i) {
    SV_U32 u32OesTex=0;
    glGenTextures(1,&u32OesTex);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES,u32OesTex);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES,0);
    u32TexObjOESVect.push_back(u32OesTex);
  }
  s32EglImageFdVect.assign(s32TexNum,-1);
  s32EglImageTypeVect.assign(s32TexNum,-1);
  stEglImageSizeVect.assign(s32TexNum, SV_SIZE_S{0, 0});
  u32EglImageStride0Vect.assign(s32TexNum, 0);
  u32EglImageStride1Vect.assign(s32TexNum, 0);
  u32EglImageOffset0Vect.assign(s32TexNum, 0);
  u32EglImageOffset1Vect.assign(s32TexNum, 0);
  pEglImageVect.assign(s32TexNum,(void*)NULL);
}

namespace _local 
{
SV_VOID InnerSV_CreateMesh(const std::vector<SV_CAMERA_PARAMS_S> &stCameraParamsVector, \
    const SV_SIZE_S& stVehicleSize,const SV_BOWL_GRID_PARAM_S& stGridParam,
    std::vector<std::vector<mesh::SV_MESH_S> >* stCamerasMeshVect,std::vector<SV_S32>* s32MeshSizeVect)
{
  SV_BOWL_GRID_PARAM_S stGridParamTmp = stGridParam;

  SV_S32 s32NopZ =  mesh::CalcGridBowlHeight(stCameraParamsVector, mesh::CalcHalfDiagLengthFromVehicleSize(stVehicleSize), stGridParam) ;
  stGridParamTmp.s32NopZ = s32NopZ;
  static const char* s8ChannlName[] = {"LEFT ","RIGHT","FRONT","BACK "};
  SV_S64 s64TriAll=0,s64TriTheoryAll=0;
  LOG(INFO)<<"[mesh] subdiv="<<stGridParam.s32Angles<<" nopz="<<stGridParam.s32NopZ
           <<" step="<<stGridParam.f32StepX<<" (bowl_h="<<s32NopZ<<")";
  LOG(INFO)<<"[mesh] ch      angles  tri_actual  tri_max   used     vbo";
  for(SV_S32 i=0;i< stCameraParamsVector.size();i++) 
  {
    mesh::SV_MESH_GEN_PARAM_S stGenParams = mesh::CalcMeshGenParam(stGridParam,stVehicleSize,i);;
    mesh::InnerSV_CameraMeshClass stCameraMeshClass(stGenParams);
    std::vector<mesh::SV_MESH_S> stMeshVect;
    stCameraMeshClass.GenGLMesh(stCameraParamsVector[i],&stMeshVect);
    //打印本通道实际渲染三角形数与配置暗示的理论上限
    const mesh::SV_MESH_STAT_S& stStat = stCameraMeshClass.stGetMeshStat();
    std::ostringstream ssAngles;
    ssAngles<<stStat.s32AngleUsed<<"/"<<stStat.s32Angles;
    LOG(INFO)<<"[mesh] "<<(i<4?s8ChannlName[i]:"?    ")
             <<std::setw(9)<<ssAngles.str()
             <<std::setw(12)<<stStat.s64TriangleActual
             <<std::setw(9)<<stStat.s64TriangleTheory
             <<std::setw(7)<<std::fixed<<std::setprecision(1)
             <<_local::f32MeshUsage(stStat.s64TriangleActual,stStat.s64TriangleTheory)<<"%"
             <<std::setw(7)<<(stStat.s64VertexActual*sizeof(mesh::SV_MESH_S)>>10)<<"KB";
    s64TriAll += stStat.s64TriangleActual;
    s64TriTheoryAll += stStat.s64TriangleTheory;
    stCamerasMeshVect->push_back(stMeshVect);
    s32MeshSizeVect->push_back(stMeshVect.size());
  }
  LOG(INFO)<<"[mesh] TOTAL         "<<std::setw(12)<<s64TriAll<<std::setw(9)<<s64TriTheoryAll
           <<std::setw(7)<<std::fixed<<std::setprecision(1)
           <<_local::f32MeshUsage(s64TriAll,s64TriTheoryAll)<<"%";
}

SV_VOID Textrue2DInit(const SV_S32& s32CameraNumber,std::vector<SV_U32>* pu32TexObjVect) {
  SV_S32 i=0;
  for(;i<s32CameraNumber;++i) {
    SV_U32 glutextrue;
    glGenTextures(1, &glutextrue);
    glBindTexture(GL_TEXTURE_2D, glutextrue);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    pu32TexObjVect->push_back(glutextrue);
  }
}

static SV_VOID InnerSV_MeshToVao(const std::vector<mesh::SV_MESH_S>& stMeshVect,
    SV_U32* pu32VAOVect ) {
  SV_U32 u32VBO;
  glGenBuffers(1, &u32VBO);
  glGenVertexArrays(1,pu32VAOVect);
  glBindBuffer(GL_ARRAY_BUFFER,u32VBO);
  glBufferData(GL_ARRAY_BUFFER, stMeshVect.size()*sizeof(stMeshVect[0]), &stMeshVect[0], GL_STATIC_DRAW);
  glBindVertexArray(*pu32VAOVect);
  glBindBuffer(GL_ARRAY_BUFFER, u32VBO);
  // Position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
  // TexCoord attribute
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);
}

SV_VOID InnerSv_CreateVaoVect(std::vector<std::vector<mesh::SV_MESH_S> >& stCamerasMeshVect,
std::vector<SV_U32>* pu32VAOVect) {
  SV_S32 i=0;
  for(;i<stCamerasMeshVect.size();++i) {
    SV_U32 u32Va0;
    InnerSV_MeshToVao(stCamerasMeshVect[i],&u32Va0);
    pu32VAOVect->push_back(u32Va0);
  }
  return;
}

SV_VOID InnerSv_CreateCameraOriginVaoVect(std::vector<SV_U32>* pu32VaoVect) {
  SV_S32 s32CameraNumber = s32GetCameraChannelNumber();
  // 1 2 3 构成三角形1；2 4 3 构成三角形2
  // 3 4
  // 1 2
  mesh::SV_MESH_S stMeshOrigin[6] ={{{-1,1,0},{0,0,1}},{{-1,-1,0},{0,1,1}},{{1,1,0},{1,0,1}}, \
      {{-1,-1,0},{0,1,1}},{{1,-1,0},{1,1,1}},{{1,1,0},{1,0,1}}};
  mesh::SV_MESH_S stMeshHmirror[6] ={{{-1,-1,0},{1,1,1}},{{1,-1,0},{0,1,1}},{{-1,1,0},{1,0,1}}, \
        {{1,-1,0},{0,1,1}},{{1,1,0},{0,0,1}},{{-1,1,0},{1,0,1}}};
  std::vector<mesh::SV_MESH_S> stMeshVectOrigin(stMeshOrigin,stMeshOrigin+6);
  std::vector<mesh::SV_MESH_S> stMeshVectHMirror(stMeshHmirror,stMeshHmirror+6);
  for(SV_S32 i=0;i<s32CameraNumber;++i) {
    SV_U32 u32Va0;
    InnerSV_MeshToVao(SV_ENUM_CAMERA_BACK==i?stMeshVectHMirror:stMeshVectOrigin,&u32Va0);
    pu32VaoVect->push_back(u32Va0);
  }
}

static SV_VOID InnerSV_RenderProcess(const SV_U32& u32Vao,const SV_S32 &s32VaoSize,
    const SV_U32& u32TextObject,const SV_BOOL& bOES) {

  glBindVertexArray(u32Vao);
  //零拷贝路径绑定external纹理,普通路径绑定2D纹理
  if(SV_TRUE == bOES)
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, u32TextObject);
  else
    glBindTexture(GL_TEXTURE_2D, u32TextObject);
  glDrawArrays(GL_TRIANGLES, 0, s32VaoSize);
  glBindVertexArray(0);
}


}//end of _local
}//end camera
}//end svrender
}//end sv_avm
}//end sm
