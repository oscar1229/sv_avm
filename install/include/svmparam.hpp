/*************************************************************************************************************************
 * svmparam.hpp
 *
 *
 *有关环视标定参数读取保存到XML文件的类的定义
 *
 **************************************************************************************************************************/

#ifndef SV_SVAVM_SVMPARAM_HPP
#define SV_SVAVM_SVMPARAM_HPP
#pragma once

#include<stdio.h>
#include<stdlib.h>
#include<string>

#include<glog/logging.h>

#include "common/svtype.hpp"//POD数据自定义及SV公共数据结构
#include "include/sv_avmcommon.hpp"
namespace sm {
namespace sv_avm {
namespace svmparam {
//子模块函数返回代码
enum ISV_ENUM_AVM_ERR_E{
ISV_ENUM_SUCCEED =0,
ISV_ENUM_FAILURED =-1,
ISV_ENUM_AVM_ERR_OPENED = 0x1010, //重复创建对象
ISV_ENUM_AVM_MALLOC_FAILED,//内存申请失败

ISV_ENUM_AVM_XML_NOREADPERMISIION = 0x2010,//读方式打开XML文件失败或无读权限
ISV_ENUM_AVM_XML_NOWRITEPERMISIION,//写方式打开文件失败或无写权限
ISV_ENUM_AVM_XML_FILENODELOST,//文件节点丢失或不存在

};

//@brief 环视标定结果保存到XML文件或读取到XML文件的类
//@remarks 该类不可拷贝及复制
//         该类禁止在调用InnerSV_ReadFromXml或InnerSV_Init之前的读取数据成员的操作
//         如果Class的调用是在标定功能中调用，则在调用InnerSV_WriteToXml前，必须先调用InnerSV_SetCalibratePartenChannl；以便保存完整的标定输出与输入，切记
//         该类的使用实例如下：
//         char* s8XmlFileName="./res/_aSvmParam.xml";
//         InnerSV_SvmParamClass clSvmParam;
//         if (clSvmParam.InnerSV_s32ReadFromXml(s8XmlFileName)!=SV_ENUM_SUCCEED) {
//             clSvmParam.InnerSV_Init
//         }
//         SV_CAMERA_PARAMS_S stCameraLeftParams = InnerSV_stGetCameraParamsEachChannl(0);//读取或设置参数如果需要
//         ...
//         InnerSV_s32WriteToXml(_aSvmParam.xml);//保存变更过后的参数，到XML文件，通常只在标定后或加载标定后调用
class InnerSV_SvmParamClass{
  public:
    explicit InnerSV_SvmParamClass();
	~InnerSV_SvmParamClass();
	//@brief 从XML文件中读取整个InnerSV_SvmParamClass对象
	//@param in s8XmlFileFulPathName xml文件全路径名
	//       in bNeedCaliPaternFlag  是否需要读取标定模板参数的Flag
	//@return  成功：SV_ENUM_SUCCEED 失败
	//@remarks  bNeedCaliPaternFlag为SV_FALSE时只读取各通道相机参数及车型参数，不读取标定模板
	//          如果使能标定模板读取，则在读取标定模板的同时，会在函数内部动态申请一块内存空间，该内存空间，在对象析构时自动释放
	//          可能的返回值为：SV_ENUM_AVM_XML_NOREADPERMISIION、SV_ENUM_AVM_XML_FILENODELOST、
	//          SV_ENUM_AVM_MALLOC_FAILED
	SV_S32 InnerSV_s32ReadFromXml(const SV_S8* ps8XmlFileFulPathName,SV_BOOL& bNeedCaliPaternFlag);
	//@brief 写入SvmParamClass到XML文件
	//@param in s8XmlFileFulPathName xml文件全路径名
	//@return 成功：SV_ENUM_SUCCEED 失败 返回SV_ENUM_AVM_XML_NOWRITEPERMISIION
	SV_S32 InnerSV_s32WriteToXml(const SV_S8* ps8XmlFileFulPathName);
	//@brief 生成初始化环视标定参数
	//@param 无
	//@return 无
	//@remarks 通常，当读取失败时，用于初始化除标定模板外参数
	SV_VOID InnerSV_Init();
	//@brief 获取各通道摄像头参数
	//@param in s32Ch 摄像头通道号
	//@return 摄像头参数
	//@remarks  输入参数s32Ch可选值为sm命名空间的SV_ENUM_CAMERA_LEFT，SV_ENUM_CAMERA_RIGHT,SV_ENUM_CAMERA_FRONT,SV_ENUM_CAMERA_BACK
	//          使用实例：
	//          SV_CAMERA_PARAMS_S stCamparam=InnerSV_stGetCameraParamsEachChannl(s32Ch)
	inline SV_CAMERA_PARAMS_S InnerSV_stGetCameraParamsEachChannl(const SV_S32& s32Ch) {
	  CHECK(SV_TRUE==bInitialized);//断言，防止未从文件读取或初始化时的参数获取及设置
	  CHECK(s32Ch<=SV_ENUM_CAMERA_BACK);
	  return stCameraParams[s32Ch];
	}
    //@brief 设置各通道摄像头参数
    //@param in s32Ch 摄像头通道号
	//       in stCameraParam 摄像头参数
    //@return 无
    //@remarks  输入参数s32Ch可选值为sm命名空间的SV_ENUM_CAMERA_LEFT，SV_ENUM_CAMERA_RIGHT,SV_ENUM_CAMERA_FRONT,SV_ENUM_CAMERA_BACK
	inline SV_VOID InnerSV_SetCameraParamEachChannl(const SV_S32& s32Ch,const SV_CAMERA_PARAMS_S& stCameraParam) {
	  CHECK(SV_TRUE==bInitialized);
	  CHECK(s32Ch<=SV_ENUM_CAMERA_BACK);
	  stCameraParams[s32Ch]=stCameraParam;
	}
	//@brief 读取车辆尺寸参数
	inline SV_SIZE_S InnerSV_stGetVehicleSize() {
	  CHECK(SV_TRUE==bInitialized);
	  return stVehicleSize;
	}
	//@brief 设置车辆尺寸参数
	inline SV_VOID InnerSV_SetVehicleSize(const SV_SIZE_S& stVSize) {
	  CHECK(SV_TRUE==bInitialized);
	  stVehicleSize=stVSize;
	}
	//@brief 读取当前摄像头标定方式及对应的标定模板
	//@param in s32Ch 摄像头通道号
	//       out ppstCaliPatern 指向标定模板内存空间的指针的指针
	//       out ps32Method 标定方式
	//@return  成功 SV_ENUM_SUCCEED 失败 SV_ENUM_AVM_MALLOC_FAILED
	//@remarks  输入参数s32Ch为摄像头通道号，可选值为sm命名空间的SV_ENUM_CAMERA_LEFT，SV_ENUM_CAMERA_RIGHT,SV_ENUM_CAMERA_FRONT,SV_ENUM_CAMERA_BACK
	//          输出s32CaliMethod可能值为： SV_ENUM_CALIMETHOD_UCHESSBORD，SV_ENUM_CALIMETHOD_U8POINTS SV_ENUM_CALIMETHOD_BUTT
	//          因为在读取标定模板时，需要当前摄像头的标定方式eCaliMethod值，在函数内部申请一块对应大小的内存空间，
	//          并将值指给输出参数ppCaliPatern，所以输出参数ppCaliPatern需为二级指针.
	//          此函数内部申请的内存空间，需在外部使用外后，由调用者销毁，切记,当内部申请内存空间失败时返回SV_FALSE
	//          使用示例如下：
	//          SV_VOID* pCaliPatern=NULL;
	//          SV_S32 s32CaliMethod;
	//         if(SV_SUCCEED == InnerSV_S32GetCalibrateParternChannl(1,&pPatern,&s32CaliMethod)) {
	//           if (SV_ENUM_CALIMETHOD_UCHESSBORD==eCaliMethod && NULL != pPatern) {
	//             SV_CALI_UCHESSBOARD_PATERN_S* pstChessBoardCaliPatern = reinterpret_cast< SV_CALI_UCHESSBOARD_PATERN_S*>(pPatern);
	//  ...
	//             free(pstChessBoardCaliPatern);
	//           }
	//           else if (SV_ENUM_CALIMETHOD_U8POINTS == eCaliMethod && NULL != pCaliPatern) {
	//             SV_CALI_U8P_PATERN_S* pstU8CaliPatern =reinterpret_cast< SV_CALI_U8P_PATERN_S*>(pCaliPatern);
	//             ...
	//             free(pstU8CaliPatern);
	//           }
    //         }
    SV_S32 InnerSV_s32GetCalibrateParternChannl (
	    const SV_S32& s32Ch,
	    SV_VOID** ppstCaliPatern,SV_S32* ps32Method);
    //@brief 根据输入的s32Ch通道摄像头的eMethod值，设置对象的标定模板
    //@param in s32Method 标定方式
    //       in s32Ch 摄像头通道号
    //       in pstPatern 指向标定模板内存空间的指针
    //@return 成功 SV_ENUM_SUCCEED 失败 SV_ENUM_AVM_MALLOC_FAILED
    //@remarks   输入参数s32Method可选值 SV_ENUM_CALIMETHOD_UCHESSBORD，SV_ENUM_CALIMETHOD_U8POINTS SV_ENUM_CALIMETHOD_BUTT
    //           输入参数s32Ch可选值为sm命名空间的SV_ENUM_CAMERA_LEFT，SV_ENUM_CAMERA_RIGHT,SV_ENUM_CAMERA_FRONT,SV_ENUM_CAMERA_BACK
	//           函数内部申请的内存空间，在对象析构时自动释放
	//           为了统一接口，兼容两种标定方式的标定模板的设置，因此输入变量pstCaliPatern为const SV_VOID指针类型
	SV_S32 InnerSV_s32SetCalibratePartenChannl(
	                          const SV_S32& s32Method,
	                          const SV_S32& s32Ch,
	                          const SV_VOID* pstPatern);//NotFinished
  protected:
	SV_BOOL bInitialized;
  private:
	//@brief 摄像头参数类型枚举
	enum
	{
	  SV_ENUM_CAMERA_K=0, //相机矩阵
	  SV_ENUM_CAMERA_DISTOR,//鱼眼畸变
	  SV_ENUM_CAMERA_ROTATE,//旋转向量
	  SV_ENUM_CAMERA_TRANSLATE,//平移向量
	  SV_ENUM_CAMERA_SV_BUTT
	};

