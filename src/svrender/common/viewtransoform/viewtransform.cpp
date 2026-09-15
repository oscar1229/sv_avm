/*
 * viewtransform.cpp
 *
 */

#include "viewtransform.hpp"

#include <stdio.h>
#include <math.h>

#include<vector>

#include "src/svrender/common/glm/gtc/matrix_transform.hpp"
#include "src/svrender/common/glm/gtc/type_ptr.hpp"

namespace sm {
namespace sv_avm {
namespace svrender {
namespace mvp{

InnerSV_MvCalss::InnerSV_MvCalss(SV_VOID):stViewPointCameraPos(glm::vec3(0.0, 0.1, -3.45)), \
    stViewCameraRotate(glm::vec2(-1.57, -0.75)), stVehicleScale(glm::vec3(1.0, 1.0, 1.0)), bInitialized(SV_FALSE){};
InnerSV_MvCalss::~InnerSV_MvCalss(SV_VOID) {
;
}
SV_VOID InnerSV_MvCalss::Initialized(SV_VOID) {
CalcAllPrivateDatas();
bInitialized = SV_TRUE;
}

SV_VOID InnerSV_MvCalss::SetVirtualCameraParams(const glm::vec3& stPos, const glm::vec2& stRot) {
DLOG(INFO) << __FUNCTION__;
  //prevent the camera to flip the model upside down and look under the model
glm::vec2 stRotTmp(stRot[0], glm::clamp(stRot[1], gf32CamLimitRyMin, gf32CamLimitRyMax));
  // // prevent the camera from zooming too close
glm::vec3 stPosTmp(stPos[0], stPos[1], glm::clamp(stPos[2], gf3CamLimitZoomMin, gf32CamLimitZoomMax));
if (IsViewPointParamsEqual(stPosTmp, stRotTmp)) {//判断参数是否有变化
    return;
}
stViewPointCameraPos = stPosTmp;
stViewCameraRotate = stRotTmp;
CalcAllPrivateDatas();
}

SV_VOID InnerSV_MvCalss::SetVehicleScale(const glm::vec3& stVehicleScale) {
this->stVehicleScale = stVehicleScale;
stVehicleMvp3D = CalcVehicleMvpMatrix(stVehicleScale, SV_ENUM_VIEWMODE_3D);
stVehicleMvpBird = CalcVehicleMvpMatrix(stVehicleScale, SV_ENUM_VIEWMODE_2D);
}
glm::mat4 InnerSV_MvCalss::GetCameraMvpMatrix(const SV_S32& s32ViewMode) {
DLOG(INFO) << __FUNCTION__;
CHECK(bInitialized);
return (SV_ENUM_VIEWMODE_3D == s32ViewMode?stCameraMvp3D:stCameraMvpBird );
}
glm::mat4 InnerSV_MvCalss::CalcVehicleMvpMatrix(const glm::vec3& stVehicleScale, const SV_S32& s32ViewMode) {
DLOG(INFO) << __FUNCTION__;
glm::mat4 stCameraMatrix = CalcVehicleCameraMatrix(stVehicleScale);
return (SV_ENUM_VIEWMODE_3D == s32ViewMode?stCameraMvp3D:stCameraMvpBird )* stCameraMatrix;
}

glm::mat4 InnerSV_MvCalss::CalcMvMatrix(SV_VOID) {
DLOG(INFO) << __FUNCTION__;
return glm::rotate(glm::rotate(glm::translate(glm::mat4(1.0f), stViewPointCameraPos), stViewCameraRotate[1], glm::vec3(1, 0, 0)), \
        stViewCameraRotate[0], glm::vec3(0, 0, 1));
}

glm::mat4 InnerSV_MvCalss::CalcMnMatrix(SV_VOID) {
DLOG(INFO) << __FUNCTION__;
return glm::mat3(glm::rotate(glm::rotate(glm::rotate(glm::rotate(glm::mat4(1.0f), stViewCameraRotate[1], glm::vec3(1, 0, 0)), stViewCameraRotate[0], glm::vec3(0, 0, 1)),
                glm::radians(gf32CarOrigitationX), glm::vec3(1, 0, 0)), glm::radians(gf32CarOrigitationY), glm::vec3(0, 1, 0)));
}

SV_BOOL InnerSV_MvCalss::IsViewPointParamsEqual(const glm::vec3& stPos, const glm::vec2& stRot) {
DLOG(INFO) << __FUNCTION__;
return (stViewPointCameraPos == stPos && stViewCameraRotate == stRot);
}

SV_VOID InnerSV_MvCalss::CalcAllPrivateDatas(SV_VOID) {
DLOG(INFO) << __FUNCTION__;
stMv = CalcMvMatrix();
stMn = CalcMnMatrix();
stCameraMvp3D = gstProjection3D*stMv;
stCameraMvpBird = gstProjectionBird*gMvBird;
stVehicleMvp3D = CalcVehicleMvpMatrix(stVehicleScale, SV_ENUM_VIEWMODE_3D);
stVehicleMvpBird = CalcVehicleMvpMatrix(stVehicleScale, SV_ENUM_VIEWMODE_2D);
glm::vec4 stTmp = stMv*glm::vec4(0, 0.1, stViewPointCameraPos.z, 1.0);
stVehicleEyeLight = glm::vec3(-stTmp.y, stTmp.x, -stTmp.z);
}

glm::mat4 InnerSV_MvCalss::CalcVehicleCameraMatrix(const glm::vec3& stVehicleScale) {
DLOG(INFO) << __FUNCTION__;
return glm::rotate(glm::rotate(glm::scale(glm::mat4(1.0f), stVehicleScale), glm::radians(gf32CarOrigitationX), glm::vec3(1, 0, 0)),
        glm::radians(gf32CarOrigitationY), glm::vec3(0, 1, 0));
}

}  // namespace mvp
}  // namespace svrender
}  // namespace sv_avm
}  // namespace sm
