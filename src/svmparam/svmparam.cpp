/*
 * svmparam.cpp
 */
#include"svmparam.hpp"

#include<unistd.h>
#include<stdio.h>

#include<string>

#include<opencv2/core.hpp>

namespace sm {
namespace sv_avm {
namespace svmparam {

//读取当前文件节点的单通道SV_F64类型的Mat数据到一维数组中
//输出参数ppF64MatVal为提取的矩阵值数组
//当文件节点不存在，返回SV_FALSE
//使用范例：
//vector<SV_F64> f64MatValVect;
//SV_BOOL bRet = InnerSV_bRead64FMatFromXML(cvNode,&f64MatValVect);
//if (SV_TRUE == bRet) {
// ...
//}
//
static const SV_S32 InnerSV_s32Read64FMatFromXML(const cv::FileNode& cvNode, std::vector<SV_F64>* paF64MatValVect);

inline static SV_SIZE_S stSizeToSV_SIZE_S(const cv::Size& stVal)
{
SV_SIZE_S stDst = {stVal.width, stVal.height};
return stDst;
}

inline static cv::Size stSV_SIZE_SToSize(const SV_SIZE_S& stSize)
{
return cv::Size(stSize.s32Width, stSize.s32Height);
}

inline static SV_POINT2F32_S stPoint2fToSV_POINT2F32_S(const cv::Point2f& stPoint)
{
SV_POINT2F32_S stPtem = {stPoint.x, stPoint.y};
return stPtem;
}

InnerSV_SvmParamClass:: InnerSV_SvmParamClass():bInitialized(SV_FALSE)
{
pXmlReadFd = NULL;
pXmlWriteFd = NULL;
s32CameraNumber = s32GetCameraChannelNumber();
for (SV_S32 i = 0;i < s32CameraNumber;++i)
{
    memset(&stCameraParams[i], 0, sizeof(stCameraParams[i]));
    s32CaliMethod[i] = SV_ENUM_CALIMETHOD_BUTT;
    pstCaliPatern[i] = NULL;
}
stVehicleSize.s32Width = -1;
stVehicleSize.s32Height = -1;
}

InnerSV_SvmParamClass::~InnerSV_SvmParamClass()
{
pXmlReadFd = NULL;
pXmlWriteFd = NULL;
for (SV_S32 i = 0;i < s32CameraNumber;++i)//释放读写函数内部申请的标定模板内存kongjian
{
    if (NULL != pstCaliPatern[i])
        free(pstCaliPatern[i]);
}
}
SV_S32 InnerSV_SvmParamClass::InnerSV_s32ReadFromXml(const SV_S8* ps8XmlFileFulPathName, SV_BOOL& bNeedCaliPaternFlag) {
SV_S32 s32Ret;//返回值
cv::FileStorage fs(ps8XmlFileFulPathName, cv::FileStorage::READ);//创建文件读取对象
if (!fs.isOpened()) {
    LOG(ERROR) << __FUNCTION__ << ":Read " << ps8XmlFileFulPathName << " failed";
    return ISV_ENUM_AVM_XML_NOREADPERMISIION;
}
pXmlReadFd = reinterpret_cast<SV_VOID*>(&fs);
if (ISV_ENUM_SUCCEED != (s32Ret = InnerSV_s32ReadVehicleSizeFromXML()))//读取车型参数
    goto _ERR_;
for (SV_S32 i = 0;i < s32CameraNumber;++i) {
    if (ISV_ENUM_SUCCEED != (s32Ret = InnerSV_s32ReadCameraParamFromXML(i)))
        goto _ERR_;
    if (SV_TRUE == bNeedCaliPaternFlag) {
        if (ISV_ENUM_SUCCEED != (s32Ret = InnerSV_s32ReadCaliPartenEachFromXML(i)))
        goto _ERR_;
    }
}
fs.release();
pXmlReadFd = NULL;
bInitialized = SV_TRUE;
return ISV_ENUM_SUCCEED;
_ERR_:
fs.release();
pXmlReadFd = NULL;
return s32Ret;
}

SV_S32 InnerSV_SvmParamClass::InnerSV_s32WriteToXml(const SV_S8* ps8XmlFileFulPathName)
{
cv::FileStorage fs(ps8XmlFileFulPathName, cv::FileStorage::WRITE);//创建文件写入对象
if (!fs.isOpened())
    return ISV_ENUM_AVM_XML_NOWRITEPERMISIION;
pXmlWriteFd = reinterpret_cast<SV_VOID*>(&fs);

InnerSV_WriteVehicleSizeToXML();//写入车型参数
  //写入各通道参数
for (SV_S32 i = 0;i < s32CameraNumber;++i) {
    InnerSV_WriteCameraParamToXML(i);//写入摄像头参数
    InnerSV_WriteCaliPartenEachToXML(i);
}
fs.release();
pXmlWriteFd = NULL;
return ISV_ENUM_SUCCEED;
}

SV_VOID InnerSV_SvmParamClass::InnerSV_Init()
{
SV_S32 i;
for (i = 0;i < s32CameraNumber;++i)
{
    memcpy(stCameraParams[i].af64CameraK, kf64CameraK, sizeof(stCameraParams[i].af64CameraK));
    memcpy(stCameraParams[i].af64CameraDistort, kf64CameraDistor, sizeof(stCameraParams[i].af64CameraDistort));
    memcpy(stCameraParams[i].af64CameraRotateVect, kf64CameraRotate, sizeof(stCameraParams[i].af64CameraRotateVect));
    memcpy(stCameraParams[i].af64CameraTranslateVect , kf64CameraTrans, sizeof(stCameraParams[i].af64CameraTranslateVect));
    stCameraParams[i].stImageSize = kstImageSize;
}
    stVehicleSize.s32Width = 150;
    stVehicleSize.s32Height = 297;
    bInitialized = SV_TRUE;
}

SV_S32 InnerSV_SvmParamClass::InnerSV_s32GetCalibrateParternChannl(
    const SV_S32& s32Ch,
    SV_VOID** ppstCaliPatern, SV_S32* ps32CaliMethod) {
CHECK(SV_TRUE == bInitialized);//断言，防止未从文件读取或初始化时的参数获取
CHECK(s32Ch <= SV_ENUM_CAMERA_BACK);
SV_S32 s32CaliMethodTemp = s32CaliMethod[s32Ch];
  //棋盘格标定方式
if (static_cast<SV_S32>(SV_ENUM_CALIMETHOD_UCHESSBORD) == s32CaliMethodTemp) {
    CHECK(pstCaliPatern[s32Ch] != NULL);
    SV_CALI_UCHESSBOARD_PATERN_S stPatern = *reinterpret_cast<SV_CALI_UCHESSBOARD_PATERN_S*>(pstCaliPatern[s32Ch]);
    //申请内存空间，注意，需在外部由使用者释放
    SV_CALI_UCHESSBOARD_PATERN_S* pTemp = reinterpret_cast<SV_CALI_UCHESSBOARD_PATERN_S*>(malloc(sizeof(stPatern)));
    if (NULL == pTemp) {
        LOG(ERROR) << "malloc SV_CALI_UCHESSBOARD_PATERN_S failed";
        return ISV_ENUM_AVM_MALLOC_FAILED;
    }
    *pTemp = stPatern;
    *ppstCaliPatern = reinterpret_cast<void*>(pTemp);
}
  //8点式标定方式
else if (static_cast<SV_S32>(SV_ENUM_CALIMETHOD_U8POINTS) == s32CaliMethodTemp) {
    SV_CALI_U8P_PATERN_S stPatern = *reinterpret_cast<SV_CALI_U8P_PATERN_S*>(pstCaliPatern[s32Ch]);
    SV_CALI_U8P_PATERN_S* pTemp = reinterpret_cast<SV_CALI_U8P_PATERN_S*>(malloc(sizeof(stPatern)));
    //申请内部空间，注意，需在外部由使用者释放
    if (NULL == pTemp) {
        LOG(ERROR) << "malloc SV_CALI_U8P_PATERN_S failed";
        return ISV_ENUM_AVM_MALLOC_FAILED;
    }
    *pTemp = stPatern;
    *ppstCaliPatern = reinterpret_cast<void*>(pTemp);
}
else ;
  //返回读取到的标定方式
  *ps32CaliMethod = s32CaliMethodTemp;
return ISV_ENUM_SUCCEED;
}//end of InnerSV_eGetCalibrateParternChannl

SV_S32 InnerSV_SvmParamClass::InnerSV_s32SetCalibratePartenChannl(
    const SV_S32& s32Method,
    const SV_S32& s32Ch,
    const SV_VOID* pstPatern) {
    CHECK(SV_TRUE == bInitialized);//断言，防止未从文件读取或初始化时的参数获取
    CHECK(s32Ch <= SV_ENUM_CAMERA_BACK);
SV_S32 s32PreCaliMethod = s32CaliMethod[s32Ch];
if (static_cast<SV_S32>(SV_ENUM_CALIMETHOD_UCHESSBORD) == s32Method) {
    SV_CALI_UCHESSBOARD_PATERN_S stPaternTemp = *reinterpret_cast<SV_CALI_UCHESSBOARD_PATERN_S*>(const_cast<void*>(pstPatern));
    //申请新的标定模板内存空间，并清空原有标定模板内存空间
    if (s32PreCaliMethod != s32Method) {
        SV_CALI_UCHESSBOARD_PATERN_S* pstTemp = reinterpret_cast<SV_CALI_UCHESSBOARD_PATERN_S*>(malloc(sizeof(stPaternTemp)));
        if (NULL == pstTemp) {
        LOG(ERROR) << "malloc SV_CALI_UCHESSBOARD_PATERN_S failed";
        return ISV_ENUM_AVM_MALLOC_FAILED;
        }
        if (pstCaliPatern[s32Ch] != NULL)
        free(pstCaliPatern[s32Ch]);
        pstCaliPatern[s32Ch] = reinterpret_cast<SV_VOID*>(pstTemp);
    }
    //替换标定模板的值
    *reinterpret_cast<SV_CALI_UCHESSBOARD_PATERN_S*>(pstCaliPatern[s32Ch]) = stPaternTemp;
}
else if (static_cast<SV_S32>(SV_ENUM_CALIMETHOD_U8POINTS) == s32Method )
{
    SV_CALI_U8P_PATERN_S stPaternTemp = *reinterpret_cast<SV_CALI_U8P_PATERN_S*>(const_cast<void*>(pstPatern));
    if (s32PreCaliMethod != s32Method) {
      //申请新的标定模板内存空间，并清空原有标定模板内存空间
        SV_CALI_U8P_PATERN_S* pstTemp = reinterpret_cast<SV_CALI_U8P_PATERN_S*>(malloc(sizeof(stPaternTemp)));
        if (NULL == pstTemp) {
        LOG(ERROR) << "malloc SV_CALI_U8P_PATERN_S failed";
        return ISV_ENUM_AVM_MALLOC_FAILED;
        }
        if (pstCaliPatern[s32Ch] != NULL)
        free(pstCaliPatern[s32Ch]);
        pstCaliPatern[s32Ch] = reinterpret_cast<SV_VOID*>(pstTemp);
        }
    //替换标定模板值
    *reinterpret_cast<SV_CALI_U8P_PATERN_S*>(pstCaliPatern[s32Ch]) = stPaternTemp;
}
s32CaliMethod[s32Ch] = s32Method;
return ISV_ENUM_SUCCEED;
}//end of InnerSV_SetCalibratePartenChannl


std::string InnerSV_SvmParamClass::InnerSV_s8GetNodeStrOfCameraParamEachChannl(const SV_S32& s32Ch) {
std::string s8NodeStr_temp = "CameraParam";
SV_S8 s8ChannlStr_temp = '0'+s32Ch;
s8NodeStr_temp+=s8ChannlStr_temp;
return s8NodeStr_temp;
}

std::string InnerSV_SvmParamClass::InnerSV_s8GetNodeStr0fCaliMethod(const SV_S32& s32Ch) {
std::string s8NodeStr_temp = "CaiMethod";
SV_S8 s8ChannlStr_temp = '0'+s32Ch;
s8NodeStr_temp+=s8ChannlStr_temp;
return s8NodeStr_temp;
}

std::string InnerSV_SvmParamClass::InnerSV_s8GetNodeStr0fUChessBoardPatern(const SV_S32& s32Ch){
std::string s8NodeStr_temp = "ChessBordPatern";
SV_S8 s8ChannlStr_temp = '0'+s32Ch;
s8NodeStr_temp+=s8ChannlStr_temp;
return s8NodeStr_temp;
}

std::string InnerSV_SvmParamClass::InnerSV_s8GetNodeStr0fU8PPatern(const SV_S32& s32Ch) {
std::string s8NodeStr_temp = "U8PPatern";
SV_S8 s8ChannlStr_temp = '0'+s32Ch;
s8NodeStr_temp+=s8ChannlStr_temp;
return s8NodeStr_temp;
}

SV_S32 InnerSV_SvmParamClass::IneerSV_s32ReadCameraParamCompFromXML(const SV_VOID* pCameraParamFileNode, const SV_S32& s32Ch,
    const SV_S32& eCamParamComp) {
const SV_S8* paCompNodeStr[SV_ENUM_CAMERA_SV_BUTT] = {ks8CameraKNodeStr, ks8CameraDistortNodeStr,
        ks8CameraRotateVectNodeStr, ks8CameraTranslateVectNodeStr};//  //各分量文件节点号
const SV_F64* pCompVal[4] = {stCameraParams[s32Ch].af64CameraK, stCameraParams[s32Ch].af64CameraDistort,
        stCameraParams[s32Ch].af64CameraRotateVect, stCameraParams[s32Ch].af64CameraTranslateVect};//各分量数组头
const SV_S32 s32CompSize[4] = {9, 4, 3, 3};//各分量Size,相机矩阵为3x3 ，畸变参数式1x4，旋转及平移是1x3

SV_S32 s32CompIdx = static_cast<SV_S32>(eCamParamComp);
  //获取FileNode
cv::FileNode cvNode_temp = *(reinterpret_cast<cv::FileNode*>(const_cast<SV_VOID*>(pCameraParamFileNode)));
CHECK(!cvNode_temp.isNone());
cv::FileNode cvNodeK_temp = cvNode_temp[paCompNodeStr[s32CompIdx]];
std::vector<SV_F64> f64MatReadValVect;
SV_S32 s32Ret;
if (ISV_ENUM_SUCCEED != (s32Ret = InnerSV_s32Read64FMatFromXML(cvNodeK_temp, &f64MatReadValVect))) {
    return s32Ret;
}
CHECK(s32CompSize[s32CompIdx] == f64MatReadValVect.size());
memcpy(const_cast<SV_F64*>(pCompVal[s32CompIdx]), &f64MatReadValVect[0], s32CompSize[s32CompIdx]*sizeof(f64MatReadValVect[0]));
return ISV_ENUM_SUCCEED;
}



SV_S32 InnerSV_SvmParamClass::InnerSV_s32ReadCameraParamFromXML(const SV_S32& s32Ch) {
std::string s8CameraParamNodeStr_temp = InnerSV_s8GetNodeStrOfCameraParamEachChannl(s32Ch);
  //获取FileNode
cv::FileStorage cvFs_temp = *(reinterpret_cast<cv::FileStorage*>(pXmlReadFd));
cv::FileNode cvNodeMain_temp = cvFs_temp[s8CameraParamNodeStr_temp];
SV_S32 s32Ret;//返回值
if (cvNodeMain_temp.isNone()) {
    return ISV_ENUM_AVM_XML_FILENODELOST;
}
  //读取各分量参数
for (SV_S32 i = 0;i < s32CameraNumber;++i) {
    if (ISV_ENUM_SUCCEED != (s32Ret = IneerSV_s32ReadCameraParamCompFromXML(reinterpret_cast<SV_VOID*>(&cvNodeMain_temp), s32Ch, i))) {
        return s32Ret;
    }
    //读取图形尺寸
    cv::FileNode n = cvNodeMain_temp[ks8CameraImageSizeNodeStr];
    if (n.isNone())
        return ISV_ENUM_AVM_XML_FILENODELOST;
    cv::Size stSize;
    n >> stSize;
    stCameraParams[s32Ch].stImageSize.s32Width = stSize.width;
    stCameraParams[s32Ch].stImageSize.s32Height = stSize.height;
}//end of for循环
return ISV_ENUM_SUCCEED;
}

SV_VOID InnerSV_SvmParamClass::InnerSV_WriteCameraParamToXML(const SV_S32 s32Ch)
{
  //各摄像头参数数组转Mat结构
std::vector<SV_F64> as64KVect(stCameraParams[s32Ch].af64CameraK, stCameraParams[s32Ch].af64CameraK+9);
std::vector<SV_F64> as64DistorVect(stCameraParams[s32Ch].af64CameraDistort, stCameraParams[s32Ch].af64CameraDistort+4);
std::vector<SV_F64> as64RotateVect(stCameraParams[s32Ch].af64CameraRotateVect, stCameraParams[s32Ch].af64CameraRotateVect+3);
std::vector<SV_F64> as64TanslateVect(stCameraParams[s32Ch].af64CameraTranslateVect, stCameraParams[s32Ch].af64CameraTranslateVect+3);
cv::Mat s64MatK(as64KVect);
cv::Mat s64MatDistor(as64DistorVect);
cv::Mat s64MatRot(as64RotateVect);
cv::Mat s64MatTrans(as64TanslateVect);

  //摄像头尺寸
cv::Size stCameraSize = cv::Size(stCameraParams[s32Ch].stImageSize.s32Width, stCameraParams[s32Ch].stImageSize.s32Height);

  //获取文件节点名
std::string s8CameraParamStr = InnerSV_s8GetNodeStrOfCameraParamEachChannl(s32Ch);
cv::FileStorage cvFs = *(reinterpret_cast<cv::FileStorage*>(pXmlWriteFd));
  //写入当前通道摄像头参数首阶文件节点
cvFs << s8CameraParamStr << "{";
        cvFs << ks8CameraKNodeStr << s64MatK;
        cvFs << ks8CameraDistortNodeStr << s64MatDistor;
        cvFs << ks8CameraRotateVectNodeStr << s64MatRot;
        cvFs << ks8CameraTranslateVectNodeStr << s64MatTrans;
        cvFs << ks8CameraImageSizeNodeStr << stCameraSize;
cvFs << "}";
return;
}

SV_S32 InnerSV_SvmParamClass::InnerSV_s32ReadVehicleSizeFromXML()
{
cv::FileStorage cvFs_temp = *(reinterpret_cast<cv::FileStorage*>(pXmlReadFd));
cv::FileNode cvNode = cvFs_temp[ksVehicleSizeNodeStr];
if (cvNode.isNone()) {
    return ISV_ENUM_AVM_XML_FILENODELOST;
}
cv::Size stSize;
cvNode >> stSize;
stVehicleSize = stSizeToSV_SIZE_S(stSize);
return ISV_ENUM_SUCCEED;
}

SV_VOID InnerSV_SvmParamClass::InnerSV_WriteVehicleSizeToXML()
{
cv::FileStorage cvFs_temp = *(reinterpret_cast<cv::FileStorage*>(pXmlWriteFd));
cv::Size stSize = stSV_SIZE_SToSize(stVehicleSize);
cvFs_temp << ksVehicleSizeNodeStr << stSize;
return;
}

SV_S32 InnerSV_SvmParamClass::InnerSV_s32ReadCaliMethodEachFromXML(const SV_S32& s32Ch, SV_S32* caliMethod) {
cv::FileStorage cvFs_temp = *(reinterpret_cast<cv::FileStorage*>(pXmlReadFd));
std::string s8MethodNode = InnerSV_s8GetNodeStr0fCaliMethod(s32Ch);
cv::FileNode n = cvFs_temp[s8MethodNode];
if (n.isNone()) {
    return ISV_ENUM_AVM_XML_FILENODELOST;
}
SV_S32 s32Temp;
n >> s32Temp;
  *caliMethod = s32Temp;
return ISV_ENUM_SUCCEED;
}

SV_S32 InnerSV_SvmParamClass::InnerSV_s32ReadUChessBoardPatern(const SV_S32& s32Ch) {
cv::FileStorage cvFs_temp = *(reinterpret_cast<cv::FileStorage*>(pXmlReadFd));
std::string s8PaternStr = InnerSV_s8GetNodeStr0fUChessBoardPatern(s32Ch);
cv::FileNode n = cvFs_temp[s8PaternStr];
if (n.isNone()) {
    return ISV_ENUM_AVM_XML_FILENODELOST;
}
SV_CALI_UCHESSBOARD_PATERN_S stPatern;//模板数据结构
n[ks8ChessPositionNodeStr] >> stPatern.s32ChessPosition;//读取chessPosition
cv::Size stChess;
n[ks8ChessBoardCenterNodeStr] >> stChess;
stPatern.stChessBoardCenter = stSizeToSV_SIZE_S(stChess);
n[ks8ChessBoardEdgeNodeStr] >> stChess;
stPatern.stChessBoardEdge = stSizeToSV_SIZE_S(stChess);
SV_S32 s32Gsize;
n[ks8GridSizeNodeStr] >> s32Gsize;
stPatern.s32GridSize = s32Gsize;
  //申请新的内存空间
SV_CALI_UCHESSBOARD_PATERN_S* pstParten = reinterpret_cast<SV_CALI_UCHESSBOARD_PATERN_S*>(malloc(sizeof(stPatern)));
if (NULL == pstParten) {
    return ISV_ENUM_AVM_MALLOC_FAILED;
}
  *pstParten = stPatern;
if (NULL != pstCaliPatern[s32Ch] &&  static_cast<SV_S32>(SV_ENUM_CALIMETHOD_UCHESSBORD) != s32CaliMethod[s32Ch]) {
    free(pstCaliPatern[s32Ch]);
    }
pstCaliPatern[s32Ch] = reinterpret_cast<SV_VOID*>(pstParten);
return ISV_ENUM_SUCCEED;
}

SV_S32 InnerSV_SvmParamClass::InnerSV_s32ReadU8PointPatern(const SV_S32& s32Ch) {
cv::FileStorage cvFs_temp = *(reinterpret_cast<cv::FileStorage*>(pXmlReadFd));
std::string s8PaternStr = InnerSV_s8GetNodeStr0fU8PPatern(s32Ch);
cv::FileNode n = cvFs_temp[s8PaternStr];
if (n.isNone()) {
    return ISV_ENUM_AVM_XML_FILENODELOST;
}
SV_CALI_U8P_PATERN_S stPaten;//模板数据结构
n[ks8U8PaternBoardSize] >> stPaten.s3U8PaternBoardSize;
cv::Mat imagePoint;
n[ks8U8ImagePointsNodeStr] >> imagePoint;
CHECK(CV_32FC2 == imagePoint.type());
SV_S32 S32PointNum = static_cast<SV_S32>(imagePoint.cols*imagePoint.rows);
CHECK(S32PointNum == sizeof(stPaten.stf32ImagePoints)/sizeof(stPaten.stf32ImagePoints[0]));
cv::Point2f* pstTemp = imagePoint.ptr<cv::Point2f>(0);
for (SV_S32 i = 0;i < S32PointNum;++i)
{
    stPaten.stf32ImagePoints[i] = stPoint2fToSV_POINT2F32_S(pstTemp[i]);
}
  //申请新的内存空间
SV_CALI_U8P_PATERN_S* pstParten = reinterpret_cast<SV_CALI_U8P_PATERN_S*>(malloc(sizeof(stPaten)));
if (NULL == pstParten) {
    return ISV_ENUM_AVM_MALLOC_FAILED;
}
  *pstParten = stPaten;
if (NULL != pstCaliPatern[s32Ch] &&  static_cast<SV_S32>(SV_ENUM_CALIMETHOD_U8POINTS) != s32CaliMethod[s32Ch]) {
    free(pstCaliPatern[s32Ch]);
    }
    pstCaliPatern[s32Ch] = reinterpret_cast<SV_VOID*>(pstParten);
    return ISV_ENUM_SUCCEED;
}

SV_S32 InnerSV_SvmParamClass::InnerSV_s32ReadCaliPartenEachFromXML(const SV_S32& s32Ch)
{
SV_S32 s32Ret;//返回值
SV_S32 s32Mode;//读取当前通道标定模式
if (ISV_ENUM_SUCCEED != (s32Ret = InnerSV_s32ReadCaliMethodEachFromXML(s32Ch, &s32Mode))) {
    return s32Ret;
}
if (static_cast<SV_S32>(SV_ENUM_CALIMETHOD_BUTT) <= s32Mode) {
    return ISV_ENUM_SUCCEED;
}
else if (static_cast<SV_S32>(SV_ENUM_CALIMETHOD_UCHESSBORD) == s32Mode) {
    if (ISV_ENUM_SUCCEED != (s32Ret = InnerSV_s32ReadUChessBoardPatern(s32Ch))) {
        return s32Ret;
    }
}
else {
    if (ISV_ENUM_SUCCEED != (s32Ret = InnerSV_s32ReadU8PointPatern(s32Ch)))
        return s32Ret;
}
s32CaliMethod[s32Ch] = s32Mode;
return ISV_ENUM_SUCCEED;
}

SV_VOID InnerSV_SvmParamClass::InnerSV_WriteUChessBoardPatern(const SV_S32& s32Ch) {
cv::FileStorage cvFs_temp = *(reinterpret_cast<cv::FileStorage*>(pXmlWriteFd));
SV_CALI_UCHESSBOARD_PATERN_S* pstPatern_temp = reinterpret_cast<SV_CALI_UCHESSBOARD_PATERN_S*>(pstCaliPatern[s32Ch]);
cvFs_temp << InnerSV_s8GetNodeStr0fUChessBoardPatern(s32Ch) << "{";
cvFs_temp << ks8ChessPositionNodeStr << pstPatern_temp->s32ChessPosition;
cv::Size stChessBoardTemp = stSV_SIZE_SToSize(pstPatern_temp->stChessBoardCenter);
cvFs_temp << ks8ChessBoardCenterNodeStr << stChessBoardTemp;
stChessBoardTemp = stSV_SIZE_SToSize(pstPatern_temp->stChessBoardEdge);
cvFs_temp << ks8ChessBoardEdgeNodeStr << stChessBoardTemp;
cvFs_temp << ks8GridSizeNodeStr << pstPatern_temp->s32GridSize;
cvFs_temp << "}";
return;
}
SV_VOID InnerSV_SvmParamClass::InnerSV_WriteU8PointPatern(const SV_S32& s32Ch) {
cv::FileStorage cvFs_temp = *(reinterpret_cast<cv::FileStorage*>(pXmlWriteFd));
SV_CALI_U8P_PATERN_S* pstPatern_temp =  reinterpret_cast<SV_CALI_U8P_PATERN_S*>(pstCaliPatern[s32Ch]);
cvFs_temp << InnerSV_s8GetNodeStr0fU8PPatern(s32Ch) << "{";
cvFs_temp << ks8U8PaternBoardSize << pstPatern_temp->s3U8PaternBoardSize;
  //以1X8的矩阵形式保存像素点
cv::Point2f* pstPoints_Temp =  reinterpret_cast<cv::Point2f*>(pstPatern_temp->stf32ImagePoints);
std::vector<cv::Point2f> stPointsVect(pstPoints_Temp, pstPoints_Temp+sizeof(pstPatern_temp->stf32ImagePoints)/sizeof(pstPatern_temp->stf32ImagePoints[0]));
cv::Mat mat_temp(stPointsVect);
cvFs_temp << ks8U8ImagePointsNodeStr << mat_temp;
cvFs_temp << "}";
return;
}

SV_VOID InnerSV_SvmParamClass::InnerSV_WriteCaliPartenEachToXML(const SV_S32& s32Ch) {

cv::FileStorage cvFs_temp = *(reinterpret_cast<cv::FileStorage*>(pXmlWriteFd));
  //写入标定模式
SV_S32 s32Mode = s32CaliMethod[s32Ch];
cvFs_temp << InnerSV_s8GetNodeStr0fCaliMethod(s32Ch) << s32Mode;
  //写入标定模板
if (static_cast<SV_S32>(SV_ENUM_CALIMETHOD_BUTT) <= s32Mode || pstCaliPatern[s32Ch] == NULL)
    return;
else if (static_cast<SV_S32>(SV_ENUM_CALIMETHOD_UCHESSBORD) >= s32Mode) {
    InnerSV_WriteUChessBoardPatern(s32Ch);
}
else {
    InnerSV_WriteU8PointPatern(s32Ch);
}
return;
}//end of InnerSV_WriteCaliPartenEachToXML

static const SV_S32 InnerSV_s32Read64FMatFromXML(const cv::FileNode& cvNode, std::vector<SV_F64>* paF64MatValVect) {
//  SV_BOOL bRet = static_cast<SV_BOOL>(cvNode.isNone());
 // if (SV_FALSE == bRet)
if (cvNode.isNone()) {
    LOG(ERROR) << "FileNode Lost";
    return ISV_ENUM_AVM_XML_FILENODELOST;
}
cv::Mat matTemp;
cvNode >> matTemp;
  //判断矩阵是否为SV_64F型
CHECK(CV_64FC1 == matTemp.type());
SV_S32 s32Size = static_cast<SV_S32>(matTemp.rows)*static_cast<SV_S32>(matTemp.cols);
SV_F64* pf64Temp = matTemp.ptr<SV_F64>(0);
for (SV_U32 i = 0;i < s32Size;++i) {
    paF64MatValVect->push_back(pf64Temp[i]);
}
return ISV_ENUM_SUCCEED;
}//end of InnerSV_bRead64FMatFromXML

}  // namespace svmparam
}  // namespace sv_avm
}  // namespace sm