	//定义个参数分量文件节点字符串常量，以保证读写XML时文件节点相同
	const SV_S8* ks8CameraKNodeStr="K";//摄像头内参文件节点
	const SV_S8* ks8CameraDistortNodeStr="Distortion";//鱼眼畸变参数
	const SV_S8* ks8CameraRotateVectNodeStr="Rotate";//旋转向量
    const SV_S8* ks8CameraTranslateVectNodeStr="Translate";//平移向量
    const SV_S8* ks8CameraImageSizeNodeStr="ImageSize";//摄像头图像尺寸
    const SV_S8* ksVehicleSizeNodeStr="VehicleSize";//车辆尺寸
    const SV_S8* ks8ChessPositionNodeStr="ChessBoardPosition";//左右通道棋盘格位置
    const SV_S8* ks8ChessBoardCenterNodeStr="ChessBoardCenter";//中间区域棋盘格规格
    const SV_S8* ks8ChessBoardEdgeNodeStr="ChessBoardEdge";//边缘区域棋盘格规格
    const SV_S8* ks8GridSizeNodeStr="GridSize";//单格棋盘格尺寸
    const SV_S8* ks8U8PaternBoardSize="U8PatternSize";//8点式标定布长度
    const SV_S8* ks8U8ImagePointsNodeStr="ImagePoints";//8点式标定像素坐标节点
    //Init操作初始化参数
    const SV_F64 kf64CameraK[9]={3.3663932877255422e+02,0.0, 6.3583064178582879e+02,
        0.0,3.1971130511644719e+02,3.6648115807921403e+02,
        0.0,0.0,1.0};
    const SV_F64 kf64CameraDistor[4]={0.0,0.0,0.0,0.0};
    const SV_F64 kf64CameraTrans[3]={ -2.0406123947224861e-01,1.3088482537645041e+00,3.7495659155705119e-01};
    const SV_F64 kf64CameraRotate[3]={2.4639143620937900e+00,5.5126125145234184e-02,-1.7706632258495471e-02};
    const SV_SIZE_S kstImageSize={1280,720};

