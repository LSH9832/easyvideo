#ifndef STREAMCAPTURE_CPP
#define STREAMCAPTURE_CPP

#include "easyvideo/opencv/streamCapture.h"
#include "easyvideo/opencv/stream.h"
#include "easyvideo/videoDecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
}


#define STREAM_CAP_FIRST_READ_TIMES 100

using namespace easyvideo;

struct StreamCaptureHandler
{
    Stream stream;
    // AVFormatContext *input_ctx = nullptr;
    // AVStream *video = nullptr;
    // AVPacket packet;
    // int video_stream, ret;
    int ret;
    VideoDecoder* decoder=nullptr;
    bool no_decoder=false;
    bool isOpened = false;
    int firstRead = STREAM_CAP_FIRST_READ_TIMES;
    int fps=-1;
    int recvMethod = STREAM_RECV_METHOD_BLOCK;

    std::mutex imgProcessMutex1, imgProcessMutex2;
    std::condition_variable imgProcessCond1, imgProcessCond2;
    bool imgProcessing = false;
    bool stopThread = false;
    bool tempJudgeEqual = false;
    uint64_t count_inner = 0;
    uint64_t count_outer = 0;

    std::thread recv_t;

    cv::Mat currentImg;

    bool readStream(streamData& sdata)
    {
        if (!isOpened)
        {
            return false;
        }
        sdata.data = stream.read(sdata.size, sdata.pts, sdata.dts, sdata.isKeyFrame);
        if (sdata.data == nullptr || sdata.size <= 0)
        {
            return false;
        }
        return true;
    }

    bool read(cv::Mat& img)
    {
        if (!isOpened)
        {
            return false;
        }
        int size = 0;
        void* data = stream.read(size);
        if (data == nullptr || size <= 0)
        {
            return false;
        }
        ret = decoder->decodeFrame((uint8_t*)data, size, img);
        bool success = ret == 0;
        if (!success && firstRead)
        {
            firstRead--;
            success = read(img);
        }
        if (firstRead)
        {
            firstRead = false;
        }
        return success;
    }

    bool getFrame(cv::Mat& frame, uint64_t& id)
    {
        std::unique_lock<std::mutex> lock(imgProcessMutex1);
        imgProcessCond2.wait(lock, [this, id](){return (!imgProcessing && (id != count_inner)) || stopThread;});
        if (stopThread)
        {
            return false;
        }
        imgProcessing = true;
        imgProcessCond1.notify_one();
        frame = currentImg.clone();
        id = count_inner;
        imgProcessing = false;
        imgProcessCond1.notify_one();
        return true;
    }

    void recvThread()
    {
        if (!isOpened)
        {
            return;
        }

        while (!stopThread)
        {
            // std::cout << "recvThread" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            std::unique_lock<std::mutex> lock(imgProcessMutex2);
            imgProcessCond1.wait(lock, [this](){return !imgProcessing || stopThread;});
            if (stopThread)
            {
                return;
            }
            imgProcessing = true;
            imgProcessCond2.notify_one();
            bool success = read(currentImg);
            imgProcessing = false;
            if (success)
            {
                count_inner++;
            }
            imgProcessCond2.notify_one();
            
            
        }

    }


};


StreamCapture::StreamCapture(std::string url, int apiPreference)
{
    open(url, apiPreference);
}

StreamCapture::~StreamCapture()
{
    release();
}

static bool stringStartswith(std::string& str_, const std::string& prefix) {
    size_t str_len = str_.length();
    size_t prefix_len = prefix.length();
    if (prefix_len > str_len) return false;
    return str_.find(prefix) == 0;
}

static bool stringEndswith(std::string& str_, const std::string& suffix) {
    size_t str_len = str_.length();
    size_t suffix_len = suffix.length();
    if (suffix_len > str_len) return false;
    return (str_.find(suffix, str_len - suffix_len) == (str_len - suffix_len));
}

bool StreamCapture::open(std::string url, std::string decoder_name)
{
    if (isOpened())
    {
        release();
    }

    if (impl_ == nullptr)
    {
        impl_ = new StreamCaptureHandler();
    }
    auto impl = static_cast<StreamCaptureHandler*>(impl_);
    impl->isOpened = false;

    int ret = impl->stream.open(url);
    if (ret < 0)
    {
        fprintf(stderr, "Cannot open stream.\n");
        return false;
    }

    if (decoder_name.empty())
    {
        std::cout << "decoder name empty, origin data only" << std::endl;
        impl->no_decoder = true;
    }
    else
    {
        if (stringEndswith(decoder_name, "_rkmpp"))
        {
            impl->decoder = getVideoDecoder(CODEC_PLATFORM_RKMPP);
        }
        else
        {
            impl->decoder = getVideoDecoder(CODEC_PLATFORM_FFMPEG);
        }

        ret = impl->decoder->open_codec(
            impl->stream.width(), 
            impl->stream.height(),
            impl->stream.fps(),
            decoder_name
        );
        if (ret < 0)
        {
            fprintf(stderr, "Cannot open video decoder.\n");
            return false;
        }
    }
    
    impl->isOpened = true;
    impl->firstRead = STREAM_CAP_FIRST_READ_TIMES;
    impl->stopThread = false;
    if (impl->recvMethod == STREAM_RECV_METHOD_DROP)
    {
        std::cout << "recvMethod: drop" << std::endl;
        impl->recv_t = std::thread(&StreamCaptureHandler::recvThread, impl);
    }
    else
    {
        std::cout << "recvMethod: block" << std::endl;
    }
    return true;
}


