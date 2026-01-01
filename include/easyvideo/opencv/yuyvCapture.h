#ifndef YUYV2BGR_H
#define YUYV2BGR_H

// #include <opencv2/opencv.hpp>
#include "./baseCapture.h"

namespace easyvideo
{
class YUYV2BGRCapture: public BaseCapture
{
public:
    YUYV2BGRCapture();

    YUYV2BGRCapture(std::string source, int apiPreference);

    bool open(std::string source, int apiPreference=0);

    bool isOpened();

    void release();

    void set(int propId, double value);

    double get(int propId);

    bool read(cv::Mat& image);

    void operator>>(cv::Mat& image);

private:
    cv::VideoCapture cap;
    cv::Mat recvImage, outImage;
    cv::Size sz;
    int area = 0;
    bool first = true;
    size_t step=1;

};

}

#endif