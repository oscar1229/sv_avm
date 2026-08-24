cmake_minimum_required(VERSION 3.10)
project(sv_avm)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fPIC -fpermissive -w")

add_definitions(
    -DGLOG_USE_GLOG_EXPORT
    -DEGL_USE_X11
    -DLINUX
    -DSV_STONKAM_TEST
    -DSTTONKAM_DEBUG
    -DNDEBUG
    -DUBUNTU
)

# --- 依赖查找 ---
# OpenCV: 由 find_package 定位，不使用绝对路径。
# 若安装在非标准前缀，用 -DOpenCV_DIR=<...>/lib/cmake/opencv4 或 -DCMAKE_PREFIX_PATH=... 传入。
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui calib3d)

# glog (系统包 libgoogle-glog-dev)
find_package(glog QUIET)

# EGL / GLESv2 (通过 pkg-config 查找)
find_package(PkgConfig REQUIRED)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLES REQUIRED glesv2)

# X11
find_package(X11 REQUIRED)

# 线程
find_package(Threads REQUIRED)

# Optional MPP/VI live camera input. The static image demo still builds if MPP is absent.
set(MPP_ROOT /home/szl/code/mpp CACHE PATH "MPP source/build root")
set(SV_AVM_MPP_LIB "")
if(EXISTS ${MPP_ROOT}/include/sys/sys_api.h AND EXISTS ${MPP_ROOT}/build/lib/libmpp.so)
    add_definitions(-DSV_AVM_ENABLE_MPP)
    set(SV_AVM_MPP_LIB ${MPP_ROOT}/build/lib/libmpp.so)
    set(SV_AVM_MPP_INCLUDE_DIRS
        ${MPP_ROOT}/include
        ${MPP_ROOT}/include/sys
        ${MPP_ROOT}/include/vi
        ${MPP_ROOT}/include/v2d
        ${MPP_ROOT}/include/venc
        ${MPP_ROOT}/include/vdec
        ${MPP_ROOT}/utils/include
        ${MPP_ROOT}/al/include
    )
    message(STATUS "MPP live VI input enabled: ${MPP_ROOT}")
else()
    message(WARNING "MPP not found at ${MPP_ROOT}; --live-vi will be unavailable. Pass -DMPP_ROOT=<path> to enable it.")
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/..
    ${OpenCV_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLES_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
    ${SV_AVM_MPP_INCLUDE_DIRS}
)

# ============================================================
# 库拆分：src 下的三个模块各封装为独立库
# (测试程序已移至与 src 同级的 sv_avm_test 目录)
#   svmparam     - 基础参数模块 (无内部依赖)
#   svmcalibrate - 标定模块 (依赖 svmparam)
#   svrender     - 渲染模块 (依赖 svmparam)
# ============================================================

# --- svmparam ---
file(GLOB_RECURSE SVMPARAM_SRCS src/svmparam/*.cpp)
add_library(svmparam SHARED ${SVMPARAM_SRCS})
target_link_libraries(svmparam
    ${OpenCV_LIBS}
    glog::glog
    Threads::Threads
)

# --- svmcalibrate ---
file(GLOB_RECURSE SVMCALIBRATE_SRCS src/svmcalibrate/*.cpp)
add_library(svmcalibrate SHARED ${SVMCALIBRATE_SRCS})
target_link_libraries(svmcalibrate
    svmparam
    ${OpenCV_LIBS}
    glog::glog
    Threads::Threads
)

# --- svrender ---
file(GLOB_RECURSE SVRENDER_SRCS src/svrender/*.cpp)
add_library(svrender SHARED ${SVRENDER_SRCS})
target_link_libraries(svrender
    svmparam
    ${OpenCV_LIBS}
    glog::glog
    ${GLES_LIBRARIES} ${EGL_LIBRARIES}
    ${X11_LIBRARIES}
    Threads::Threads
    dl assimp
)

# --- 安装前缀：默认装到工程内 install/，不污染 /usr/local ---
# 用户显式传 -DCMAKE_INSTALL_PREFIX=... 时以用户指定为准。
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install CACHE PATH "install prefix" FORCE)
endif()
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

# --- 安装：导出三个库到 install 目录 ---
install(TARGETS svmparam svmcalibrate svrender
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES
    ${CMAKE_SOURCE_DIR}/include/svrender.hpp
    ${CMAKE_SOURCE_DIR}/include/svmcalibrate.hpp
    ${CMAKE_SOURCE_DIR}/include/sv_avmcommon.hpp
    ${CMAKE_SOURCE_DIR}/include/sv_avm.hpp
    ${CMAKE_SOURCE_DIR}/src/svmparam/svmparam.hpp
    DESTINATION include
)

# 测试程序
set(TEST_DIR ${CMAKE_SOURCE_DIR}/sv_avm_test)

add_executable(sv_avm_render_test ${TEST_DIR}/sv_avm_render_main_test.cpp)

target_link_libraries(sv_avm_render_test
    svrender
    svmcalibrate
    svmparam
    ${OpenCV_LIBS}
    glog::glog
    ${GLES_LIBRARIES} ${EGL_LIBRARIES}
    ${X11_LIBRARIES}
    Threads::Threads
    dl assimp
    ${SV_AVM_MPP_LIB}
)

# 构建后把运行所需的资源拷贝到可执行文件同级目录，使其可直接运行
# 注意：config.json 不拷贝。程序以 ../config.json (工程根目录) 为唯一配置来源，
#       拷贝一份到 build/ 只会造成"改了却不生效"的误解。
add_custom_command(TARGET sv_avm_render_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${TEST_DIR}/res $<TARGET_FILE_DIR:sv_avm_render_test>/res
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${TEST_DIR}/_aParam.xml $<TARGET_FILE_DIR:sv_avm_render_test>/_aParam.xml
)