bool StreamCapture::open(std::string url, int apiPreference)
{
    if (isOpened())
    {
        release();
    }

    if (impl_ == nullptr)
    {
        impl_ = new StreamCaptureHandler();
    }
    auto impl = static_cast<StreamCaptureHandler*>(impl_);
    impl->isOpened = false;

    int ret = impl->stream.open(url);
    if (ret < 0)
    {
        fprintf(stderr, "Cannot open stream.\n");
        return false;
    }
// #define ENABLE_RKMPP
#ifdef ENABLE_RKMPP
    impl->decoder = getVideoDecoder(CODEC_PLATFORM_RKMPP);
#else
    impl->decoder = getVideoDecoder(CODEC_PLATFORM_FFMPEG);
#endif
    ret = impl->decoder->open_codec(
        impl->stream.width(), 
        impl->stream.height(),
        impl->stream.fps(),
        impl->stream.codec_id()
    );
    if (ret < 0)
    {
        fprintf(stderr, "Cannot open video decoder.\n");
        return false;
    }
    impl->isOpened = true;
    impl->firstRead = STREAM_CAP_FIRST_READ_TIMES;
    impl->stopThread = false;
    if (impl->recvMethod == STREAM_RECV_METHOD_DROP)
    {
        std::cout << "recvMethod: drop" << std::endl;
        impl->recv_t = std::thread(&StreamCaptureHandler::recvThread, impl);
    }
    else
    {
        std::cout << "recvMethod: block" << std::endl;
    }
    return true;
}


bool StreamCapture::read(cv::Mat &frame)
{
    if (impl_ == nullptr)
    {
        return false;
    }
    auto impl = static_cast<StreamCaptureHandler*>(impl_);
    if (!impl->isOpened)
    {
        return false;
    }
    if(impl->no_decoder)
    {
        std::cerr << "decoder not init!" << std::endl;
    }

    if (impl->recvMethod == STREAM_RECV_METHOD_DROP)
    {
        return impl->getFrame(frame, impl->count_outer);
    }
    else
    {
        return impl->read(frame);
    }
}


bool StreamCapture::readStream(streamData& data)
{
    if (impl_ == nullptr)
    {
        return false;
    }
    auto impl = static_cast<StreamCaptureHandler*>(impl_);
    if (!impl->isOpened)
    {
        return false;
    }
    return impl->readStream(data);
}


bool StreamCapture::isOpened()
{
    if (impl_ == nullptr)
    {
        return false;
    }
    return static_cast<StreamCaptureHandler*>(impl_)->isOpened;
}

void StreamCapture::operator >> (cv::Mat &frame)
{
    read(frame);
}

void StreamCapture::set(int propId, double value) 
{
    if (impl_ == nullptr)
    {
        impl_ = new StreamCaptureHandler();
    }
    auto impl = static_cast<StreamCaptureHandler*>(impl_);
    if (propId == STREAM_RECV_METHOD)
    {
        impl->recvMethod = (int)value;
        // std::cout << "recvMethod: " << value << std::endl;
    }
    else if (propId == STREAM_RECV_STEP)
    {
        impl->stream.setStep((size_t)value);
    }
}

double StreamCapture::get(int propId)
{
    if (impl_ == nullptr)
    {
        return -1;
    }
    auto impl = static_cast<StreamCaptureHandler*>(impl_);
    switch (propId)
    {
    case cv::CAP_PROP_FPS:
        return impl->fps;
        break;
    case cv::CAP_PROP_FRAME_WIDTH:
        return impl->stream.width();
        break;
    case cv::CAP_PROP_FRAME_HEIGHT:
        return impl->stream.height();
        break;
    case cv::CAP_PROP_POS_FRAMES:
        return impl->stream.cur_dts();
        break;
    default:
        break;
    }
    return 0;
}

void StreamCapture::release()
{
    if (impl_ == nullptr)
    {
        return;
    }
    auto impl = static_cast<StreamCaptureHandler*>(impl_);

    if (impl->recv_t.joinable())
    {
        impl->stopThread = true;
        impl->recv_t.join();
    }

    impl->stream.close();
    if (impl->decoder != nullptr)
    {
        delete impl->decoder;
        impl->decoder = nullptr;
    }
    delete impl;
    impl_ = nullptr;
}


#endif