    //显式申明移动构造函数和赋值运算符，禁用当前类的复制，只声明，不做定义
    InnerSV_SvmParamClass(const InnerSV_SvmParamClass&);
    InnerSV_SvmParamClass& operator = (const InnerSV_SvmParamClass& m);

    //@brief 获取各通道摄像头参数XML文件节点名称
    //@param in s32Ch 摄像头通道号
    //@return 当前摄像头参数XML文件节点名称
    //@remarks s32Ch可选值为sm命名空间的SV_ENUM_CAMERA_LEFT，SV_ENUM_CAMERA_RIGHT,SV_ENUM_CAMERA_FRONT,SV_ENUM_CAMERA_BACK
    //         使用方式：std::string s8NodeStr = InnerSV_s8GetNodeStrOfCameraParamEachChannl(1);
    std::string InnerSV_s8GetNodeStrOfCameraParamEachChannl(const SV_S32& s32Ch);
    //@brief 获取各通道摄像头标定方式XML文件节点
    //@param in s32Ch 摄像头通道号
    //@return 标定方式XML文件节点名称
    //@remarks  s32Ch可选值为sm命名空间的SV_ENUM_CAMERA_LEFT，SV_ENUM_CAMERA_RIGHT,SV_ENUM_CAMERA_FRONT,SV_ENUM_CAMERA_BACK
    //          使用方式：std::string s8NodeStr = InnerSV_s8GetNodeStr0fCaliMethod(1);
    std::string InnerSV_s8GetNodeStr0fCaliMethod(const SV_S32& s32Ch);
    //@brief 获取各通道摄像头标定棋盘格式标定标定模板文件节点
    //@param in s32Ch 摄像头通道号
    //@return 棋盘格式标定标定模板文件节点名称
    //@remarks 使用方式：std::string s8NodeStr = InnerSV_s8GetNodeStr0fUChessBoardPatern(1);
    std::string InnerSV_s8GetNodeStr0fUChessBoardPatern(const SV_S32& s32Ch);
    //@brief 获取各通道摄像头标定8点式标定标定模板文件节点
    //@param in s32Ch 摄像头通道号
    //@return 8点式标定标定模板文件节点名称
    //@remarks std::string s8NodeStr = InnerSV_s8GetNodeStr0fUChessBoardPatern(1);
    std::string InnerSV_s8GetNodeStr0fU8PPatern(const SV_S32& s32Ch);

