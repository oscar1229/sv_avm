/*
 * vbo.cpp
 *
 */

#include "vbo.hpp"

#include<string.h>

#include <GLES3/gl3.h>

namespace sm {
namespace sv_avm {
namespace svrender {
namespace vehicle {
namespace vehicleloder {
namespace vbo{
InnerSV_VehicleVboClass::InnerSV_VehicleVboClass():bInitialized(SV_FALSE){
this->u32Vao = 0;
this->u32Id = 0;
this->u32Count = 0;
memset(au32Buffer, 0, sizeof(au32Buffer));
}

InnerSV_VehicleVboClass::~InnerSV_VehicleVboClass() {
;
}

const SV_VOID InnerSV_VehicleVboClass::Init(const std::vector<SV_POINT3F32_S>& stVertexVect,
    const std::vector<SV_POINT3F32_S>& stNormalVect, const std::vector<SV_POINT2F32_S>& stTexCoordVect,
    const SV_U32 &s32Id) {
CHECK(stVertexVect.size() == stNormalVect.size()) << "stVertexVect.size() != stNormalVect.size()";//判断stVertexVect与stNormalVect Size是否一致
this->u32Id = s32Id;
this->u32Count = stVertexVect.size();
  //Vertex array
glGenVertexArrays(1, &u32Vao);
glBindVertexArray(u32Vao);
  //Allocate and assign three VBO to our handle (vertices, normals and texture coordinates)
glGenBuffers(4, au32Buffer);
  //store vertices into buffer
glBindBuffer(GL_ARRAY_BUFFER, au32Buffer[SV_ENUM_VBO_P_VERTEX]);
glBufferData(GL_ARRAY_BUFFER, sizeof(stVertexVect[0]) * stVertexVect.size(), &stVertexVect[0], GL_STATIC_DRAW);
  // vertices are on index 0 and contains three floats per vertex
glVertexAttribPointer(GLuint(0), 3, GL_FLOAT, GL_FALSE, 0, 0);
glEnableVertexAttribArray(0);
  //store normals into buffer
glBindBuffer(GL_ARRAY_BUFFER, au32Buffer[SV_ENUM_VBO_P_NORMAL]);
glBufferData(GL_ARRAY_BUFFER, sizeof(stNormalVect[0]) * stNormalVect.size(), &stNormalVect[0], GL_STATIC_DRAW);
  // normals are on index 1 and contains three floats per vertex
glVertexAttribPointer(GLuint(1), 3, GL_FLOAT, GL_FALSE, 0, 0);
glEnableVertexAttribArray(1);
  //store texture coordinates
glBindBuffer(GL_ARRAY_BUFFER, au32Buffer[SV_ENUM_VBO_P_TEXCOORD]);
glBufferData(GL_ARRAY_BUFFER, sizeof(stTexCoordVect[0]) * stTexCoordVect.size(), &stTexCoordVect[0], GL_STATIC_DRAW);
  //coordinates are on index 2 and contains two floats per vertex
glVertexAttribPointer(GLuint(2), 2, GL_FLOAT, GL_FALSE, 0, 0);
glEnableVertexAttribArray(2);

glBindVertexArray(0);
bInitialized = SV_TRUE;
}//end of Init
}  // namespace vbo
}  // namespace vehicleloder
}  // namespace vehicle
}  // namespace svrender
}  // namespace sv_avm
}  // namespace sm


