/*
 * shader.hpp
 *
 *SVM 3D Render相关顶点着色器及片段做色器GLSLd文件
 *
 */

#ifndef SV_SVM_SVRENDER_GLSHADER_SHADER_HPP_
#define SV_SVM_SVRENDER_GLSHADER_SHADER_HPP_
#pragma once
namespace sm {
namespace sv_avm {
namespace svrender {
namespace glshader {
static const char s_v_shader_line[] =
    " #version 300 es \n "
    " layout(location = 0) in vec4 vPosition; \n "
    " void main() \n "
    " { \n "
        " gl_Position = vPosition; \n "
        " gl_PointSize = 3.0f;\n"
    " } \n ";

static const char s_f_shader_line[] =
    "#version 300 es \n"
    "precision mediump float;"
    "layout(location = 0) out vec4 fragColor; \n "
    "void main() {"
    "  fragColor = vec4(1.0, 1.0, 0.0, 1.0);"
    "}";
// Vertices shader without view and projection parameters for exposure correction
static const char s_v_shader[] =
    " #version 300 es \n "
    " layout(location = 0) in vec4 vPosition; \n "
    " layout(location = 1) in vec2 vTexCoord; \n "
    " out vec2 TexCoord; \n "
    " void main() \n "
    " { \n "
        " gl_Position = vPosition; \n "
        " TexCoord = vTexCoord; \n "
    " } \n ";
// Fragment shader without blending and exposure correction
// Exposure correction
static const char s_f_shader[] =
    "#version 300 es \n"
    " precision mediump float;\n "
    " in vec2 TexCoord; \n "
    " out vec4 fragColor; \n "
    " uniform sampler2D myTexture; \n "
    " void main() \n "
    " {\n "
        " fragColor = texture(myTexture, TexCoord); \n "
    " }\n ";



// Vertices shader without view and projection parameters for calibrateion result
static const char s_v_shader_bowl[] =
    " #version 300 es \n "
    " layout(location = 0) in vec4 vPosition; \n "
    " layout(location = 1) in vec3 vTexCoord; \n "
    " out vec3 TexCoord; \n "
    " void main() \n "
    " { \n "
        " gl_Position = vec4(vPosition.xyz, 5); \n "
        " TexCoord = vTexCoord; \n "
    " } \n ";


static const char s_f_shader_bowl[] =
    "#version 300 es \n"
    " precision mediump float;\n "
    " in vec3 TexCoord; \n "
    " out vec4 fragColor; \n "
    " uniform sampler2D myTexture; \n "
    " void main() \n "
    " {\n "
        " vec2 coord =TexCoord.xy; \n"
        " vec4 gain =vec4(1,1,1,TexCoord.z); \n"
        " fragColor = texture(myTexture, coord)*gain; \n "

    " }\n ";


/******************************* Vertices shaders ************************************/
// Vertices shader with view and projection parameters
static const char s_v_shader_glm[] =
    " #version 300 es \n "
    " layout(location = 0) in vec4 vPosition; \n "
    " layout(location = 1) in vec3 vTexCoord; \n "
    " out vec3 TexCoord; \n "
    " uniform mat4 mvp; \n"
    " void main() \n "
    " { \n "
        " gl_Position = mvp * vec4(vPosition.xyz, 1); \n "
        " TexCoord = vTexCoord; \n "
    " } \n ";


// Fragment shader without blending and with exposure correction
// To render non-overlap regions
static const char s_f_shader_ec[] =
    "#version 300 es \n"
    " precision mediump float;\n "
    " in vec3 TexCoord; \n "
    " out vec4 fragColor; \n "
    " uniform sampler2D myTexture; \n "
    " void main() \n "
    " {\n "
        " vec2 coord =TexCoord.xy; \n"
        " fragColor =vec4(texture(myTexture, coord).rgb,TexCoord.z); \n "
    " }\n ";

// Fragment shader for zero-copy NV12 path:采样GL_TEXTURE_EXTERNAL_OES外部纹理,
// 由驱动自动完成YUV(NV12)->RGB转换;结构与s_f_shader_ec一致(TexCoord.z作alpha增益)
static const char s_f_shader_ec_oes[] =
    "#version 300 es \n"
    "#extension GL_OES_EGL_image_external_essl3 : require \n"
    " precision mediump float;\n "
    " in vec3 TexCoord; \n "
    " out vec4 fragColor; \n "
    " uniform samplerExternalOES myTexture; \n "
    " void main() \n "
    " {\n "
        " vec2 coord =TexCoord.xy; \n"
        " fragColor =vec4(texture(myTexture, coord).rgb,TexCoord.z); \n "
    " }\n ";


static const char s_v_shader_model[] =
    "#version 300 es \n"
    " \n"
    "layout(location = 0) in vec3 position; \n"
    "layout(location = 1) in vec3 normal; \n"
    " \n"
    "uniform mat4 mvp, mv; \n"
    "uniform mat3 mn; \n"
    " \n"
    "//material \n"
    " \n"
    "out vec3 eyePosition, eyeNormal; \n"
    " \n"
    "void main() \n"
    "{ \n"
    "    gl_Position = mvp*vec4(position,1); \n"
    " \n"
    "    eyePosition = (mv*vec4(position,1)).xyz; \n"
    "    eyeNormal = normalize(mn*normal); \n"
    "} \n";
static const char s_f_shader_model[] =
        "#version 300 es \n"
        " precision mediump float;\n "
        " \n"
        "uniform vec3 ambient; \n"
        "uniform vec3 diffuse; \n"
        "uniform vec3 eyelight; \n"
        "uniform float translucence; \n"
        " \n"
        "in vec3 eyePosition, eyeNormal; \n"
        " \n"
        "out vec4 fragColor; \n"
        " \n"
        "void main() \n"
        "{ \n"
        "    vec3 N = normalize(eyeNormal); \n"
        "    vec3 L = normalize(eyelight-eyePosition); \n"
        "    vec3 finalColor = vec3(0.0); \n"
        " \n"
        "    //Blin-Phong model \n"
        "    finalColor = ambient; \n"
        "    float lambertTerm = dot(L, N); \n"
        "    if(lambertTerm >= 0.0) \n"
        "    { \n"
        "        finalColor +=  diffuse * lambertTerm; \n"
        "    } \n"
        " \n"
        "    fragColor = vec4(finalColor,translucence); \n"
        "} \n";

}//end of glshader
}//end of svrender
}//end of sv_avm
}//end of sm


#endif /* SV_SVM_SVRENDER_GLSHADER_SHADER_HPP_ */