    //@brief 从XML文件中读取单一通道摄像头摄像头参数的某一分量
    //@param in s32Ch 摄像头通道号
    //       in pCameraParamFileNode 指向摄像头参数的文件节点对象
    //       in s32CamParamComp 摄像头参数子项枚举
    //@return 成功SV_ENUM_SUCCEED 失败 SV_ENUM_AVM_XML_FILENODELOST
    //@remarks  pCameraParamFileNode不可为NULL
    //          s32CamParamComp 可选值为SV_ENUM_CAMERA_K、SV_ENUM_CAMERA_DISTOR、SV_ENUM_CAMERA_ROTATE、SV_ENUM_CAMERA_TRANSLATE
    SV_S32 IneerSV_s32ReadCameraParamCompFromXML(const SV_VOID* pCameraParamFileNode,const SV_S32& s32Ch,const SV_S32& s32CamParamComp);
    //@brief 从XML文件中读出单一通道摄像头参数，包括内外参
    //@param in s32Ch 摄像头通道号
    //@return 成功SV_ENUM_SUCCEED 失败 SV_ENUM_AVM_XML_FILENODELOST
    //@remarks 使用方式:SV_S32 s32Ret = InnerSV_s32ReadCameraParamFromXML();
    SV_S32 InnerSV_s32ReadCameraParamFromXML(const SV_S32& s32Ch);
    //@brief 写入通道摄像头参数XML文件中，包括内外参
    //@param in s32Ch摄像头通道号
    //@return 无
    SV_VOID InnerSV_WriteCameraParamToXML(const SV_S32 s32Ch);

