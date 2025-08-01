#ifndef VIDEO_DECODER_HW_CPP
#define VIDEO_DECODER_HW_CPP

#include "easyvideo/videoDecoder.h"

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
#include <libavutil/frame.h>
}

// ----------------------------------------------------------------------------------------

enum HW_TYPE
{
    HW_TYPE_NONE,
    HW_TYPE_CUDA,
    HW_TYPE_MPP,
    HW_TYPE_OTHER
};

static enum AVPixelFormat hw_pix_fmt;

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) 
    {
        if (*p == hw_pix_fmt)
        return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}


struct VideoDecoder::Impl
{
    cv::Mat AVFrameToCVMat(AVFrame *inFrameV);

    int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type);

    AVBufferRef *hw_device_ctx = NULL;
    
    AVCodecContext *codec_ctx_=nullptr;
    bool enable_hwaccel_=false;
    HW_TYPE hw_type=HW_TYPE_NONE;

};


int VideoDecoder::Impl::hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
{
    int err = 0;

    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
                                    NULL, NULL, 0)) < 0) {
        fprintf(stderr, "Failed to create specified HW device.\n");
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    return err;
}

cv::Mat VideoDecoder::Impl::AVFrameToCVMat(AVFrame *frame)
{
    if (AV_PIX_FMT_YUV420P == frame->format)  // YUV420 to BGR
    {
        uint8_t* buffer = (uint8_t*)av_malloc(codec_ctx_->width * codec_ctx_->height * 3 / 2);
        // std::cout << frame->width << "," << frame->height << std::endl;
        int frame_size = frame->width * frame->height;

        memcpy(buffer, frame->data[0], frame_size);
        memcpy(buffer + frame_size, frame->data[1], frame_size/4);
        memcpy(buffer + frame_size * 5/4, frame->data[2], frame_size/4);

        cv::Mat img(codec_ctx_->height * 3 / 2, codec_ctx_->width, CV_8UC1, buffer);

        cv::cvtColor(img, img, cv::COLOR_YUV2BGR_I420);
        av_freep(&buffer);
        return img;
    }
    else if (AV_PIX_FMT_NV12 == frame->format) // NV12 to BGR
    {
        uint8_t* buffer = (uint8_t*)av_malloc(codec_ctx_->width * codec_ctx_->height * 3 / 2);
        // std::cout << frame->width << "," << frame->height << std::endl;
        int frame_size = frame->width * frame->height;

        memcpy(buffer, frame->data[0], frame_size);
        memcpy(buffer + frame_size, frame->data[1], frame_size / 2);

        cv::Mat img(codec_ctx_->height * 3 / 2, codec_ctx_->width, CV_8UC1, buffer);

        cv::cvtColor(img, img, cv::COLOR_YUV2BGR_NV12);
        av_freep(&buffer);
        return img;
    }
    else
    {
        std::cerr << "unsupported format:" << frame->format << std::endl;
        return cv::Mat();
    }
}


// ------------------------------------------------------------------------------------------------

