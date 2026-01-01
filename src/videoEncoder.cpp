#ifndef FFMPEG_ENCODER_CPP
#define FFMPEG_ENCODER_CPP

#include "easyvideo/videoEncoder.h"
// #include "pylike/str.h"

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


struct VideoEncoder::Impl
{
    // vars
    bool enable_hardware=false;
    bool isInit = false;
    bool useMPP = false;

    AVCodecContext *outputVc=nullptr;
    SwsContext* sws_ctx=nullptr;

    int codec_id=-1;
    std::string codec_name;

    cv::Mat padFrame;
    
    // function
    AVFrame *CVMatToAVFrame(cv::Mat &inMatV, int YUV_TYPE);
    
};

void VideoEncoder::release()
{
    if (impl_ == nullptr) return;
    avcodec_free_context(&impl_->outputVc);
    delete impl_;
    impl_ = nullptr;
}

AVFrame *VideoEncoder::Impl::CVMatToAVFrame(cv::Mat &inMat, int YUV_TYPE)
{
    if (!isInit)
    {
        std::cerr << "encoder not init!" << std::endl;
        return nullptr;
    }

    int width, height;
    if (useMPP)
    {
        inMat.copyTo(padFrame(cv::Rect(0, 0, inMat.cols, inMat.rows)));
        width = padFrame.cols;
        height = padFrame.rows;
    }
    else
    {
        width = inMat.cols;
        height = inMat.rows;
    }

    //得到Mat信息
    AVPixelFormat dstFormat = AV_PIX_FMT_YUV420P;
    
    //创建AVFrame填充参数 注：调用者释放该frame
    AVFrame *frame = av_frame_alloc();
    frame->width = width;
    frame->height = height;
    frame->format = dstFormat;

    //初始化AVFrame内部空间
    int ret = av_frame_get_buffer(frame, 64);
    if (ret < 0)
    {
        return nullptr;
    }
    ret = av_frame_make_writable(frame);
    if (ret < 0)
    {
        return nullptr;
    }

    //转换颜色空间为YUV420
    cv::cvtColor(useMPP?padFrame:inMat, inMat, cv::COLOR_BGR2YUV_I420);

    //按YUV420格式，设置数据地址
    int frame_size = width * height;
    unsigned char *data = inMat.data;

    memcpy(frame->data[0], data, frame_size);
    memcpy(frame->data[1], data + frame_size, frame_size/4);
    memcpy(frame->data[2], data + frame_size * 5/4, frame_size/4);

    return frame;
}

int VideoEncoder::encodeFrame(cv::Mat &frame, void* packet)
{
    if(!impl_->isInit)
    {
        std::cerr << "encoder not init!" << std::endl;
        return -1;
    }
    AVFrame *yuv = impl_->CVMatToAVFrame(frame, 0);

    AVPacket* pack = (AVPacket*)packet;

    int ret = avcodec_send_frame(impl_->outputVc, yuv);
    if (ret != 0){
        std::cerr << "avcodec_send_frame error:" << ret << std::endl;
        return ret;
    }

    // std::cout << 222 << std::endl;
    ret = avcodec_receive_packet(impl_->outputVc, pack);
    if (ret != 0) {
        if (ret != -11)
        {
            std::cerr << "avcodec_receive_packet error:" << ret << std::endl;
        }
        return ret;
    }

    // av_packet_rescale_ts(pack, impl_->outputVc->time_base, impl_->outputVc->time_base);

    av_frame_free(&yuv);
    return ret;

}


int VideoEncoder::encodeFrame(cv::Mat &frame, uint8_t *outData, int &outLen)
{
    if(!impl_->isInit)
    {
        std::cerr << "encoder not init!" << std::endl;
        return -1;
    }
    AVFrame *yuv = impl_->CVMatToAVFrame(frame, 0);
    AVPacket pack;
    memset(&pack, 0, sizeof(pack));

    // std::cout << 111 << std::endl;
    int ret = avcodec_send_frame(impl_->outputVc, yuv);
    if (ret != 0){
        std::cerr << "avcodec_send_frame error:" << ret << std::endl;
        return ret;
    }

    // std::cout << 222 << std::endl;
    ret = avcodec_receive_packet(impl_->outputVc, &pack);
    if (ret != 0) {
        if (ret != -11)
        {
            std::cerr << "avcodec_receive_packet error:" << ret << std::endl;
        }
        return ret;
    }

    // std::cout << 333 << std::endl;
    outLen = pack.size;

    std::cout << "pack flags: " << pack.flags << std::endl;
    std::cout << "pack pts: " << pack.pts << std::endl;
    std::cout << "pack dts: " << pack.dts << std::endl;
    std::cout << "pack stream_index: " << pack.stream_index << std::endl;
    std::cout << "pack dts: " << pack.dts << std::endl;
    std::cout << "pack dts: " << pack.duration << std::endl;
    


    // std::cout << "pack size:" << pack.size << std::endl;
    memcpy(outData, pack.data, pack.size);
    av_packet_unref(&pack);
    av_frame_free(&yuv);
    return ret;
}


