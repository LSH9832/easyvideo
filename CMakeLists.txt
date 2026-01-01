cmake_minimum_required(VERSION 3.10)
project(easyvideo)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-std=c++14 -O3 -fPIC -w ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS -pthread)
# set(CMAKE_BUILD_TYPE Release)

option(ENABLE_RKMPP "enable using rkmpp and rga to accelerate" OFF)

if(ENABLE_RKMPP)
add_definitions(-DENABLE_RKMPP)
endif()

link_directories(
    /usr/local/ffmpeg/lib
    # /usr/lib/x86_64-linux-gnu
)
include_directories(
    /usr/local/ffmpeg/include
    ./include
    /usr/include/opencv4
)

find_package(OpenCV 4 REQUIRED)

add_library(easyvideo SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/capture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/streamCapture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jpegCapture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/yuyvCapture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/videoEncoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/videoDecoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/videoWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/push.cpp
)

target_link_libraries(easyvideo
    ${OpenCV_LIBS}
    jpeg
    avformat
    avcodec
    avdevice
    avfilter
    avutil
    postproc
    swresample
    swscale
)


if(ENABLE_RKMPP)
target_link_libraries(easyvideo
    rockchip_mpp
    rga
)
endif()

add_executable(pushRTSP
    demo/pushRTSP.cpp
)

target_link_libraries(pushRTSP
    ${OpenCV_LIBS}
    easyvideo
    # jpeg
    # avformat
    # avcodec
    # avdevice
    # avfilter
    # avutil
    # postproc
    # swresample
    # swscale
)
