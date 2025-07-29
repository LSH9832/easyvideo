#include "easyvideo/push.h"

namespace easyvideo
{
AVFrame *RTSPPusher::CVMatToAVFrame(cv::Mat &inMat, int YUV_TYPE) {
    //得到Mat信息
    AVPixelFormat dstFormat = AV_PIX_FMT_YUV420P;
    int width = inMat.cols;
    int height = inMat.rows;
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
    cv::cvtColor(inMat, inMat, cv::COLOR_BGR2YUV_I420);

    //按YUV420格式，设置数据地址
    int frame_size = width * height;
    unsigned char *data = inMat.data;

    memcpy(frame->data[0], data, frame_size);
    memcpy(frame->data[1], data + frame_size, frame_size/4);
    memcpy(frame->data[2], data + frame_size * 5/4, frame_size/4);

    return frame;
}

int RTSPPusher::push() {

    // std::cout << "00" << std::endl;

    int ret = 0;
    cv::Mat frame;
    AVFrame *yuv;
    AVFrame *nv12;

    if (enable_hardware)
    {
        nv12 = av_frame_alloc();
        av_hwframe_get_buffer(outputVc->hw_frames_ctx, nv12, 0);
        std::cout << "format:" << nv12->format << std::endl;
    }

    long pts = 0;
    AVPacket pack;
    memset(&pack, 0, sizeof(pack));

    ret = avformat_write_header(output, NULL);

    outputConnected_ = ret == 0;
    // std::cout << ret << std::endl;
    conditionVariable2.notify_all();
    
    long max_dts = 0;
    long count = 0;
    

    while(true){
        // frame = pop_one_frame();
        // std::cout << 1 << std::endl;
        popOneFrameData(frame);
        if (frame.empty())
        {
            break;
        }
        // std::cout << 2 << std::endl;
        yuv = CVMatToAVFrame(frame, 0);

        yuv->pts = pts;
        pts += 1;

        if (enable_hardware)
        {
            // std::cout << 3 << std::endl;
            ret = av_hwframe_transfer_data(nv12, yuv, 0);
            // sws_scale(sws_ctx, yuv->data, yuv->linesize, 0, yuv->height,
            //           nv12->data, nv12->linesize);
            // std::cout << 4 << std::endl;
            if (ret < 0)
            {
                // av_frame_free(&yuv);
                continue;
            }
        }

        // std::cout << 5 << std::endl;
        AVERROR(EAGAIN);
        ret = avcodec_send_frame(outputVc, enable_hardware?nv12:yuv); // enable_hardware?nv12:

        if (ret != 0){
            // av_free(&pack);
            std::cerr << "avcodec_send_frame error:" << ret << std::endl;
            continue;
        }

        // std::cout << 6 << std::endl;
        ret = avcodec_receive_packet(outputVc, &pack);
        // std::cout << 7 << std::endl;

        if (ret != 0 || pack.size > 0) {
        }
        else {
            
            av_frame_free(&yuv);
            
            av_free(&pack);
            
            continue;
        }
        int firstFrame = 0;
        if (pack.dts < 0 || pack.pts < 0 || pack.dts > pack.pts || firstFrame) {
            firstFrame = 0;
            pack.dts = pack.pts = pack.duration = 0;
        }

        // pack.pts = av_rescale_q(pack.pts, outputVc->time_base, vs->time_base); // 显示时间
        pack.dts = av_rescale_q(pack.dts, outputVc->time_base, vs->time_base); // 解码时间
        if (pack.dts < max_dts+1)
        {
            pack.dts = max_dts + 1;
            pack.pts = pack.dts;
        }
        else
        {
            pack.pts = av_rescale_q(pack.pts, outputVc->time_base, vs->time_base); // 显示时间
        }
        pack.duration = av_rescale_q(pack.duration, outputVc->time_base, vs->time_base); // 数据时长
        max_dts = pack.dts;

        ret = av_interleaved_write_frame(output, &pack);

        if (ret < 0)
        {
            printf("发送数据包出错\n");
            // std::cout << 4 << std::endl;
            av_frame_free(&yuv);
            // std::cout << 5 << std::endl;
            // av_free(&pack);
            // std::cout << 6 << std::endl;
            // delete header_data;
            continue;
        }
        // std::cout << 7 << std::endl;
        av_frame_free(&yuv);
        // delete header_data;
        // std::cout << 8 << std::endl;
        // av_free(&pack);
        // std::cout << 9 << std::endl;
    }

    return ret;
}

int RTSPPusher::open_codec(int width, int height, int den, int kB, std::string encoder_name) {
    int ret = 0;
    avformat_network_init();
    // const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    enable_hardware = false;

    AVCodec* codec=NULL; // = avcodec_find_encoder_by_name("h264_nvenc");
    if (!encoder_name.empty())
    {
        std::cout << "try find encoder: " << encoder_name << std::endl;
        codec = avcodec_find_encoder_by_name(encoder_name.c_str());
        std::cout << "codec value: " << codec << std::endl;
    }
    else
    {
        std::cout << "use software decoder" << std::endl;
    }
// Intel QSV
    if (!codec)
    {
        if (!encoder_name.empty())
        {
            std::cerr << "Can`t find encoder '" << encoder_name 
                      << "'! Try soft encoder." << std::endl; // 找不到指定编码器
        }
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        enable_hardware = false;
        if (!codec)
        {
            throw std::logic_error("Can`t find h264 encoder!"); // 找不到264编码器
        }
    }
    else
    {
        //  codec->

    }
    // b 创建编码器上下文
    outputVc = avcodec_alloc_context3(codec);
    if (!outputVc)
    {
        throw std::logic_error("avcodec_alloc_context3 failed!"); // 创建编码器失败
    }
    // c 配置编码器参数
    fps = den;
    outputVc->flags |= AVFMT_FLAG_IGNIDX; // 全局参数
    outputVc->codec_id = codec->id;
    outputVc->codec_type = AVMEDIA_TYPE_VIDEO;
    outputVc->thread_count = 4;

    outputVc->bit_rate = kB * 1024 * 8; // 压缩后每秒视频的bit位大小为50kb
    outputVc->width = width;
    outputVc->height = height;
    outputVc->time_base = {1, den};
    outputVc->framerate = {den, 1};

    outputVc->gop_size = 30;
    outputVc->max_b_frames = 0;
    outputVc->qmax = 51;
    outputVc->qmin = 10;
    outputVc->pix_fmt = enable_hardware?AV_PIX_FMT_CUDA:AV_PIX_FMT_YUV420P;

    if (enable_hardware)
    {
        AVBufferRef* hw_device_ctx = nullptr;
        av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);  // CUDA设备
        outputVc->hw_device_ctx = av_buffer_ref(hw_device_ctx);

        sws_ctx = sws_getContext(
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
        outputVc->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);

    }
    
