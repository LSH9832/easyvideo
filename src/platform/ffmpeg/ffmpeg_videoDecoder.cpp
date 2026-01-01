#ifndef VIDEO_FFMPEGDECODER_CPP
#define VIDEO_FFMPEGDECODER_CPP

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

static enum AVPixelFormat hw_pix_fmt;

enum HW_TYPE
{
    HW_TYPE_NONE,
    HW_TYPE_CUDA,
    HW_TYPE_MPP,
    HW_TYPE_OTHER
};

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


// ---------------------------------------------------------------------------------------

class FFMPEGVideoDecoder: public VideoDecoder 
{
public:
    FFMPEGVideoDecoder() {}

    virtual int open_codec(int width, int height, int fps, int decode_id=CODEC_H264);
    virtual int open_codec(int width, int height, int fps, std::string decoder_name="");
    virtual int decodeFrame(uint8_t *inData, int inLen, cv::Mat &outMatV, bool readAll=false);
private:

    struct Impl;
    Impl *impl_=nullptr;
};


struct FFMPEGVideoDecoder::Impl
{
    cv::Mat AVFrameToCVMat(AVFrame *inFrameV);

    int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type);

    AVBufferRef *hw_device_ctx = NULL;
    
    AVCodecContext *codec_ctx_=nullptr;
    bool enable_hwaccel_=false;
    HW_TYPE hw_type=HW_TYPE_NONE;

};


int FFMPEGVideoDecoder::Impl::hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
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


static inline void YUV420P2BGR(uint8_t* yuv420p, int w, int h, uint8_t* bgr) {
    int frameSize = w * h;
    int yIndex = 0;
    int uIndex = frameSize;
    int vIndex = frameSize + (frameSize / 4);

    #pragma omp parallel for
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            uint8_t Y = yuv420p[yIndex++];
            uint8_t U = yuv420p[uIndex + (i / 2) * (w / 2) + (j / 2)];
            uint8_t V = yuv420p[vIndex + (i / 2) * (w / 2) + (j / 2)];
            // ITU-R BT.601 conversion
            int R = 1.164 * (Y - 16) + 1.596 * (V - 128);
            int G = 1.164 * (Y - 16) - 0.813 * (V - 128) - 0.391 * (U - 128);
            int B = 1.164 * (Y - 16) + 2.018 * (U - 128);
            // Clamp to [0, 255]
            R = R < 0 ? 0 : (R > 255 ? 255 : R);
            G = G < 0 ? 0 : (G > 255 ? 255 : G);
            B = B < 0 ? 0 : (B > 255 ? 255 : B);
            // Store BGR
            bgr[(i * w + j) * 3 + 0] = B;
            bgr[(i * w + j) * 3 + 1] = G;
            bgr[(i * w + j) * 3 + 2] = R;
        }
    }
}

static inline void NV12_2_BGR(uint8_t* nv12, int w, int h, uint8_t* bgr) {
    int frameSize = w * h;
    int yIndex = 0;
    int uvIndex = frameSize;

    #pragma omp parallel for
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            uint8_t Y = nv12[yIndex++];
            uint8_t U = nv12[uvIndex + (i / 2) * w + (j / 2) * 2];
            uint8_t V = nv12[uvIndex + (i / 2) * w + (j / 2) * 2 + 1];
            // ITU-R BT.601 conversion
            int R = 1.164 * (Y - 16) + 1.596 * (V - 128);
            int G = 1.164 * (Y - 16) - 0.813 * (V - 128) - 0.391 * (U - 128);
            int B = 1.164 * (Y - 16) + 2.018 * (U - 128);
            // Clamp to [0, 255]
            R = R < 0 ? 0 : (R > 255 ? 255 : R);
            G = G < 0 ? 0 : (G > 255 ? 255 : G);
            B = B < 0 ? 0 : (B > 255 ? 255 : B);
            // Store BGR
            bgr[(i * w + j) * 3 + 0] = B;
            bgr[(i * w + j) * 3 + 1] = G;
            bgr[(i * w + j) * 3 + 2] = R;
        }
    }
}

