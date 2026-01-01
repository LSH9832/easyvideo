#ifndef EASYVIDEO_CAPTURE_CPP
#define EASYVIDEO_CAPTURE_CPP

#include <easyvideo/opencv/capture.h>

easyvideo::NormalCapture::NormalCapture() {};

easyvideo::NormalCapture::NormalCapture(std::string url, int apiPreference)
{
    cap.open(url, apiPreference);
}

bool easyvideo::NormalCapture::open(std::string url, int apiPreference)
{
    return cap.open(url, apiPreference);
}

bool easyvideo::NormalCapture::read(cv::Mat &frame)
{
    return cap.read(frame);
}

bool easyvideo::NormalCapture::isOpened()
{
    return cap.isOpened();
}

void easyvideo::NormalCapture::release()
{
    cap.release();
}

void easyvideo::NormalCapture::operator >> (cv::Mat &frame)
{
    cap >> frame;
}

void easyvideo::NormalCapture::set(int propId, double value)
{
    cap.set(propId, value);
}

double easyvideo::NormalCapture::get(int propId)
{
    return cap.get(propId);
}

easyvideo::NormalCapture::~NormalCapture()
{
    cap.release();
}

static bool stringStartswith(std::string& str_, const std::string& prefix) {
    size_t str_len = str_.length();
    size_t prefix_len = prefix.length();
    if (prefix_len > str_len) return false;
    return str_.find(prefix) == 0;
}

// bool stringEndswith(std::string& str_, const std::string& suffix) {
//     size_t str_len = str_.length();
//     size_t suffix_len = suffix.length();
//     if (suffix_len > str_len) return false;
//     return (str_.find(suffix, str_len - suffix_len) == (str_len - suffix_len));
// }

bool checkIsStream(std::string source)
{
    return stringStartswith(source, "rtsp://") || stringStartswith(source, "rtmp://");
}

easyvideo::BaseCapture* easyvideo::Capture::createCapture(
    std::string url, int apiPreference, 
    int type, bool dropFrame, 
    std::string decoder
)
{
    if (checkIsStream(url))
    {
        type = CAP_TYPE_STREAM;
    }
    switch (type)
    {
        case CAP_TYPE_NORMAL:
            std::cout << "NormalCapture" << std::endl;
            return new NormalCapture(url, apiPreference);
        case CAP_TYPE_JPEG:
            std::cout << "MJPG2BGRCapture" << std::endl;
            return new MJPG2BGRCapture(url, apiPreference);
        case CAP_TYPE_YUYV:
            std::cout << "YUYV2BGRCapture" << std::endl;
            return new YUYV2BGRCapture(url, apiPreference);
        case CAP_TYPE_STREAM:
        {
            std::cout << "StreamCapture" << std::endl;
            auto streamCapture = new StreamCapture();
            streamCapture->set(STREAM_RECV_METHOD, dropFrame?STREAM_RECV_METHOD_DROP:STREAM_RECV_METHOD_BLOCK);
            if (decoder == "auto")
            {
                streamCapture->open(url, apiPreference);
            }
            else
            {
                streamCapture->open(url, decoder);
            }
            return &(*streamCapture);
        }
        default:
            return nullptr;
    }
}


#endif