    av_opt_set(outputVc->priv_data, "tune", "zerolatency", 0);
    av_opt_set(outputVc->priv_data, "preset", "ultrafast", 0);


    // d 打开编码器上下文
    ret = avcodec_open2(outputVc, codec, 0);
    if (ret != 0)
    {
        std::cout << "avcodec_open2 failed!" << std::endl;
        return encoder_name.empty()?ret:open_codec(width, height, den, kB, "");
    }
    // std::cout << "avcodec_open2 success!" << std::endl;

    ret = avformat_alloc_output_context2(&output, nullptr, "rtsp", url.c_str());
    if (ret != 0)
    {
        std::cout << "avformat_alloc_output_context2 failed!" << std::endl;
        return ret;
    }

    vs = avformat_new_stream(output, outputVc->codec);
    vs->codecpar->codec_tag = 0;
    // 从编码器复制参数
    avcodec_parameters_from_context(vs->codecpar, outputVc);
    av_dump_format(output, 0, url.c_str(), 1);

//    ret = avio_open(&output->pb, url.c_str(), AVIO_FLAG_WRITE);
    return ret;
}

cv::Mat RTSPPusher::pop_one_frame() {
    while(true){
        std::unique_lock<std::mutex> lock(queue_mutex);
        conditionVariable.wait(lock,[this](){return pic_buffer.size() > 0;});
        cv::Mat tmp = pic_buffer.front().clone();
        pic_buffer.pop();
        return tmp;
    }
}

void RTSPPusher::popOneFrameData(cv::Mat &outMat) {
    while(true)
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        conditionVariable.wait(lock,[this](){return pic_buffer.size() > 0;});
        outMat = pic_buffer.front().clone();
        pic_buffer.pop();
        return;
    }
}

RTSPPusher::RTSPPusher(std::string url){
    this->url = url;
    outputVc = nullptr;
    output = nullptr;
}

void RTSPPusher::push_frame(cv::Mat &frame) {
    if(pic_buffer.size()<256){
        std::unique_lock<std::mutex> lock(queue_mutex);
        pic_buffer.push(frame);
        conditionVariable.notify_all();
    }
}

void RTSPPusher::pushFrameData(cv::Mat &frame)
{
    if(pic_buffer.size()<256){
        std::unique_lock<std::mutex> lock(queue_mutex);
        pic_buffer.push(frame);
        conditionVariable.notify_all();
    }
}

void RTSPPusher::start() {
    push_thread = std::thread(&RTSPPusher::push, this);
    push_thread.detach();
    std::unique_lock<std::mutex> lock(connect_mutex);
    std::cout << "waiting for connection to " << url << std::endl;
    conditionVariable2.wait(lock,[this](){return outputConnected_;});
    std::cout << "connection to " << url << " success." << std::endl;
}

void RTSPPusher::stop()
{
    cv::Mat emptyMat;
    pushFrameData(emptyMat);
}

}