#ifndef EASYVIDEO_VIDEOWRITER_H
#define EASYVIDEO_VIDEOWRITER_H

#include "./videoEncoder.h"


class VideoWriter
{
public:
    VideoWriter();

    ~VideoWriter();

    bool init(std::string dest, int width, int height, int fps, int kB=512, 
              std::string encoder_name="libx264", int num_threads=4, int gop=10);

    bool write(cv::Mat& image);

    bool is_init();

    bool release();


private:

    VideoEncoder* encoder_ = nullptr;
    bool init_ = false;

    struct Impl;
    Impl *impl_=nullptr;

};




#endif