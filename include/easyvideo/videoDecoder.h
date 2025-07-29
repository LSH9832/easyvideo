#ifndef _VIDEO_DECODER_H_
#define _VIDEO_DECODER_H_

#include <iostream>
#include <string>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>
#include <opencv2/opencv.hpp>

#include "./videoCodecType.h"

class VideoDecoder {
public:
    VideoDecoder() {}; 
    VideoDecoder(int width, int height, int fps, int decode_id=CODEC_H264);
    int open_codec(int width, int height, int fps, int decode_id=CODEC_H264);
    int decodeFrame(uint8_t *inData, int inLen, cv::Mat &outMatV, bool readAll=false);
    
    int decode_id_=CODEC_H264;
    int width_;
    int height_;
    int fps_;

private:

    struct Impl;
    Impl *impl_=nullptr;

};


#endif  // _VIDEO_DECODER_H_