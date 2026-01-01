#ifndef STREAM_CPP
#define STREAM_CPP

#include "easyvideo/opencv/stream.h"
extern "C" {
#include <libavcodec/avcodec.h>
// #include <libavcodec/>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
}

struct Stream::Impl
{
    bool isOpened = false;
    int video_stream, ret;

    AVFormatContext *input_ctx = nullptr;
    AVStream *video = nullptr;
    AVPacket* packet = nullptr;

    int step = 1;

    int width=-1, height=-1, fps=-1, codec_id=-1;
};



int Stream::open(std::string url)
{
    if (impl == nullptr)
    {
        impl = new Impl();
    }
    impl->isOpened = false;

    AVDictionary* options = nullptr;
    // av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "rtsp_transport", "udp", 0);
    av_dict_set_int(&options, "udp_buffer_size", 1024 * 1024, 0);
    av_dict_set(&options, "stimeout", "2000000", 0);

    // int ret = avformat_open_input(&fmt_ctx, rtsp_url, nullptr, &options);

    /* open the input file */
    if (avformat_open_input(&impl->input_ctx, url.c_str(), NULL, &options) != 0) {
        fprintf(stderr, "Cannot open input source '%s'\n", url.c_str());
        return -3;
    }

    if (avformat_find_stream_info(impl->input_ctx, NULL) < 0) {
        fprintf(stderr, "Cannot find input stream information.\n");
        return -2;
    }

    /* find the video stream information */
    int ret = av_find_best_stream(impl->input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Cannot find a video stream in the input file\n");
        return -1;
    }
    impl->video_stream = ret;
    impl->video = impl->input_ctx->streams[impl->video_stream];
    // INFO << "video stream codec id: " << video->codecpar->codec_id << ENDL;
    
    // std::cout << "fps: " << impl->video->time_base.den << "," << impl->video->avg_frame_rate.num << std::endl;
    impl->fps = impl->video->avg_frame_rate.num;
    impl->width = impl->video->codecpar->width;
    impl->height = impl->video->codecpar->height;
    impl->codec_id = impl->video->codecpar->codec_id;
    impl->isOpened = true;
    return 0;
}

void Stream::setStep(size_t step)
{
    impl->step = step;
}

int Stream::width()
{
    if (impl == nullptr)
    {
        return -1;
    }
    return impl->width;
}

int Stream::height()
{
    if (impl == nullptr)
    {
        return -1;
    }
    return impl->height;
}

int Stream::fps()
{
    if (impl == nullptr)
    {
        return -1;
    }
    return impl->fps;
}

int Stream::codec_id()
{
    if (impl == nullptr)
    {
        return -1;
    }
    return impl->codec_id;
}

int Stream::cur_dts()
{
    if (impl == nullptr)
    {
        return -1;
    }
    // return impl->video->cur_dts;
    return 0;
}

void* Stream::read(int& size)
{
    if (impl == nullptr)
    {
        return nullptr;
    }
    if (!impl->isOpened)
    {
        return nullptr;
    }

    if (impl->packet != nullptr)
    {
        av_packet_unref(impl->packet);
        av_packet_free(&impl->packet);
        impl->packet = nullptr;
    }
    impl->packet = av_packet_alloc();
    
    int ret = 0;
    for (int i=0;i<impl->step;++i)
    {
        ret = av_read_frame(impl->input_ctx, impl->packet);
    }
    
    if (ret < 0)
    {
        size = 0;
        return nullptr;
    }
    size = impl->packet->size;
    return impl->packet->data;
}

void* Stream::read(int& size, int64_t& pts, int64_t& dts, bool& isKeyFrame)
{
    if (impl == nullptr)
    {
        return nullptr;
    }
    if (!impl->isOpened)
    {
        return nullptr;
    }

    if (impl->packet != nullptr)
    {
        av_packet_unref(impl->packet);
        av_packet_free(&impl->packet);
        impl->packet = nullptr;
    }
    impl->packet = av_packet_alloc();
    
    int ret = av_read_frame(impl->input_ctx, impl->packet);
    if (ret < 0)
    {
        size = 0;
        return nullptr;
    }
    size = impl->packet->size;
    pts = impl->packet->pts;
    dts = impl->packet->dts;
    isKeyFrame = (impl->packet->flags & AV_PKT_FLAG_KEY) != 0;
    return impl->packet->data;
}

void Stream::close()
{
    avformat_close_input(&impl->input_ctx);
    if (impl->packet != nullptr)
    {
        av_packet_unref(impl->packet);
        av_packet_free(&impl->packet);
        impl->packet = nullptr;
    }
}


#endif