#ifndef JPEG2RGB_H
#define JPEG2RGB_H


// #include <opencv2/opencv.hpp>
#include "./baseCapture.h"

#include <stdio.h>
// #include <stdlib.h>
#include <string.h>


void jpg2rgb(unsigned char *data, unsigned char *buffer_, size_t& data_length);


namespace easyvideo
{
class MJPG2BGRCapture: public BaseCapture
{
public:
    MJPG2BGRCapture() {}


    MJPG2BGRCapture(std::string source, int apiPreference)
    {
        open(source, apiPreference);
        // cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
        // cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    }

    bool open(std::string source, int apiPreference=0)
    {
        bool success = cap.open(source, apiPreference);
        if(success)
        {
            // cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
            // cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);        
            sz.height = cap.get(::cv::CAP_PROP_FRAME_HEIGHT);
            sz.width = cap.get(::cv::CAP_PROP_FRAME_WIDTH);
            setSize(sz);
            // cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
            cap.set(::cv::CAP_PROP_FOURCC, ::cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
            cap.set(::cv::CAP_PROP_CONVERT_RGB, 0);
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
        if (!isOpened()) return;
        cap.set(propId, value);
        if (propId == 4) {sz.height = cap.get(propId);}
        if (propId == 3) {sz.width = cap.get(propId);}
        setSize(sz);
    }

    double get(int propId)
    {
        if (!isOpened()) return -1;
        return cap.get(propId);
    }

    bool read(::cv::Mat& image)
    {
        bool success = cap.read(recvImage);
        if (success)
        {
            if (first)
            {
                outImage = cv::Mat(sz, CV_8UC3);
                first = false;
            }
            size_t datasize = 0;
            for (;&recvImage.data[datasize] != &recvImage.dataend[0];datasize++);
            if (datasize != last_size) jpg2rgb(recvImage.data, outImage.data, datasize);
            // else std::cout << "skip" << std::endl;
            image = outImage;
            last_size = datasize;
        }
        return success;
    }

    void operator>>(::cv::Mat& image)
    {
        read(image);
    }

private:
    ::cv::VideoCapture cap;
    ::cv::Mat recvImage, outImage;
    ::cv::Size sz;
    int area = 0;
    bool first = true;
    size_t last_size=0;

    void setSize(cv::Size sz_)
    {
        sz = sz_;
        area = sz.area();
    }

};
}


#endif