int VideoEncoder::open_codec(int width, int height, int den, int kB, int encode_id, int num_threads, int gop)
{
    if (impl_ == nullptr)
    {
        impl_ = new Impl();
    }
    if (impl_->isInit)
    {
        std::cerr << "encoder already init!" << std::endl;
        return -1;
    }
    
    int ret = 0;
    impl_->enable_hardware = false;
     // 
    encode_id_ = encode_id;
    // std::cout << 1 << std::endl;
    const AVCodec* codec = avcodec_find_encoder((AVCodecID)encode_id);

    if (!codec)
    {
        std::cerr << "Can`t find encoder: " << encode_id << std::endl; // 找不到264编码器
        return -1;
    }
    

    // b 创建编码器上下文
    impl_->outputVc = avcodec_alloc_context3(codec);
    if (!impl_->outputVc)
    {
        throw std::logic_error("avcodec_alloc_context3 failed!"); // 创建编码器失败
    }
    // c 配置编码器参数
    std::cout << "codec id: " << codec->id << ", " << impl_->outputVc->codec_id << std::endl;
    // impl_->outputVc->flags |= AVFMT_FLAG_IGNIDX; // 全局参数
    impl_->outputVc->codec_id = codec->id;
    // impl_->outputVc->codec_type = AVMEDIA_TYPE_VIDEO;
    // impl_->outputVc->thread_count = num_threads; 

    impl_->outputVc->bit_rate = kB * 1024 * 8; // 压缩后每秒视频的bit位大小为50kb
    impl_->outputVc->width = width;
    impl_->outputVc->height = height;
    impl_->outputVc->time_base = {1, den};
    impl_->outputVc->framerate = {den, 1};

    impl_->outputVc->gop_size = MAX(0, gop);
    impl_->outputVc->max_b_frames = 0;
    // impl_->outputVc->qmax = 51;
    // impl_->outputVc->qmin = 10;
    impl_->outputVc->pix_fmt = impl_->enable_hardware?AV_PIX_FMT_CUDA:AV_PIX_FMT_YUV420P;

    if (impl_->enable_hardware)
    {
        AVBufferRef* hw_device_ctx = nullptr;
        av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);  // CUDA设备
        impl_->outputVc->hw_device_ctx = av_buffer_ref(hw_device_ctx);

        impl_->sws_ctx = sws_getContext(
            width, height, AV_PIX_FMT_YUV420P,
            width, height, AV_PIX_FMT_CUDA,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        AVBufferRef *hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx);  
        AVHWFramesContext *frames_ctx = (AVHWFramesContext*)hw_frames_ctx->data;  

        // 配置帧参数  
        frames_ctx->format = AV_PIX_FMT_CUDA;        // 硬件像素格式（如CUDA为AV_PIX_FMT_CUDA）‌:ml-citation{ref="1,2" data="citationList"}  
        frames_ctx->sw_format = AV_PIX_FMT_YUV420P;     // 软件像素格式（CPU可读格式）‌:ml-citation{ref="2" data="citationList"}  
        frames_ctx->width = width;                    // 分辨率  
        frames_ctx->height = height;  
        frames_ctx->initial_pool_size = 20;           // 预分配帧池大小‌:ml-citation{ref="2" data="citationList"}  

        // 初始化帧上下文  
        av_hwframe_ctx_init(hw_frames_ctx);
        impl_->outputVc->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);

    }
    
    // av_opt_set(impl_->outputVc->priv_data, "tune", "zerolatency", 0);
    // av_opt_set(impl_->outputVc->priv_data, "preset", "fast", 0);


    // d 打开编码器上下文
    std::cout << 1 << std::endl;
    ret = avcodec_open2(impl_->outputVc, codec, 0);
    std::cout << 2 << std::endl;
    if (ret < 0)
    {
        std::cerr << "[E] " << __FILE__ << ":" << __LINE__ << ":<" << __FUNCTION__ << "> - avcodec_open2 failed!" << std::endl;
        return -1;
    }
    std::cout << "avcodec_open2 success!" << std::endl;
    impl_->isInit = true;
    return ret;
}


