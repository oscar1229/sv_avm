cmake_minimum_required(VERSION 3.10)
project(sv_avm_demo)

# Build the example against installed sv_avm libraries and sibling MPP source.
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

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui calib3d)
find_package(glog QUIET)
find_package(PkgConfig REQUIRED)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLES REQUIRED glesv2)
find_package(X11 REQUIRED)
find_package(Threads REQUIRED)

set(MPP_ROOT "${CMAKE_SOURCE_DIR}/../../../multimedia/mpp/" CACHE PATH "MPP source directory")
get_filename_component(MPP_ROOT "${MPP_ROOT}" ABSOLUTE)
if(NOT EXISTS "${MPP_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "MPP source tree not found at ${MPP_ROOT}")
endif()
set(BUILD_TESTS OFF CACHE BOOL "Build MPP tests" FORCE)
add_subdirectory("${MPP_ROOT}" "${CMAKE_BINARY_DIR}/mpp" EXCLUDE_FROM_ALL)

# MPP is a subproject here, so CMAKE_SOURCE_DIR points at this project, not MPP.
# mpi/mux/CMakeLists.txt uses ${CMAKE_SOURCE_DIR}/mpi/demux to find codec/h264_utils.h
# and other demux helpers; that path resolves to a non-existent directory and
# breaks the build. Add the real path here to avoid patching the shared MPP checkout.
if(TARGET mpp_mux)
  target_include_directories(mpp_mux PRIVATE "${MPP_ROOT}/mpi/demux")
else()
  message(FATAL_ERROR "mpp_mux target missing: MPP layout changed")
endif()
add_definitions(-DSV_AVM_ENABLE_MPP)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLES_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
    ${MPP_ROOT}/include ${MPP_ROOT}/include/sys ${MPP_ROOT}/include/vi
    ${MPP_ROOT}/include/v2d ${MPP_ROOT}/include/venc ${MPP_ROOT}/include/vdec
    ${MPP_ROOT}/utils/include ${MPP_ROOT}/al/include
)

set(SV_AVM_LIB_DIR "${CMAKE_SOURCE_DIR}/lib")
foreach(_lib svmparam svmcalibrate svrender)
    if(NOT EXISTS "${SV_AVM_LIB_DIR}/lib${_lib}.so")
        message(FATAL_ERROR "Installed sv_avm library missing: lib${_lib}.so")
    endif()
    add_library(${_lib} SHARED IMPORTED)
    set_target_properties(${_lib} PROPERTIES
        IMPORTED_LOCATION "${SV_AVM_LIB_DIR}/lib${_lib}.so")
endforeach()

add_executable(sv_avm_render_test sv_avm_test/sv_avm_render_main_test.cpp)
if(TARGET vi_k3_cam_plugin)
    add_dependencies(sv_avm_render_test vi_k3_cam_plugin)
elseif(TARGET vi_k1_cam_plugin)
    add_dependencies(sv_avm_render_test vi_k1_cam_plugin)
endif()
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
    mpp
)

get_filename_component(_opencv_libdir "${OpenCV_DIR}/../.." ABSOLUTE)
set_target_properties(sv_avm_render_test PROPERTIES
    BUILD_RPATH "${SV_AVM_LIB_DIR}:${_opencv_libdir}")
