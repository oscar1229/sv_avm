/*
 * display.hpp
 *
 */

#ifndef SV_SVM_SVRENDER_DISPLAY_DISPLAY_HPP_
#define SV_SVM_SVRENDER_DISPLAY_DISPLAY_HPP_

#pragma once

#include<string>

#include "common/svtype.hpp"//POD数据自定义及SV公共数据结构

namespace sm {
namespace sv_avm {
namespace svrender {
namespace display {

enum {
  //键盘事件
  InnerSV_ENUM_KEY_LEFT_E=0x10,
  InnerSV_ENUM_KEY_RIGHT_E,
  InnerSV_ENUM_KEY_UP_E,
  InnerSV_ENUM_KEY_DOWN_E,
  InnerSV_ENUM_KEY_QUARD_E,
  InnerSV_ENUM_KEY_SCAN_E,
  //鼠标事件
  InnerSV_ENUM_M_SCROLL_UP_E =0x20,
  InnerSV_ENUM_M_SCROLL_DOWN_E,
  InnerSV_ENUM_M_MOVE_E,
  InnerSV_ENUM_KEY_NONE_E=0xFF,
};
//@remarks 使用示例如下：
//#ifdef EGL_USE_X11
//        InnerSV_CreateDisplay(NULL,NULL);
//#else
//        InnerSV_CreateDisplay(0,NULL,NULL);
//#endif
//         while(true) {
//           InnerSV_DisplayClear();
//           ...
//           InnerSV_DisplaySwap();
//         }
//         InnerSV_DeleteDisplay();

//@brief 开启基于X11的EGL窗口显示
//@param in s8KeyBoardDev 键盘设备全路径名
//       in s8MouseDev 鼠标事件全路径名
//@remarks 如果Display已创建，则调用此函数会直接返回，切记
//         InnerSV_CreateDisplay与InnerSV_DeleteDisplay必须成对出现
#ifdef EGL_USE_X11
SV_VOID InnerSV_CreateDisplay(const char* s8KeyBoardDev,const char* s8MouseDev);
#else
//@brief 开启基于FB的EGL窗口显示
//@param in s32FbDevIdx Fb设备好0-3
//       in s8KeyBoardDev 键盘设备
//       in s8MouseDev鼠标设备
//@remarks 如果Display已创建，则调用此函数会直接返回，切记
//         InnerSV_CreateDisplay与InnerSV_DeleteDisplay必须成对出现
SV_VOID InnerSV_CreateDisplay(const SV_S32& s32FbDevIdx,const char* s8KeyBoardDev,const char* s8MouseDev);
#endif
//@brief 清空当前的绘图窗口
//@remarks 此函数需在每次EGL窗口绘制前调用
SV_VOID InnerSV_DisplayClear(SV_VOID);
//@brief 刷新EGL窗口显示
//@remarks 此函数需在每次EGL窗口绘制完成后调用
SV_VOID InnerSV_DisplaySwap(SV_VOID);
//@brief 关闭基于X11的EGL窗口显示
SV_VOID InnerSV_DeleteDisplay(const SV_S32& s32FbDevIdx);
//@brief 获取当前显示器显示分辨率
//@return 当前显示器的显示分辨率尺寸
SV_SIZE_S InnerSV_GetDisplayFrameSize(SV_VOID);

//@brief 获取当前EGLDisplay句柄(void*形式,避免在公共头引入EGL头)
//@return 已创建的EGLDisplay;若Display未创建返回NULL
//@remarks 供dma_buf零拷贝路径创建EGLImage使用
SV_VOID* InnerSV_GetEglDisplay(SV_VOID);

//@brief 检测是否存在输入事件
//@return 1 存在输入事件 0 不存在
SV_S32 InnerSV_DisplayGetEventNum(SV_VOID);
SV_S32 InnerSV_DisplayNextEvent(SV_VOID);

//@brief 配置无显示器时的离屏渲染参数
//@param in s8OutputPath 渲染结果保存路径;NULL或空串表示禁用离屏渲染
//       in s32Width  离屏渲染宽度
//       in s32Height 离屏渲染高度
//@remarks 必须在InnerSV_CreateDisplay之前调用才生效。
//         仅当显示器不可用时启用;显示器可用时该配置被忽略。
SV_VOID InnerSV_SetOffscreenConfig(const char* s8OutputPath,const SV_S32& s32Width,const SV_S32& s32Height);

//@brief 强制使用离屏渲染,即使显示器可用
//@param in bForce SV_TRUE 强制离屏 SV_FALSE 自动检测(默认)
//@remarks 必须在InnerSV_CreateDisplay之前调用;
//         需同时通过InnerSV_SetOffscreenConfig配置输出路径
SV_VOID InnerSV_SetForceOffscreen(const SV_BOOL& bForce);

//@brief 查询当前是否处于离屏渲染模式
//@return SV_TRUE 离屏渲染 SV_FALSE 正常窗口显示
SV_BOOL InnerSV_bIsOffscreenMode(SV_VOID);

//@brief 将当前渲染结果保存为图片
//@return SV_TRUE 保存成功 SV_FALSE 保存失败或未处于离屏模式
//@remarks 仅离屏模式下有效;需在InnerSV_DeleteDisplay之前调用
SV_BOOL InnerSV_bSaveOffscreenFrame(SV_VOID);

}//end of display
}//end of svrender
}//end of sv_avm
}//end of sm


#endif /* SV_SVM_SVRENDER_DISPLAY_DISPLAY_HPP_ */
