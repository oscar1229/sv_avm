/*
 * vehicle.cpp
 */
#include "vehiclerender.hpp"

//OpenGL
#define GL_GLEXT_PROTOTYPES 1
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

//Camera movement
#include "src/svrender/common/glm/gtc/matrix_transform.hpp"
#include "src/svrender/common/glm/gtc/type_ptr.hpp"

#include "vehicleloder/vehicleloder.hpp"
#include "src/svrender/common/shader/shader.hpp"
#include "src/svrender/common/viewtransoform/viewtransform.hpp"
namespace sm {
namespace sv_avm {
namespace svrender {
namespace vehicle {

SV_BOOL InnerSV_VehicleRenderClass::Init(const std::string& s8DaeFile, const SV_SIZE_S& stVehicleSize, const SV_F32& f32Transluce) {
if (SV_FALSE == VehicleInit(s8DaeFile, stVehicleSize)) {
    return SV_FALSE;
}
if (NULL == this->gpclMvClass) {
    LOG(WARNING) << "Passed Null gpclMvClass";
    return SV_FALSE;
}
glm::vec3 stVehicleScale = pclVehicleLoder->GetVehicleScal();
gpclMvClass->SetVehicleScale(stVehicleScale);
f32Translucency = f32Transluce < 0.5?0.5:f32Transluce;
f32Translucency = f32Transluce > 1.0?1.0:f32Transluce;
return ProgramInit();
}

SV_VOID InnerSV_VehicleRenderClass::Render(const SV_S32& s32ViewMode, const SV_RECT_S& stViewPoint ) {
 // glm::vec3 stVehicleScale =pclVehicleLoder->GetVehicleScal();
glm::mat4 mv = this->gpclMvClass->GetMv();
glm::mat4 mn = this->gpclMvClass->GetMn();
glm::mat4 mvp = this->gpclMvClass->GetVehicleMvp(s32ViewMode);
glm::vec3 eyelight = this->gpclMvClass->GetVehicleEyeLight();
glViewport(stViewPoint.stStartPoint.s32X, stViewPoint.stStartPoint.s32Y, stViewPoint.stRectSize.s32Width, stViewPoint.stRectSize.s32Height);//视口1
glUseProgram(this->clProgram.GetHandle());
glEnable(GL_DEPTH_TEST);
  //更新车模着色器GLSL Uniform
glUniformMatrix4fv(stGLSLUniform.u32MvpUniform, 1, GL_FALSE, glm::value_ptr(mvp));
glUniformMatrix4fv(stGLSLUniform.u32MvUniform, 1, GL_FALSE, glm::value_ptr(mv));
glUniformMatrix3fv(stGLSLUniform.u32MnUniform, 1, GL_FALSE, glm::value_ptr(mn));
glUniform3f(stGLSLUniform.u32EyeLightUniform, eyelight.x, eyelight.y, eyelight.z);
glUniform1f(stGLSLUniform.u32translucencelOC, f32Translucency);
pclVehicleLoder->Draw(clProgram.GetHandle(), stGLSLUniform.u32AmbientLoc, stGLSLUniform.u32DiffuseLoc);
glUseProgram(0);
}

SV_BOOL InnerSV_VehicleRenderClass::ProgramInit(SV_VOID) {
std::string s8_v_shader_str = glshader::s_v_shader_model;
std::string s8_f_shader_str = glshader::s_f_shader_model;
 // return clProgram.LoadShaders(s8_v_shader_str,s8_f_shader_str);
if (SV_FALSE == clProgram.LoadShaders(s8_v_shader_str, s8_f_shader_str)) {
    LOG(ERROR) << "clProgram.LoadShaders failed";
    return SV_FALSE;
}
stGLSLUniform.u32MvpUniform = glGetUniformLocation(clProgram.GetHandle(), "mvp");
stGLSLUniform.u32MvUniform = glGetUniformLocation(clProgram.GetHandle(), "mv");
stGLSLUniform.u32MnUniform = glGetUniformLocation(clProgram.GetHandle(), "mn");
stGLSLUniform.u32EyeLightUniform =  glGetUniformLocation(clProgram.GetHandle(), "eyelight");
stGLSLUniform.u32AmbientLoc = glGetUniformLocation(clProgram.GetHandle(), "ambient");
stGLSLUniform.u32DiffuseLoc = glGetUniformLocation(clProgram.GetHandle(), "diffuse");
stGLSLUniform.u32translucencelOC = glGetUniformLocation(clProgram.GetHandle(), "translucence");
return SV_TRUE;
}


SV_BOOL InnerSV_VehicleRenderClass::VehicleInit(const std::string& s8VehicleFilename, const SV_SIZE_S& vehicleSize) {
pclVehicleLoder = new(std::nothrow) vehicleloder::InnerSV_VehicleLoader;
if (NULL == pclVehicleLoder) {
    LOG(WARNING) << "new InnerSV_VehicleLoader failed";
    return SV_FALSE;
};
if (SV_FALSE == pclVehicleLoder->Initialize(s8VehicleFilename.c_str(), vehicleSize)) {
    LOG(WARNING) << "pclVehicleLoder->Initialize Failed";
    delete(pclVehicleLoder);
    return SV_FALSE;
}

return SV_TRUE;
}


}  // namespace vehicle
}  // namespace svrender
}  // namespace sv_avm
}  // namespace sm

