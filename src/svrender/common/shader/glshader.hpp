/*
 * glshader.hpp
 *
 * 此文件声明了有关opengl 着色器程序编译加载的相关类
 *
 */

#ifndef SV_SVM_SVRENDER_GLSHADER_GLSHADER_HPP_
#define SV_SVM_SVRENDER_GLSHADER_GLSHADER_HPP_
#pragma once

#include<string>

#include"common/svtype.hpp"//POD数据自定义及SV公共数据结构

namespace sm {
namespace sv_avm {
namespace svrender {
namespace glshader {
//@brief 有关opengl 着色器程序编译加载的相关类
//@remarks 此类不可复制及移动
//          此类需在display已创建的情况下调用，否则会加载着色器失败
//          此类的使用示例如下：
//          InnerSV_ProgramClass clGlProgram;
//          if(SV_TRUE == clGlProgram.LoadShaders(s8V_Shader,s8_P_shader)）{
//          ...
//          clGlProgram.GetHandle();
//          ...
//          }
class InnerSV_ProgramClass {
public:
  explicit InnerSV_ProgramClass();
  ~InnerSV_ProgramClass();

   //@brief           Load shaders
   //
   // @param           const char* v_shader - vertex shader name
   //                  const char* p_shader - pixel shader name
   //
   // @return          The function returns 0 if shaders were loaded successfully. Otherwise -1 has been returned.
   //
   // @remarks         The function loads and compiles vertex/pixel shaders and links the program object
   //                  specified by program.
   //该函数中创建的相关对象，在析构函数中释放
   //
  SV_BOOL LoadShaders(const std::string& s8V_Shader, const std::string& s8_P_shader);
  //@brief 清除加载的着色器程序
  //@remarks  The function frees the memory and invalidates the name associated with the shader object
  //       specified by shader.
  SV_VOID DestroyShaders(SV_VOID);
  //@brief 返回着色器程序句柄
  inline SV_U32 GetHandle() {return stProgram.u32ProgramHandle;};
private:
  struct programInfo
  {
      SV_U32 u32VertShaderNum;   // Vertex shader id
      SV_U32 u32PixelShaderNum;  // Pixel shader id
      SV_U32 u32ProgramHandle;   // Program id
  };
  //显式申明移动构造函数和赋值运算符，禁用当前类的复制，只声明，不做定义
  InnerSV_ProgramClass(const InnerSV_ProgramClass&);
  InnerSV_ProgramClass& operator = (const InnerSV_ProgramClass& m);

   // @brief           Compile a vertex or pixel shader
   // @param          const std::string& s8ShaderStr- vertix or pixel shader name
   //                 const SV_U32& s32Num- vertix or pixel shader number
   // @return          The function returns 0 if shaders were compilled successfully. Otherwise -1 has been returned.
   // @remarks         The function compiles vertex or pixel shader.
  SV_BOOL CompileShader(const SV_S8* ps8ShaderStr , const SV_U32& s32Num);

  programInfo stProgram;    // GL program
};

}//end of glshader
}//end of svrender
}//end of sv_avm
}//end of sm




#endif /* SV_SVM_SVRENDER_GLSHADER_GLSHADER_HPP_ */
