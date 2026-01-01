#ifndef RESIZE_H
#define RESIZE_H

#include <opencv2/opencv.hpp>

float static_resize(cv::Mat& src, cv::Mat& dist, cv::Size input_size) 
{
    float ratio = std::min(input_size.width / (src.cols*1.0), input_size.height / (src.rows*1.0));
    int unpad_w = (int)round(ratio * src.cols);
    int unpad_h = (int)round(ratio * src.rows);

    cv::Mat re(unpad_h, unpad_w, CV_8UC3);
    cv::resize(src, re, re.size());
    dist = cv::Mat(input_size.height, input_size.width, CV_8UC3, cv::Scalar(114, 114, 114));
    re.copyTo(dist(cv::Rect(0, 0, re.cols, re.rows)));
    return 1./ ratio;
}

#endif