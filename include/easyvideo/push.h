//
// Created by wensi on 2023/10/17.
//

#ifndef EASYVIDEO_PUSH_H
#define EASYVIDEO_PUSH_H

#include <iostream>
#include <string>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>
#include <opencv2/core/opengl.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
#include <libavutil/hwcontext.h>
}

namespace easyvideo
{

class RTSPPusher {
public:
    RTSPPusher(std::string url);
    void start();
    void stop();
    void push_frame(cv::Mat &frame);
    void pushFrameData(cv::Mat &frame);
    int open_codec(int width, int height, int den, int kB=100, std::string encoder_name="");
    bool isConnected();

private:
    int push();
    AVFrame *CVMatToAVFrame(cv::Mat &inMatV, int YUV_TYPE);
    cv::Mat pop_one_frame();

    void popOneFrameData(cv::Mat &outMat);

private:
    std::mutex queue_mutex, connect_mutex;
    std::string url;
    std::queue<cv::Mat> pic_buffer;
    std::thread push_thread;
    std::condition_variable conditionVariable, conditionVariable2;

    AVCodecContext *outputVc;
    int fps=30;
    AVFormatContext *output;
    bool outputConnected_ = false;
    AVStream *vs;

    SwsContext* sws_ctx=nullptr;

    bool enable_hardware=false;
};


}



#endif // EASYVIDEO_PUSH_H