    //@brief 从XML文件中读取车型参数
    //@return 成功SV_ENUM_SUCCEED 失败 SV_ENUM_AVM_XML_FILENODELOST
    //@remarks 使用方式:SV_S32 s32Ret = InnerSV_s32ReadVehicleSizeFromXML();
    SV_S32 InnerSV_s32ReadVehicleSizeFromXML();
    //@brief 将车型参数写入XML文件中
    SV_VOID InnerSV_WriteVehicleSizeToXML();
    //@brief 读取某一通道标定模式
    //@param in s32Ch 摄像头通道号
    //@return 成功SV_ENUM_SUCCEED 失败 SV_ENUM_AVM_XML_FILENODELOST
    SV_S32 InnerSV_s32ReadCaliMethodEachFromXML(const SV_S32& s32Ch,SV_S32* ps32CaliMethod);
    //@brief 读取某一通道标定模板
    //@param in s32Ch 摄像头通道号
    //@return 成功SV_ENUM_SUCCEED 失败 SV_ENUM_AVM_XML_FILENODELOST、
    //        SV_ENUM_AVM_MALLOC_FAILED
    //@remarks 此函数内部存在内存空间申请，内部申请的空间无需调用者释放
    SV_S32 InnerSV_s32ReadUChessBoardPatern(const SV_S32& s32Ch);//读取棋盘格标定模板
    SV_S32 InnerSV_s32ReadU8PointPatern(const SV_S32& s32Ch);//读取8点式标定模板
    //@brief 从XML文件中读取标定类型及标定模板
    //@param in s32Ch 摄像头通道号
    //@return 成功SV_ENUM_SUCCEED 失败 SV_ENUM_AVM_XML_FILENODELOST、
    //        SV_ENUM_AVM_MALLOC_FAILED
    SV_S32 InnerSV_s32ReadCaliPartenEachFromXML(const SV_S32& s32ch);

    //@brief 写入某一通道标定模板
    //@param in s32Ch 摄像头通道号
    SV_VOID InnerSV_WriteUChessBoardPatern(const SV_S32& s32Ch);
    SV_VOID InnerSV_WriteU8PointPatern(const SV_S32& s32Ch);
    //@brief 写入标定类型及标定模板到XML文件中
    SV_VOID InnerSV_WriteCaliPartenEachToXML(const SV_S32& s32Ch);//Not Finished

    SV_S32 s32CameraNumber; //摄像头通道数量
    SV_CAMERA_PARAMS_S stCameraParams[4];//各摄像头参数，创建对象时需初始化
    SV_SIZE_S stVehicleSize;//车辆尺寸，即车辆的长、款；创建对象时需初始化
    //创建class时，需赋初值SV_ENUM_CALIMETHOD_BUTT,允许赋值SV_ENUM_CALIMETHOD_UCHESSBORD，SV_ENUM_CALIMETHOD_U8POINTS,SV_ENUM_CALIMETHOD_BUTT
    SV_S32 s32CaliMethod[static_cast<SV_U32>(SV_ENUM_CAMERA_BUTT)];
    //四通道标定模板结构体指针，SV_VOID*型，在读取XML文件时申请存储空间，析构class时需释放。
    SV_VOID* pstCaliPatern[static_cast<SV_U32>(SV_ENUM_CAMERA_BUTT)];
    void*  pXmlReadFd;//创建对象时初始化为NULL,调用InnerSV_ReadFromXml创建，指向xml文件对象，InnerSV_ReadFromXml调用结束时销毁
    void*  pXmlWriteFd;//创建对象时初始化为NULL,调用InnerSV_WriteToXml时创建， 指向xml文件对象，InnerSV_WriteToXml调用结束时销毁
};

}//end of namespace svmparam
}//end of namespace sv_avm
}//end of namespace sm
#endif /* SV_SVAVM_SVMPARAM_HPP */
