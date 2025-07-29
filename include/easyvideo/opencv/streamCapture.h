#ifndef STREAMCAPTURE_H
#define STREAMCAPTURE_H

#include "./baseCapture.h"
#define STREAM_RECV_METHOD -100

namespace easyvideo
{
enum StreamRecvMethod
{
    STREAM_RECV_METHOD_BLOCK,
    STREAM_RECV_METHOD_DROP
};


class StreamCapture: public BaseCapture
{
public:
    StreamCapture() {};

    StreamCapture(std::string url, int apiPreference=cv::CAP_ANY);

    bool open(std::string url, int apiPreference=cv::CAP_ANY);

    bool read(cv::Mat &frame);

    bool isOpened();

    void release();

    void operator >> (cv::Mat &frame);

    void set(int propId, double value);

    double get(int propId);

    ~StreamCapture();

private:
    void* impl_=nullptr;
};

}

#endif // STREAMCAPTURE_H