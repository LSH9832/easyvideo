#ifndef ALL_CAPTURE_H
#define ALL_CAPTURE_H

#include "./jpegCapture.h"
#include "./streamCapture.h"
#include "./yuyvCapture.h"
// #include "easycpp/str.h"

namespace easyvideo
{
class NormalCapture: public BaseCapture
{
public:
    NormalCapture();

    NormalCapture(std::string url, int apiPreference=cv::CAP_ANY);

    bool open(std::string url, int apiPreference=cv::CAP_ANY);

    bool read(cv::Mat &frame);

    bool isOpened();

    void release();

    void operator >> (cv::Mat &frame);

    void set(int propId, double value);

    double get(int propId);

    ~NormalCapture();

private:
    ::cv::VideoCapture cap;
};


namespace Capture
{
    enum CaptureType
    {
        CAP_TYPE_NORMAL,
        CAP_TYPE_JPEG,
        CAP_TYPE_YUYV,
        CAP_TYPE_STREAM
    };

    BaseCapture* createCapture(
        std::string url, int apiPreference=cv::CAP_ANY, 
        int type=CAP_TYPE_NORMAL, bool dropFrame=false, std::string decoder="auto"
    );
}
}



#endif