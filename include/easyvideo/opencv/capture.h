#ifndef ALL_CAPTURE_H
#define ALL_CAPTURE_H

#include "./jpeg2rgb.h"
#include "./streamCapture.h"
#include "./yuyv2bgr.h"
#include "pylike/str.h"

namespace easyvideo
{
namespace Capture
{
    enum CaptureType
    {
        CAP_TYPE_NORMAL,
        CAP_TYPE_JPEG,
        CAP_TYPE_YUYV,
        CAP_TYPE_STREAM
    };


    class NormalCapture: public BaseCapture
    {
    public:
        NormalCapture() {};

        NormalCapture(std::string url, int apiPreference=cv::CAP_ANY)
        {
            cap.open(url, apiPreference);
        }

        bool open(std::string url, int apiPreference=cv::CAP_ANY)
        {
            return cap.open(url, apiPreference);
        }

        bool read(cv::Mat &frame)
        {
            return cap.read(frame);
        }

        bool isOpened()
        {
            return cap.isOpened();
        }

        void release()
        {
            cap.release();
        }

        void operator >> (cv::Mat &frame)
        {
            cap >> frame;
        }

        void set(int propId, double value)
        {
            cap.set(propId, value);
        }

        double get(int propId)
        {
            return cap.get(propId);
        }

        ~NormalCapture()
        {
            cap.release();
        }

    private:
        cv::VideoCapture cap;
    };


    BaseCapture* createCapture(std::string url, int apiPreference=cv::CAP_ANY, int type=CAP_TYPE_JPEG, bool dropFrame=false)
    {
        pystring url_(url);
        if (url_.startswith("rtsp://") || url_.startswith("rtmp://"))
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
                streamCapture->open(url, apiPreference);
                return &(*streamCapture);
            }
            default:
                return nullptr;
        }
    }
}
}



#endif