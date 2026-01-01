#ifndef EASYVIDEO_VIDEOWRITER_CPP
#define EASYVIDEO_VIDEOWRITER_CPP

#include "easyvideo/videoWriter.h"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
    #include <libswscale/swscale.h>
}

bool av_init=false;

struct VideoWriter::Impl
{
    const AVOutputFormat* fmt;
    AVFormatContext* oc = NULL;
    AVStream* video_st;
    AVCodecContext* c = NULL;
    AVCodecParameters* cp = NULL;
};


VideoWriter::VideoWriter()
{
    if (!av_init)
    {
        avformat_network_init();
        av_init = true;
    }
    if (impl_ == nullptr)
    {
        impl_ = new Impl();
    }
}

VideoWriter::~VideoWriter()
{
    this->release();
}

bool VideoWriter::init(std::string dest, int width, int height, int fps, int kB, 
                       std::string encoder_name, int num_threads, int gop)
{
    if (impl_ == nullptr)
    {
        impl_ = new Impl();
    }
    impl_->fmt = av_guess_format(NULL, dest.c_str(), NULL);
    impl_->oc = avformat_alloc_context();
    if (!impl_->oc) {
        fprintf(stderr, "Memory error\n");
        return false;
    }

    impl_->oc->oformat = av_guess_format(NULL, dest.c_str(), NULL);;
    impl_->video_st = avformat_new_stream(impl_->oc, NULL); // 创建视频流
    if (!impl_->video_st) {
        fprintf(stderr, "Could not allocate stream\n");
        return -1;
    }

    encoder_ = new VideoEncoder(width, height, fps, kB, encoder_name, num_threads, gop);

    impl_->cp = impl_->video_st->codecpar;
    impl_->cp->codec_id = impl_->fmt->video_codec; // 使用默认视频编解码器ID，对于H.264通常是AV_CODEC_ID_H264
    impl_->cp->codec_type = AVMEDIA_TYPE_VIDEO; // 设置媒体类型为视频
    impl_->cp->bit_rate = kB * 8 * 1024; // 设置比特率，可根据需要调整
    impl_->cp->width = width; // 设置宽度，根据输入图片调整
    impl_->cp->height = height; // 设置高度，根据输入图片调整
    impl_->video_st->time_base = (AVRational){1, fps};
    // impl_->video_st->
    // impl_->cp->gop_size = gop; // 设置关键帧间隔，可根据需要调整
    // impl_->cp->pix_fmt = AV_PIX_FMT_YUV420P; // 设置像素格式，H.264通常使用YUV420P格式


    if (avio_open(&impl_->oc->pb, dest.c_str(), AVIO_FLAG_WRITE) < 0) {
        std::cerr << "无法打开输出文件: " << dest << std::endl;
        return false;
    }

    if (avformat_write_header(impl_->oc, nullptr) < 0) {
        std::cerr << "无法写入文件头" << std::endl;
        return false;
    }

    init_ = true;

    return init_;
}


bool VideoWriter::write(cv::Mat& frame)
{
    if (!init_) 
    {
        fprintf(stderr, "videowriter not init!");
        return false;
    }
    AVPacket pack;
    memset(&pack, 0, sizeof(pack));
    encoder_->encodeFrame(frame, &pack);

    pack.stream_index = impl_->video_st->index;

    if (av_interleaved_write_frame(impl_->oc, &pack) < 0) 
    {
        std::cerr << "写入帧失败" << std::endl;
        av_packet_unref(&pack);
        return false;
    }

    av_packet_unref(&pack);

    return true;
}

bool VideoWriter::is_init()
{
    return init_;
}


bool VideoWriter::release()
{
    if (!init_) return true;

    // printf("1\n");
    encoder_->release();
    // printf("2\n");
    avio_closep(&impl_->oc->pb);
    // printf("3\n");
    avformat_free_context(impl_->oc);
    // printf("4\n");
    impl_->oc = NULL;
    delete impl_;
    // printf("5\n");
    impl_ = nullptr;
    init_ = false;
    return true;
}


#endif
