#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include <iostream>
#include <opencv2/opencv.hpp>
#include "./videoCodecType.h"


class VideoEncoder {
public:
    VideoEncoder() {}; 
    VideoEncoder(int width, int height, int fps, int kB=100, int encode_id=CODEC_H264, int num_threads=4, int gop=30);
    VideoEncoder(int width, int height, int fps, int kB=100, std::string encoder_name="libx264", int num_threads=4, int gop=30);

    void release();

    int resetByteRate(int kB);
    int getBitRate();

    int open_codec(int width, int height, int fps, int kB=100, int encode_id=CODEC_H264, int num_threads=4, int gop=30);
    int open_codec(int width, int height, int fps, int kB=100, std::string encoder_name="libx264", int num_threads=4, int gop=30);

    int encodeFrame(cv::Mat &frame, uint8_t *outData, int &outLen);

    int encodeFrame(cv::Mat &frame, void* packet);

    cv::Size encodeSize(int stride=16);   // maybe diffirent from original size while using mpp encoder
    
    int encode_id_=CODEC_H264;

    int width_=0;
    int height_=0;
    int fps_=0;

private:
    int kB_=100;
    struct Impl;
    Impl *impl_=nullptr;
};


#endif // VIDEOENCODER_H