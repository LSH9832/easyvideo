#ifndef VIDEO_CODEC_TYPE_H
#define VIDEO_CODEC_TYPE_H

enum CODEC {
    CODEC_AUTO,
    CODEC_H264 = 27,
    CODEC_H265 = 173,
};


enum CodecPlatform
{
    CODEC_PLATFORM_FFMPEG,
    CODEC_PLATFORM_RKMPP
};

#endif