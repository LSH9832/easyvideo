#ifndef JPEG2RGB_H
#define JPEG2RGB_H


// #include <opencv2/opencv.hpp>
#include "./baseCapture.h"

#include <stdio.h>
// #include <stdlib.h>
#include <string.h>


namespace easyvideo
{
void jpg2rgb(unsigned char *data, unsigned char *buffer_, size_t& data_length);

class MJPG2BGRCapture: public BaseCapture
{
public:
    MJPG2BGRCapture();

    MJPG2BGRCapture(std::string source, int apiPreference);

    bool open(std::string source, int apiPreference=0);

    bool isOpened();

    void release();

    void set(int propId, double value);

    double get(int propId);

    bool read(::cv::Mat& image);

    void operator>>(::cv::Mat& image);

private:
    ::cv::VideoCapture cap;
    ::cv::Mat recvImage, outImage;
    ::cv::Size sz;
    int area = 0;
    bool first = true;
    size_t last_size=0;

    void setSize(cv::Size sz_);

};
}


#endif