int VideoDecoder::decodeFrame(uint8_t *inData, int inLen, cv::Mat &outMatV, bool readAll)
{

    AVPacket pack;
    memset(&pack, 0, sizeof(pack));
    // pack.data = inData;
    // pack.size = inLen;
    av_packet_from_data(&pack, inData, inLen);

    pack.flags |= AV_PKT_FLAG_TRUSTED;

    int ret = 0;
    ret = avcodec_send_packet(impl_->codec_ctx_, &pack);
    if (ret != 0){
        std::cerr << "avcodec_send_packet error:" << ret << std::endl;
        return ret;
    }

    AVFrame *frame = av_frame_alloc();
    ret = avcodec_receive_frame(impl_->codec_ctx_, frame);
    if (ret != 0){
        if (ret != -11)
        {
            std::cerr << "avcodec_receive_frame error:" << ret << std::endl;
        }
        av_frame_free(&frame);
        return ret;
    }

    if (readAll)
    {
        AVFrame *frame2 = av_frame_alloc();
        ret = 0;
        while (ret == 0)
        {
            ret = avcodec_receive_frame(impl_->codec_ctx_, frame2);
        }
        av_frame_free(&frame2);
    }


    if (impl_->enable_hwaccel_)
    {
        AVFrame *sw_frame = av_frame_alloc();
        AVFrame *tmp_frame = NULL;
        // uint8_t *buffer = NULL;
        // int size;

        if (frame->format == hw_pix_fmt)
        {
            if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0)
            {
                std::cerr << "Failed to transfer the data to system memory." << std::endl;
                av_frame_free(&sw_frame);
                av_frame_free(&frame);
                return ret;
            }
            tmp_frame = sw_frame;
        }
        else
        {
            tmp_frame = frame;
        }

        // std::cout << "hw2sw_frame_format:" << tmp_frame->format << std::endl;

        outMatV = impl_->AVFrameToCVMat(tmp_frame);
        av_frame_free(&sw_frame);
        av_frame_free(&frame);
    }
    else
    {
        // std::cout << "sw_frame_format:" << frame->format << std::endl;
        outMatV = impl_->AVFrameToCVMat(frame);
        av_frame_free(&frame);
    }
    return 0;
}

VideoDecoder::VideoDecoder(int width, int height, int fps, int decode_id)
{
    width_=width;
    height_=height;
    fps_=fps;
    decode_id_=decode_id;
    int ret = open_codec(width, height, fps, decode_id);
    if (ret < 0)
    {
        std::cerr << "failed to open decoder." << std::endl;
        exit(-1);
    }
}