int VideoEncoder::open_codec(int width, int height, int den, int kB, std::string encoder_name, int num_threads, int gop)
{
    if (impl_ == nullptr)
    {
        impl_ = new Impl();
    }
    if (impl_->isInit)
    {
        std::cerr << "encoder already init!" << std::endl;
        return -1;
    }
    
    int ret = 0;
    impl_->enable_hardware = false;
     
    
    
    
    // std::cout << 1 << std::endl;
    const AVCodec* codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    // std::cout << 2 << std::endl;
    if (!codec)
    {
        std::cerr << "Can`t find " << encoder_name << " encoder!" << std::endl; // 找不到编码器
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec)
        {
            std::cerr << "Can`t find H.264 encoder!" << std::endl; // 找不到264编码器
            return -1;
        }
    }
    else
    {
        impl_->useMPP = stringEndswith(encoder_name, "_rkmpp");
        if (impl_->useMPP)
        {
            auto encSz = encodeSize();
            impl_->padFrame = cv::Mat(encSz, CV_8UC3, cv::Scalar(114,114,114));
        }
    }

    // b 创建编码器上下文
    impl_->outputVc = avcodec_alloc_context3(codec);
    if (!impl_->outputVc)
    {
        throw std::logic_error("avcodec_alloc_context3 failed!"); // 创建编码器失败
    }
    // c 配置编码器参数
    std::cout << "codec id: " << codec->id << ", " << impl_->outputVc->codec_id << std::endl;
    encode_id_ = codec->id;
    impl_->codec_name = encoder_name;
    impl_->outputVc->flags       |= AVFMT_FLAG_IGNIDX; // 全局参数
    impl_->outputVc->codec_id     = codec->id;
    impl_->outputVc->codec_type   = AVMEDIA_TYPE_VIDEO;
    impl_->outputVc->thread_count = num_threads; 

    impl_->outputVc->bit_rate     = kB * 1024 * 8; // 压缩后每秒视频的bit位大小为50kb
    impl_->outputVc->width        = encodeSize().width;
    impl_->outputVc->height       = encodeSize().height;
    impl_->outputVc->time_base    = {1, den};
    impl_->outputVc->framerate    = {den, 1};

    impl_->outputVc->gop_size = MAX(0, gop);
    impl_->outputVc->max_b_frames = 0;
    impl_->outputVc->qmax = 51;
    impl_->outputVc->qmin = 10;
    impl_->outputVc->pix_fmt = impl_->enable_hardware?AV_PIX_FMT_CUDA:AV_PIX_FMT_YUV420P;

    if (impl_->enable_hardware)
    {
        AVBufferRef* hw_device_ctx = nullptr;
        av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);  // CUDA设备
        impl_->outputVc->hw_device_ctx = av_buffer_ref(hw_device_ctx);

        impl_->sws_ctx = sws_getContext(
            width, height, AV_PIX_FMT_YUV420P,
            width, height, AV_PIX_FMT_CUDA,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        AVBufferRef *hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx);  
        AVHWFramesContext *frames_ctx = (AVHWFramesContext*)hw_frames_ctx->data;  

        // 配置帧参数  
        frames_ctx->format = AV_PIX_FMT_CUDA;        // 硬件像素格式（如CUDA为AV_PIX_FMT_CUDA）‌:ml-citation{ref="1,2" data="citationList"}  
        frames_ctx->sw_format = AV_PIX_FMT_YUV420P;     // 软件像素格式（CPU可读格式）‌:ml-citation{ref="2" data="citationList"}  
        frames_ctx->width = width;                    // 分辨率  
        frames_ctx->height = height;  
        frames_ctx->initial_pool_size = 20;           // 预分配帧池大小‌:ml-citation{ref="2" data="citationList"}  

        // 初始化帧上下文  
        av_hwframe_ctx_init(hw_frames_ctx);
        impl_->outputVc->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);

    }
    
    av_opt_set(impl_->outputVc->priv_data, "tune", "zerolatency", 0);
    av_opt_set(impl_->outputVc->priv_data, "preset", "fast", 0);


    // d 打开编码器上下文
    std::cout << 1 << std::endl;
    ret = avcodec_open2(impl_->outputVc, codec, 0);
    std::cout << 2 << std::endl;
    if (ret < 0)
    {
        std::cerr << "[E] " << __FILE__ << ":" << __LINE__ << ":<" << __FUNCTION__ << "> - avcodec_open2 failed!" << std::endl;
        return -1;
    }
    std::cout << "avcodec_open2 success!" << std::endl;
    impl_->isInit = true;
    return ret;
}



