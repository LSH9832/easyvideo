#ifndef BASE_CAPTURE_H
#define BASE_CAPTURE_H


#include <opencv2/opencv.hpp>

namespace easyvideo
{
class BaseCapture
{
public:
    BaseCapture() {};

    ~BaseCapture() {release();}

    virtual bool open(std::string url, int apiPreference=::cv::CAP_ANY) {return false;}

    virtual bool read(::cv::Mat &frame) {return false;}

    virtual bool isOpened() {return false;}

    virtual void release() {}

    virtual void operator >> (::cv::Mat &frame) {}

    virtual void set(int propId, double value) {}

    virtual double get(int propId) {return 0;}

    // ~BaseCapture() = default;
};
}
#endif // BASE_CAPTURE_H