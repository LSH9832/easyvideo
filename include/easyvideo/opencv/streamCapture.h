#ifndef STREAMCAPTURE_H
#define STREAMCAPTURE_H

#include "./baseCapture.h"
#define STREAM_RECV_METHOD -100
#define STREAM_RECV_STEP -200

namespace easyvideo
{
enum StreamRecvMethod
{
    STREAM_RECV_METHOD_BLOCK,
    STREAM_RECV_METHOD_DROP
};

struct streamData
{
    void* data=nullptr;
    int size=0;
    int64_t dts=0, pts=0;
    bool isKeyFrame=false;
};

class StreamCapture: public BaseCapture
{
public:

    StreamCapture() {};

    StreamCapture(std::string url, int apiPreference=cv::CAP_ANY);

    bool open(std::string url, int apiPreference=cv::CAP_ANY);

    bool open(std::string url, std::string decoder_name="");

    bool read(cv::Mat &frame);

    /**
     * read origin data without decoding
     */
    bool readStream(streamData& data);

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