cv::Size VideoEncoder::encodeSize(int stride)
{
#ifndef MPP_ALIGN
#define MPP_ALIGN(x, a)         (((x)+(a)-1)&~((a)-1))
#endif
    if (!impl_->useMPP) return cv::Size(width_, height_);
    return cv::Size(MPP_ALIGN(width_, stride), MPP_ALIGN(height_, stride));
}

VideoEncoder::VideoEncoder(int width, int height, int fps, int kB, int encode_id, int num_threads, int gop){
    width_ = width;
    height_ = height;
    fps_ = fps;
    kB_ = kB;
    encode_id_= encode_id;
    int ret = open_codec(width_, height_, fps_, kB_, encode_id, num_threads, gop);
    if (ret < 0)
    {
        exit(-1);
    }
}


VideoEncoder::VideoEncoder(int width, int height, int fps, int kB, std::string encoder_name, int num_threads, int gop){
    width_ = width;
    height_ = height;
    fps_ = fps;
    kB_ = kB;
    int ret = open_codec(width_, height_, fps_, kB_, encoder_name, num_threads, gop);
    if (ret < 0)
    {
        exit(-1);
    }
}

int VideoEncoder::resetByteRate(int kb)
{
    kB_ = kb;
    if (impl_ != nullptr)
    {
        std::string encoder_name = impl_->codec_name;
        int num_t = impl_->outputVc->thread_count;
        int gop = impl_->outputVc->gop_size;
        delete impl_;
        impl_ = nullptr;
        int ret = 0;
        if (encoder_name.empty())
        {
            ret = open_codec(width_, height_, fps_, kB_, encode_id_, num_t, gop);
        }
        else 
        {
            ret = open_codec(width_, height_, fps_, kB_, encoder_name, num_t, gop);
        }
        if (ret < 0)
        {
            exit(-1);
        }
    }
    return 0;
}

int VideoEncoder::getBitRate()
{
    if(impl_ != nullptr)
    {
        if (impl_->isInit)
            return impl_->outputVc->bit_rate;
    }
    return kB_ * 1024 * 8;
}

// void i420ToNv12(char *i420, char *nv12, int width, int height) {
//     int ySize = width * height;
//     int totalSize = width * height * 3 / 2;
//     int i420UIndex = ySize;
//     int i420VIndex = ySize * 5 / 4;
    
//     //复制y
//     memcpy(nv12, i420, ySize);
    
//     //复制uv
//     for (int uvIndex = ySize; uvIndex < totalSize; uvIndex += 2) 
//     {
//         *(nv12 + uvIndex) = *(i420 + i420UIndex++);
//         *(nv12 + uvIndex + 1) = *(i420 + i420VIndex++);
//     }
// }

// int ffmpeg_encode_h264(AVFrame *frame) {
//     //  = NULL;
//     AVCodecContext *codec_ctx = NULL;
//     //硬编码
//     const AVCodec *codec = avcodec_find_encoder_by_name("h264_nvenc");
//     //codec = avcodec_find_encoder_by_name("hevc_nvenc");
//     if (!codec) {
//         std::cerr << "avcodec_find_encoder_by_name failed" << std::endl;
//         return -1;
//     }
//     codec_ctx = avcodec_alloc_context3(codec);
//     if (!codec_ctx) {
//         std::cerr << "avcodec_alloc_context3 failed" << std::endl;
//         return -2;
//     }

//     AVRational rate;
//     rate.num = 1;
//     rate.den = 25;
//     codec_ctx->time_base = rate;
//     // codec_ctx->thread_count = 1;
//     // codec_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
//     //codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
//     codec_ctx->pix_fmt = AV_PIX_FMT_NV12;
//     codec_ctx->height = frame->height;
//     codec_ctx->width = frame->width;
    
//     int err = avcodec_open2(codec_ctx, codec, NULL);
//     if (err < 0) {
//         av_log(NULL, AV_LOG_INFO, "avcodec_open2 failed:%d", err);
//         return -3;
//     }

//     AVPacket pkt;
//     av_init_packet(&pkt);
//     pkt.data = NULL;
//     pkt.size = 0;
    
//     avcodec_send_frame(codec_ctx, frame);
//     avcodec_receive_packet(codec_ctx, &pkt);
//     printf("pkt.size: %d\n", pkt.size);

//     return 0;
// }


#endif // FFMPEG_ENCODER_CPP