int VideoDecoder::open_codec(int width, int height, int fps, int decode_id)
{
    decode_id_=decode_id;
    width_=width;
    height_=height;
    fps_=fps;
    if (impl_ == nullptr)
    {
        impl_ = new Impl();
    }

    const AVCodec *codec;
    int hwtype = 0; // AV_HWDEVICE_TYPE_NONE;
    bool useNVMPI = false;   // jetson
    // 27: h264, 173: h265
    if (decode_id_ == 27)
    {
        codec = avcodec_find_decoder_by_name("h264_rkmpp");
        if (NULL != codec)
        {
            hwtype = 12; // AV_HWDEVICE_TYPE_RKMPP;
            impl_->enable_hwaccel_ = true;
            impl_->hw_type = HW_TYPE_MPP;
            std::cout << "use h264_rkmpp decoder." << std::endl;
            goto end_find_decoder;
        }

        codec = avcodec_find_decoder_by_name("h264_cuvid");
        if (NULL != codec)
        {
            hwtype = 2; // AV_HWDEVICE_TYPE_CUDA;
            impl_->enable_hwaccel_ = true;
            impl_->hw_type = HW_TYPE_CUDA;
            std::cout << "use h264_cuvid decoder." << std::endl;
            goto end_find_decoder;
        }

        codec = avcodec_find_decoder_by_name("h264_nvmpi");
        if (NULL != codec)
        {
            hwtype = 2; // AV_HWDEVICE_TYPE_CUDA;
            impl_->enable_hwaccel_ = true;
            impl_->hw_type = HW_TYPE_CUDA;
            std::cout << "use h264_nvmpi decoder." << std::endl;
            useNVMPI = true;
            goto end_find_decoder;
        }

        std::cout << "[W] can not find hardware decoder, use default software decoder." << std::endl;
        codec = avcodec_find_decoder((AVCodecID)decode_id_);
    }
    else if (decode_id_ == 173)
    {
        codec = avcodec_find_decoder_by_name("hevc_rkmpp");
        if (NULL != codec)
        {
            hwtype = 12; // AV_HWDEVICE_TYPE_RKMPP;
            impl_->enable_hwaccel_ = true;
            impl_->hw_type = HW_TYPE_MPP;
            std::cout << "use hevc_rkmpp decoder." << std::endl;
            goto end_find_decoder;
        }

        codec = avcodec_find_decoder_by_name("hevc_cuvid");
        if (NULL != codec)
        {
            hwtype = 2; // AV_HWDEVICE_TYPE_CUDA;
            impl_->enable_hwaccel_ = true;
            impl_->hw_type = HW_TYPE_CUDA;
            std::cout << "use hevc_cuvid decoder." << std::endl;
            goto end_find_decoder;
        }

        codec = avcodec_find_decoder_by_name("hevc_nvmpi");
        if (NULL != codec)
        {
            hwtype = 2; // AV_HWDEVICE_TYPE_CUDA;
            impl_->enable_hwaccel_ = true;
            impl_->hw_type = HW_TYPE_CUDA;
            std::cout << "use hevc_nvmpi decoder." << std::endl;
            useNVMPI = true;
            goto end_find_decoder;
        }

        std::cout << "[W] can not find hardware decoder, use default software decoder." << std::endl;
        codec = avcodec_find_decoder((AVCodecID)decode_id_);
    }
    else
    {
        std::cout << "[W] use default software decoder." << std::endl;
        codec = avcodec_find_decoder((AVCodecID)decode_id_);
    }

end_find_decoder:

    if (!codec) {
        std::cout << "Codec not found" << std::endl;
        return -1;
    }

    if(impl_->enable_hwaccel_)
    {
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
            if (!config) {
                std::cout << i << std::endl;
                fprintf(stderr, "Decoder %s does not support device type %s.\n", 
                        codec->name, av_hwdevice_get_type_name((AVHWDeviceType)hwtype));
                return -1;
            }

            // std::cout << av_hwdevice_get_type_name(config->device_type) << std::endl;
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX 
                && config->device_type == hwtype
            ) 
            {
                hw_pix_fmt = config->pix_fmt;
                break;
            }
        }
    }
    impl_->codec_ctx_ = avcodec_alloc_context3(codec);
    if (!impl_->codec_ctx_) {
        std::cout << "Could not allocate video codec context" << std::endl;
        return -1;
    }
    impl_->codec_ctx_->width = width;
    impl_->codec_ctx_->height = height;
    impl_->codec_ctx_->time_base = (AVRational){1, fps};
    impl_->codec_ctx_->framerate = (AVRational){fps, 1};
    impl_->codec_ctx_->pkt_timebase = (AVRational){1, fps};
    


    // impl_->codec_ctx_->flags       |= AVFMT_FLAG_IGNIDX; // 全局参数
    // impl_->codec_ctx_->flags       |= AV_CODEC_FLAG2_LOCAL_HEADER;
    // impl_->codec_ctx_->flags       |= AV_CODEC_FLAG_LOW_DELAY;

    if(!impl_->enable_hwaccel_ || useNVMPI)
    {
        impl_->codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
        av_opt_set(impl_->codec_ctx_->priv_data, "tune", "zerolatency", 0);
        av_opt_set(impl_->codec_ctx_->priv_data, "preset", "fast", 0);
    }
    else
    {
        impl_->codec_ctx_->pix_fmt = hw_pix_fmt;
        impl_->codec_ctx_->get_format = get_hw_format;
        av_opt_set_int(impl_->codec_ctx_, "refcounted_frames", 1, 0);
        if (impl_->hw_decoder_init(impl_->codec_ctx_, (AVHWDeviceType)hwtype) < 0)
        {
            std::cout << "Failed to initialize hw decoder, try sw decoder" << std::endl;
            // return -1;
            av_opt_set(impl_->codec_ctx_->priv_data, "tune", "zerolatency", 0);
            av_opt_set(impl_->codec_ctx_->priv_data, "preset", "fast", 0);
        }
        
    }
    
    // std::cout << "open decoder" << std::endl;
    if (avcodec_open2(impl_->codec_ctx_, codec, NULL) < 0) {
        std::cout << "Could not open codec" << std::endl;
        return -1;
    }
    // std::cout << "open decoder success" << std::endl;
    return 0;

}


#endif  // VIDEO_DECODER_HW_CPP
