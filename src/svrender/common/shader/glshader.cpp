/*
 * glshader.cpp
 *
 *opengl 着色器程序编译加载的相关类成员函数实现
 *
 */

#include "glshader.hpp"

#include <stdio.h>
#include <stdlib.h>
#include<string.h>

#include<string>

#include<glog/logging.h>//glog
//OpenGL
#include <GLES3/gl3.h>

namespace sm {
namespace sv_avm {
namespace svrender {
namespace glshader {
namespace _local {
//打印oenglshader编程器链接失败信息
static const SV_BOOL bIsProgramLinked(const SV_U32& u32GlProgramHandle);

}  // namespace _local

InnerSV_ProgramClass::InnerSV_ProgramClass() {
memset(&stProgram, 0, sizeof(stProgram));
}

InnerSV_ProgramClass::~InnerSV_ProgramClass() {
DestroyShaders();
}

SV_BOOL InnerSV_ProgramClass::LoadShaders(const std::string& s8V_Shader, const std::string& s8_P_shader) {
  //编程器链接是否成功标志位
SV_BOOL S32Linked = false;
stProgram.u32VertShaderNum = glCreateShader(GL_VERTEX_SHADER);
stProgram.u32PixelShaderNum = glCreateShader(GL_FRAGMENT_SHADER);
  //顶点着色器变成
if (SV_FALSE == CompileShader(s8V_Shader.c_str(), stProgram.u32VertShaderNum)) {
        return SV_FALSE;
}
  //片段着色器变成CompileShader
if (SV_FALSE == CompileShader(s8_P_shader.c_str(), stProgram.u32PixelShaderNum)) {
    glDeleteShader(stProgram.u32VertShaderNum);
    return SV_FALSE;
}
if ((stProgram.u32ProgramHandle = glCreateProgram()) == 0) {
    glDeleteShader(stProgram.u32VertShaderNum);
    glDeleteShader(stProgram.u32PixelShaderNum);
    return SV_FALSE;
}
glAttachShader(stProgram.u32ProgramHandle, stProgram.u32VertShaderNum);
glAttachShader(stProgram.u32ProgramHandle, stProgram.u32PixelShaderNum);
glLinkProgram(stProgram.u32ProgramHandle);
return _local::bIsProgramLinked(stProgram.u32ProgramHandle);
}

SV_VOID InnerSV_ProgramClass::DestroyShaders(SV_VOID) {
if (stProgram.u32ProgramHandle) {
        glDeleteShader(stProgram.u32VertShaderNum);
        glDeleteShader(stProgram.u32PixelShaderNum);
        glDeleteProgram(stProgram.u32ProgramHandle);
        stProgram.u32ProgramHandle = 0;
}
}

SV_BOOL InnerSV_ProgramClass::CompileShader(const SV_S8* ps8ShaderStr , const SV_U32& s32Num) {
glShaderSource(s32Num, 1, (const char**)&ps8ShaderStr, NULL);
glCompileShader(s32Num);
SV_S32 s32Compiled = 0;
glGetShaderiv(s32Num, GL_COMPILE_STATUS, &s32Compiled);
if (!s32Compiled) {
    // Retrieve error buffer size.
    GLint errorBufSize, errorLength;
    glGetShaderiv(s32Num, GL_INFO_LOG_LENGTH, &errorBufSize);
    char * infoLog = (char*)malloc(errorBufSize * sizeof(char) + 1);
    if (infoLog) {
      // Retrieve error.
        glGetShaderInfoLog(s32Num, errorBufSize, &errorLength, infoLog);
        LOG(ERROR) << infoLog;
        if (infoLog) free(infoLog);
    }
    return SV_FALSE;
}
return SV_TRUE;
}

namespace _local {
//打印oenglshader编程器链接失败信息
static const SV_BOOL bIsProgramLinked(const SV_U32& u32GlProgramHandle) {
SV_S32 s32Linked;
glGetProgramiv(u32GlProgramHandle, GL_LINK_STATUS, &s32Linked);
if (!s32Linked) {
    // Retrieve error buffer size.
    GLint errorBufSize, errorLength;
    glGetShaderiv(u32GlProgramHandle, GL_INFO_LOG_LENGTH, &errorBufSize);
    if (errorBufSize) {
        char* infoLog = (char*)malloc(errorBufSize * sizeof(char) + 1);
        if (infoLog != NULL) {
        // Retrieve error.
        glGetProgramInfoLog(u32GlProgramHandle, errorBufSize, &errorLength, infoLog);
        LOG(ERROR) << infoLog;
        if (infoLog) free(infoLog);
        }
    }
    return false;
}
return SV_TRUE;
}
}  // namespace _local
}  // namespace glshader
}  // namespace svrender
}  // namespace sv_avm
}  // namespace sm


