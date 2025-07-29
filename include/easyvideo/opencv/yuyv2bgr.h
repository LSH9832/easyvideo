#ifndef YUYV2BGR_H
#define YUYV2BGR_H

// #include <opencv2/opencv.hpp>
#include "./baseCapture.h"


#define YUYV2BGR_RV 1.403
#define YUYV2BGR_GU -0.344
#define YUYV2BGR_GV -0.714
#define YUYV2BGR_BU 1.77
#define YUYV2BGR_BIAS 127.5


void yuyv2bgr(uchar* yuyv, uchar* bgr, size_t size)
{
    int y1=0, u=0, y2=0, v=0;
    for (int i=0; i<size/2; ++i)
    {
        y1 = yuyv[4 * i + 0];
        u = yuyv[4 * i + 1];
        y2 = yuyv[4 * i + 2];
        v = yuyv[4 * i + 3];
        
        bgr[6 * i + 0] = MAX(0, MIN((int)(YUYV2BGR_BU * (u - YUYV2BGR_BIAS) + y1), 255));  // b1
        bgr[6 * i + 1] = MAX(0, MIN(255, (int)(YUYV2BGR_GU * (u - YUYV2BGR_BIAS) + YUYV2BGR_GV * (v - YUYV2BGR_BIAS) + y1)));  // g1
        bgr[6 * i + 2] = MAX(0, MIN(255, (int)(YUYV2BGR_RV * (v - YUYV2BGR_BIAS) + y1)));  // r1

        bgr[6 * i + 3] = MAX(0, MIN((int)(YUYV2BGR_BU * (u - YUYV2BGR_BIAS) + y2), 255));  // b2
        bgr[6 * i + 4] = MAX(0, MIN(255, (int)(YUYV2BGR_GU * (u - YUYV2BGR_BIAS) + YUYV2BGR_GV * (v - YUYV2BGR_BIAS) + y2)));  // g2
        bgr[6 * i + 5] = MAX(0, MIN(255, (int)(YUYV2BGR_RV * (v - YUYV2BGR_BIAS) + y2)));  // r2
    }
}

namespace easyvideo
{
class YUYV2BGRCapture: public BaseCapture
{
public:
    YUYV2BGRCapture() {}

    YUYV2BGRCapture(std::string source, int apiPreference)
    {
        cap.open(source, apiPreference);
        cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
        cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V'));
    }

    bool open(std::string source, int apiPreference=0)
    {
        bool success = cap.open(source, apiPreference);
        if(success)
        {
            cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
            cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
        }
        return success;
    }

    bool isOpened()
    {
        return cap.isOpened();
    }

    void release()
    {
        cap.release();
    }

    void set(int propId, double value)
    {
        cap.set(propId, value);
    }

    double get(int propId)
    {
        return cap.get(propId);
    }

    bool read(cv::Mat& image)
    {
        bool success = cap.read(recvImage);
        if (success)
        {
            if (first)
            {
                sz = recvImage.size();
                area = sz.area();
                outImage = cv::Mat(sz, CV_8UC3);
                first = false;
            }
            yuyv2bgr(recvImage.data, outImage.data, area);
            image = outImage;
        }
        return success;
    }

    void operator>>(cv::Mat& image)
    {
        read(image);
    }

private:
    cv::VideoCapture cap;
    cv::Mat recvImage, outImage;
    cv::Size sz;
    int area = 0;
    bool first = true;

};

}

#endif