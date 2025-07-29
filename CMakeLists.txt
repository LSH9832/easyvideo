cmake_minimum_required(VERSION 3.10)
project(RtspTest)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-std=c++14 -O3 -fPIC -w ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS -pthread)
set(CMAKE_BUILD_TYPE Release)

link_directories(/usr/lib/x86_64-linux-gnu)
include_directories(
    ./include
    /usr/include/opencv4
)

find_package(OpenCV 4 REQUIRED)

add_library(easyvideo SHARED
    src/stream.cpp
    src/streamCapture.cpp
    src/videoDecoder_hw.cpp
    src/push.cpp
    src/jpegutils.cpp
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
