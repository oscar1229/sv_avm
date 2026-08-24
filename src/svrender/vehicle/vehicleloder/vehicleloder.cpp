/*
 * vehicleloder.cpp
 *
 */
#include"vehicleloder.hpp"

#include<stdio.h>
#include<stdlib.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GLES3/gl3.h>

#include "src/svrender/common/glm/gtc/matrix_transform.hpp"
#include "src/svrender/common/glm/gtc/type_ptr.hpp"
#include "vbo/vbo.hpp"

namespace sm {
namespace sv_avm {
namespace svrender {
namespace vehicle {
namespace vehicleloder {

#define AI_PROCESS_FLAG (aiProcess_JoinIdenticalVertices| \
        aiProcess_LimitBoneWeights| \
        aiProcess_PreTransformVertices | \
        aiProcess_Triangulate| \
        aiProcess_GenUVCoords| \
        aiProcess_SortByPType| \
        aiProcess_FindDegenerates| \
        aiProcess_FindInvalidData | \
        aiProcess_GenNormals)

#define CAR_ORIENTATION_X 90.0f
#define CAR_ORIENTATION_Y 270.0f

namespace _local {

enum {
  SV_ENUM_COLOR_AMBIENT =0,
  SV_ENUM_COLOR_DIFFUSE,
  SV_ENUM_COLOR_SPECULAR,
  SV_ENUM_COLOR_BUTT
};
//@brief 判断输入的文件是否为DAE文件
//@param in ps8DaeFilePath 全路径文件名
//@return SV_TRUE/SV_FALSE
static SV_BOOL InnerSV_bIsDaeFile(const SV_S8* ps8DaeFilePath);
//@brief 调用stImporter，读取dae车模文件
//@param in stImporter Assimp::Importer对象
//       in ps8DaeFilePath 全路径dae文件名
//       out ppAiScene
const static SV_BOOL InnerSV_bAssimpImportVehicleModelFromDae(Assimp::Importer& stImporter,
    const SV_S8* ps8DaeFilePath,aiScene** ppAiScene);
const static glm::vec3 InnerSV_stGetMProperFromAiaiMaterial(const aiMaterial* pstMaterial,
    SV_S32 s32Idx);
const static SV_F32 InnerSV_f32GetShininessFromAiaiMaterial(const aiMaterial* pstMaterial);
const static SV_VOID InnerSV_GetMaterilVectFromAiScene(const aiScene* psrScene,PST_MATERIA_VECT* pstVect);
const static glm::vec3 InnerSV_GetVehicleDaeDimensions(const aiScene* pstScene);
const static SV_VOID InnerSV_LoadVBOEachVehicleMesh(const aiMesh * mesh,vbo::InnerSV_VehicleVboClass* pstVbo);
}//end of namespace _local

InnerSV_VehicleLoader::InnerSV_VehicleLoader()
    : bInitialized(SV_FALSE), pclVbVector(NULL){
}

InnerSV_VehicleLoader::~InnerSV_VehicleLoader(void) {
  pclVbVector->clVect.clear();
  delete pclVbVector;
}

const SV_BOOL InnerSV_VehicleLoader::Initialize(const SV_S8* filepath,const SV_SIZE_S &stVehicleSize) {
  DLOG(INFO)<<__FUNCTION__;
  Assimp::Importer importer;
  importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
  aiScene* pstScene =NULL;
  if(SV_FALSE == _local::InnerSV_bAssimpImportVehicleModelFromDae(importer,filepath,&pstScene)) {
    return SV_FALSE;
  }
  _local::InnerSV_GetMaterilVectFromAiScene(pstScene,&this->pstMaterialVect);
  glm::vec3 stDaeSize = _local::InnerSV_GetVehicleDaeDimensions(pstScene);
  SV_F32 f32L = static_cast<SV_F32>(stVehicleSize.s32Height),f32W=static_cast<SV_F32>(stVehicleSize.s32Width);
  SV_F32 f32InvMxy = 1.0/std::max(f32L,f32W);
  SV_F32 f32NormalizeL = 2*f32L*f32InvMxy,f32NormalizeW = 2*f32W*f32InvMxy;
  stCarScal= glm::vec3(f32NormalizeL/stDaeSize[0],f32NormalizeW/stDaeSize[1],f32NormalizeW/stDaeSize[1]);
  DLOG(INFO)<<"stVehicleSize["<<stVehicleSize.s32Width
      <<","<<stVehicleSize.s32Height<<"]:stCarScal["
      <<stCarScal[0]<<","<<stCarScal[1]<<","<<stCarScal[2]<<"]";
  //加载加载全部VAO及其ID
  pclVbVector = new (std::nothrow) SV_VBO_VECT_S;
  if(NULL==pclVbVector) {
    LOG(ERROR) <<"new SV_VBO_VECT_S failed";
    return SV_FALSE;
  }
  pclVbVector->clVect.reserve(pstScene->mNumMeshes);
  for (SV_S32 i = 0; i < pstScene->mNumMeshes; ++i) {
    const aiMesh * pstMesh = pstScene->mMeshes[i];
    vbo::InnerSV_VehicleVboClass clVboClass;
    _local::InnerSV_LoadVBOEachVehicleMesh(pstMesh,&clVboClass);
    pclVbVector->clVect.push_back(clVboClass);
  }
  bInitialized=SV_TRUE;
  return SV_TRUE;
}

const SV_VOID InnerSV_VehicleLoader::Draw(SV_U32 s32Shader,const SV_U32 &u32AmbientLoc,\
    const SV_U32 &u32DiffuseLoc) {
  assert(SV_TRUE==bInitialized);
  for(SV_S32 i=0;i<pclVbVector->clVect.size();++i) {
    SV_U32 s32MatId = pclVbVector->clVect[i].u32GetId();
    glUniform3f(u32AmbientLoc, pstMaterialVect[i].stAmbient[0], pstMaterialVect[i].stAmbient[1], pstMaterialVect[i].stAmbient[2]);
    glUniform3f(u32DiffuseLoc, pstMaterialVect[i].stDiffuse[0], pstMaterialVect[i].stDiffuse[1], pstMaterialVect[i].stDiffuse[2]);
    glBindVertexArray(pclVbVector->clVect[i].u32GetVAO());
    glDrawArrays(GL_TRIANGLES, 0, pclVbVector->clVect[i].u32GetCount());
    glBindVertexArray( 0 );
  }
  return;
}

namespace _local {
static SV_BOOL InnerSV_bIsDaeFile(const SV_S8* ps8DaeFilePath) {
  SV_S8 *ext = strrchr(const_cast<SV_S8*>(ps8DaeFilePath),'.');
  if(NULL== ext)
    return SV_FALSE;
  if(0!= strcmp(".dae",ext))
    return SV_FALSE;
  return SV_TRUE;
}

const static SV_BOOL InnerSV_bAssimpImportVehicleModelFromDae(Assimp::Importer& stImporter,
    const SV_S8* ps8DaeFilePath,aiScene** ppAiScene) {
  DLOG(INFO)<<__FUNCTION__;
  //检查是否为dae文件
  if(SV_FALSE==InnerSV_bIsDaeFile(ps8DaeFilePath)) {
    LOG(ERROR)<<__FUNCTION__<<":"<<ps8DaeFilePath<<"is nor a dae";
    return SV_FALSE;
  }
   aiScene* scene = const_cast<aiScene*>(stImporter.ReadFile(ps8DaeFilePath,AI_PROCESS_FLAG));
    if (!scene)
    {
      LOG(ERROR)<<"Cannot open file"<<ps8DaeFilePath <<" with scene";
      return SV_FALSE;
    }
   *ppAiScene = scene;
   return SV_TRUE;
}

const static glm::vec3 InnerSV_stGetMProperFromAiaiMaterial(const aiMaterial* pstMaterial,
    SV_S32 s32Idx) {
  aiReturn eR;
  aiColor3D  stColor;
  if(SV_ENUM_COLOR_AMBIENT == s32Idx) {
    eR = pstMaterial->Get(AI_MATKEY_COLOR_AMBIENT, stColor);
  }
  else if(SV_ENUM_COLOR_DIFFUSE == s32Idx) {
    eR = pstMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, stColor);
  }
  else if(SV_ENUM_COLOR_SPECULAR == s32Idx) {
    eR = pstMaterial->Get(AI_MATKEY_COLOR_SPECULAR, stColor);
  }
  if(AI_SUCCESS!=eR) {
    LOG(ERROR)<<__FUNCTION__<<"Cannot load color Property";

  }
  return glm::vec3(stColor.r,stColor.g,stColor.b);
}

const static SV_F32 InnerSV_f32GetShininessFromAiaiMaterial(const aiMaterial* pstMaterial) {
  aiReturn eR;
  SV_F32 f32Tmp;
  pstMaterial->Get(AI_MATKEY_SHININESS, f32Tmp);
  if(AI_SUCCESS!=eR) {
    LOG(ERROR)<<__FUNCTION__<<"Cannot load shinining Property";
  }
  return  f32Tmp;
}



const static SV_VOID InnerSV_GetMaterilVectFromAiScene(const aiScene* psrScene,PST_MATERIA_VECT* pstVect) {
  DLOG(INFO)<<__FUNCTION__;
  pstVect->clear();
  for (SV_S64 i = 0; i < psrScene->mNumMaterials; i++) {
    //materials
    aiMaterial *pstM = psrScene->mMaterials[i];
    //get material name
    aiString name;
    pstM->Get(AI_MATKEY_NAME, name);
    //get material properties

    SV_MATERIAL_S stMat = { InnerSV_stGetMProperFromAiaiMaterial(pstM,SV_ENUM_COLOR_AMBIENT), \
                            InnerSV_stGetMProperFromAiaiMaterial(pstM,SV_ENUM_COLOR_DIFFUSE), \
                            InnerSV_stGetMProperFromAiaiMaterial(pstM,SV_ENUM_COLOR_SPECULAR), \
                            InnerSV_f32GetShininessFromAiaiMaterial(pstM)};
    pstVect->push_back(stMat);
  }
  DLOG(INFO)<<"pstVect:Size["<<pstVect->size()<<"]";
}

const static glm::vec3 InnerSV_GetVehicleDaeDimensions(const aiScene* pstScene) {
  DLOG(INFO)<<__FUNCTION__;
  SV_F32 f32Xmin=FLT_MAX,f32Xmax=FLT_MIN, \
      f32Ymin=FLT_MAX,f32Ymax=FLT_MIN, \
      f32Zmin=FLT_MAX,f32Zmax=FLT_MIN;
  aiMesh * mesh;
  for (unsigned int i = 0; i < pstScene->mNumMeshes; ++i)
  {
    mesh = pstScene->mMeshes[i];
    if (mesh->mNumFaces == 0)
      continue;
    // Loop through every face
    for (unsigned curr_face = 0; curr_face < mesh->mNumFaces; curr_face++)
    {
      aiFace *face = &mesh->mFaces[curr_face];

      //aiProcess_Triangulate is turned on, so looping through 3 vertices
      for (unsigned i = 0; i < 3; i++)
      {
        SV_POINT3F32_S v;
        v.f32X = mesh->mVertices[face->mIndices[i]].x;
        v.f32Y = mesh->mVertices[face->mIndices[i]].y;
        v.f32Z = mesh->mVertices[face->mIndices[i]].z;
        f32Xmin = std::min(v.f32X,f32Xmin);
        f32Xmax = std::max(v.f32X,f32Xmax);
        f32Ymin = std::min(v.f32Y,f32Ymin);
        f32Ymax = std::max(v.f32Y,f32Ymax);
        f32Zmin = std::min(v.f32Z,f32Zmin);
        f32Zmax = std::max(v.f32Z,f32Zmax);
      }
    }
  }
  DLOG(INFO)<<"DaeDimension:["<<f32Xmax-f32Xmin<<","<<f32Ymax-f32Ymin<<","<<f32Zmax-f32Zmin<<"]";
  return glm::vec3(f32Zmax-f32Zmin,f32Xmax-f32Xmin,f32Ymax-f32Ymin);
}


const static SV_POINT3F32_S InnerSV_ToPoint3f32S(const aiVector3D& st3d) {

  SV_POINT3F32_S stTmp = {static_cast<SV_F32>(st3d.x), \
      static_cast<SV_F32>(st3d.y), \
      static_cast<SV_F32>(st3d.z)};
  return stTmp;
}

const static SV_POINT2F32_S InnerSV_ToPoint2f32S(const aiVector3D& st3d) {
  SV_POINT2F32_S stTmp = {static_cast<SV_F32>(st3d.x), \
      static_cast<SV_F32>(st3d.y) };
  return stTmp;
}

const static SV_VOID InnerSV_LoadVBOEachVehicleMesh(const aiMesh * mesh,vbo::InnerSV_VehicleVboClass* pstVbo) {
  SV_S32 s32VectorSize = mesh->mNumFaces*3;
  std::vector<SV_POINT3F32_S> stVertexVect(s32VectorSize) ,stNormalVect(s32VectorSize);
  std::vector<SV_POINT2F32_S> stTextCoordVect(s32VectorSize);
  for(SV_S32 s32CurFace =0;s32CurFace<mesh->mNumFaces;++s32CurFace) {
    aiFace *pstFace = &mesh->mFaces[s32CurFace];
    //aiProcess_Triangulate is turned on, so looping through 3 vertices
    for (SV_S32 i = 0; i < 3; i++) {
      stVertexVect.push_back(InnerSV_ToPoint3f32S(mesh->mVertices[pstFace->mIndices[i]]));
      stNormalVect.push_back(InnerSV_ToPoint3f32S(mesh->mNormals[pstFace->mIndices[i]]));
      if (mesh->HasTextureCoords(0)) {
        stTextCoordVect.push_back(InnerSV_ToPoint2f32S(mesh->mTextureCoords[0][pstFace->mIndices[i]]));
      }
    }
  }
  pstVbo->Init(stVertexVect,stNormalVect,stTextCoordVect,mesh->mMaterialIndex);
  return;
}

}//end of _local
}//end of vehicleloder
}//end of vehicle
}//end of svrender
}//end of sv_avm
}//end of sm