cv::Mat FFMPEGVideoDecoder::Impl::AVFrameToCVMat(AVFrame *frame)
{
    // std::cout << frame->format << std::endl;
    if (AV_PIX_FMT_YUV420P == frame->format || AV_PIX_FMT_YUVJ420P == frame->format)  // YUV420 to BGR
    {
        uint8_t* buffer = (uint8_t*)av_malloc(codec_ctx_->width * codec_ctx_->height * 3 / 2);
        // std::cout << frame->width << "," << frame->height << std::endl;
        int frame_size = frame->width * frame->height;

        memcpy(buffer, frame->data[0], frame_size);
        memcpy(buffer + frame_size, frame->data[1], frame_size/4);
        memcpy(buffer + frame_size * 5/4, frame->data[2], frame_size/4);

        cv::Mat img(codec_ctx_->height * 3 / 2, codec_ctx_->width, CV_8UC1, buffer);

        cv::Mat rgbImg(cv::Size(codec_ctx_->width, codec_ctx_->height), CV_8UC3);
        YUV420P2BGR(img.data, codec_ctx_->width, codec_ctx_->height, rgbImg.data);

        // cv::cvtColor(img, img, cv::COLOR_YUV2BGR_I420);
        av_freep(&buffer);
        return rgbImg;
    }
    else if (AV_PIX_FMT_NV12 == frame->format) // NV12 to BGR
    {
        uint8_t* buffer = (uint8_t*)av_malloc(codec_ctx_->width * codec_ctx_->height * 3 / 2);
        // std::cout << frame->width << "," << frame->height << std::endl;
        int frame_size = frame->width * frame->height;

        memcpy(buffer, frame->data[0], frame_size);
        memcpy(buffer + frame_size, frame->data[1], frame_size / 2);

        cv::Mat img(codec_ctx_->height * 3 / 2, codec_ctx_->width, CV_8UC1, buffer);
        cv::Mat rgbImg(cv::Size(codec_ctx_->width, codec_ctx_->height), CV_8UC3);
        // cv::cvtColor(img, img, cv::COLOR_YUV2BGR_NV12);
        NV12_2_BGR(img.data, codec_ctx_->width, codec_ctx_->height, rgbImg.data);
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

int FFMPEGVideoDecoder::decodeFrame(uint8_t *inData, int inLen, cv::Mat &outMatV, bool readAll)
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



int FFMPEGVideoDecoder::open_codec(int width, int height, int fps, int decode_id)
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
        goto use_soft_decoder;
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
        goto use_soft_decoder;
    }
    else
    {
        std::cout << "[W] use default software decoder." << std::endl;
        goto use_soft_decoder;
    }

use_soft_decoder:
    codec = avcodec_find_decoder((AVCodecID)decode_id_);
    impl_->enable_hwaccel_ = false;
    // codec = avcodec_find_decoder((AVCodecID)decode_id_);
    // codec = avcodec_find_decoder((AVCodecID)decode_id_);


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
                // AV_PIX_FMT_NV12;
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
            avcodec_free_context(&impl_->codec_ctx_);
            goto use_soft_decoder;
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



bool stringStartswith(std::string& str_, const std::string& prefix) {
    size_t str_len = str_.length();
    size_t prefix_len = prefix.length();
    if (prefix_len > str_len) return false;
    return str_.find(prefix) == 0;
}

bool stringEndswith(std::string& str_, const std::string& suffix) {
    size_t str_len = str_.length();
    size_t suffix_len = suffix.length();
    if (suffix_len > str_len) return false;
    return (str_.find(suffix, str_len - suffix_len) == (str_len - suffix_len));
}

int FFMPEGVideoDecoder::open_codec(int width, int height, int fps, std::string decoder_name)
{
    // decode_id_=decode_id;
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

    codec = avcodec_find_decoder_by_name(decoder_name.c_str());
    if (NULL != codec)
    {
        decode_id_ = codec->id;
        impl_->enable_hwaccel_ = true;
        impl_->hw_type = HW_TYPE_CUDA;
        std::cout << "use " << decoder_name << " decoder." << std::endl;
        useNVMPI = stringStartswith(decoder_name, "nvmpi_") || stringEndswith(decoder_name, "_nvmpi");
        goto end_find_decoder;
    }

    std::cout << "[W] can not find hardware decoder, use default software decoder." << std::endl;
    codec = avcodec_find_decoder((AVCodecID)decode_id_);

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
                fprintf(stderr, 
                        "Decoder %s does not support current device type.\n", 
                        codec->name);
                return -1;
            }

            // std::cout << av_hwdevice_get_type_name(config->device_type) << std::endl;
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) 
            {
                hwtype = config->device_type;
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

#endif  // VIDEO_DECODER_CPP
