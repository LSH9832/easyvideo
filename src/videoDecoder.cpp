#ifndef VIDEO_DECODER_CPP
#define VIDEO_DECODER_CPP

#include "easyvideo/videoDecoder.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
#include <libavutil/hwcontext.h>
#include <libavutil/frame.h>
}


// #define ENABLE_RKMPP
#ifdef ENABLE_RKMPP
#include "./platform/rockchip/rkmpp_videoDecoder.cpp"
#endif
#include "./platform/ffmpeg/ffmpeg_videoDecoder.cpp"


VideoDecoder* getVideoDecoder(CodecPlatform plat)
{
    if (plat == CODEC_PLATFORM_FFMPEG)
    {
        return new FFMPEGVideoDecoder();
    }
    else if (plat == CODEC_PLATFORM_RKMPP)
    {
#ifdef ENABLE_RKMPP
        return new RKMPPVideoDecoder();
#else
        std::cerr << "RKMPP is not support." << std::endl;
        return new FFMPEGVideoDecoder();
#endif
    }
    else
    {
        return new FFMPEGVideoDecoder();
    }
}

#endif  // VIDEO_DECODER_CPP
