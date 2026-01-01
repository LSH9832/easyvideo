#include "easyvideo/videoDecoder.h"


#include "rockchip/mpp_buffer.h"
#include "rockchip/mpp_err.h"
#include "rockchip/mpp_frame.h"
#include "rockchip/mpp_packet.h"
#include "rockchip/mpp_task.h"
#include "rockchip/rk_mpi.h"
#include "rga/RgaApi.h"
#include "rga/im2d.hpp"
#include "rga/rga.h"
// #include "rockchip/"
#include <unordered_map>
#include <omp.h>

static inline void YUV420sp2BGR(uchar* yuv420sp, int w, int h, uchar* bgr) {
    int frameSize = w * h;
    int yIndex = 0;
    int uvIndex = frameSize;

    #pragma omp parallel for
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            uint8_t Y = yuv420sp[yIndex++];
            uint8_t U = 0, V = 0;
            if (i % 2 == 0 && j % 2 == 0) {
                V = yuv420sp[uvIndex++];
                U = yuv420sp[uvIndex++];
            }
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



#ifdef SYLIXOS
static inline bool YUV420sp2BGR_Mpp(MppFrame frame, int w, int h, uint8_t* bgr) {
    MppBuffer src_buf = mpp_frame_get_buffer(frame);
    if (!src_buf) return false;
    rga_info_t src = {0}, dst = {0};
    src.fd = mpp_buffer_get_fd(src_buf); 
    src.virAddr = src_buf;
    src.format = RK_FORMAT_YCbCr_420_SP;

    RK_U32 h_stride = mpp_frame_get_hor_stride(frame);
	RK_U32 v_stride = mpp_frame_get_ver_stride(frame);

    rga_set_rect(&src.rect, 0, 0, w, h, h_stride, v_stride, RK_FORMAT_YCbCr_420_SP);

    dst.fd = -1;
    dst.virAddr = bgr;
    dst.format = RK_FORMAT_BGR_888;
    rga_set_rect(&dst.rect, 0, 0, w, h, w, h, RK_FORMAT_BGR_888);

    c_RkRgaBlit(&src, &dst, nullptr);
    return true;
}
#else
static inline bool YUV420sp2BGR_Mpp(uint8_t* yuv, int w, int h, uint8_t* bgr) {
    rga_buffer_t src_img, dst_img;
    rga_buffer_handle_t src_handle, dst_handle;

    auto src_format = RK_FORMAT_YUYV_420;
    auto dst_format = RK_FORMAT_BGR_888;

    src_handle = importbuffer_virtualaddr(yuv, w * h * 3 / 2);
    dst_handle = importbuffer_virtualaddr(bgr, w * h * 3);

    src_img = wrapbuffer_handle(src_handle, w, h, src_format);
    dst_img = wrapbuffer_handle(dst_handle, w, h, dst_format);

    int ret = imcvtcolor(src_img, dst_img, src_format, dst_format, IM_YUV_TO_RGB_BT709_LIMIT);

    if (ret != IM_STATUS_SUCCESS)
    {
        std::cout << __FILE__ << ":" << __LINE__ << "error code: " << ret << std::endl;
        return false;
    }
    else
    {
        // std::cout << "convert success" << std::endl;
        return true;
    }
}
#endif


static inline void YUV420SP2Mat(MppFrame frame, cv::Mat& rgbImg) 
{
	RK_U32 width = 0;
	RK_U32 height = 0;

	width = mpp_frame_get_width(frame);
	height = mpp_frame_get_height(frame);

    rgbImg.create(height, width, CV_8UC3);
#ifdef SYLIXOS
    if (YUV420sp2BGR_Mpp(frame, width, height, rgbImg.data))
    {
        return;
    }
    std::cout << "failed to use rga conversion yuv420sp to bgr" << std::endl;
#endif
    
    RK_U32 h_stride = 0;
	RK_U32 v_stride = 0;

	MppBuffer buffer = NULL;
	RK_U8 *base = NULL;
    h_stride = mpp_frame_get_hor_stride(frame);
	v_stride = mpp_frame_get_ver_stride(frame);
	buffer = mpp_frame_get_buffer(frame);

	base = (RK_U8 *)mpp_buffer_get_ptr(buffer);
	RK_U32 buf_size = mpp_frame_get_buf_size(frame);
	size_t base_length = mpp_buffer_get_size(buffer);
	// mpp_log("base_length = %d\n",base_length);

	RK_U32 i;
	RK_U8 *base_y = base;
	RK_U8 *base_c = base + h_stride * v_stride;

	cv::Mat yuvImg;
	yuvImg.create(height * 3 / 2, width, CV_8UC1);

	//转为YUV420p格式
	int idx = 0;
	for (i = 0; i < height; i++, base_y += h_stride) {
		//        fwrite(base_y, 1, width, fp);
		memcpy(yuvImg.data + idx, base_y, width);
		idx += width;
	}
	for (i = 0; i < height / 2; i++, base_c += h_stride) {
		//        fwrite(base_c, 1, width, fp);
		memcpy(yuvImg.data + idx, base_c, width);
		idx += width;
	}
	//这里的转码需要转为RGB 3通道， RGBA四通道则不能检测成功
    // cv::COLOR_YUV420sp2RGB
	// cv::cvtColor(yuvImg, rgbImg, cv::COLOR_YUV420sp2RGB);
#ifndef SYLIXOS
    if (YUV420sp2BGR_Mpp(yuvImg.data, width, height, rgbImg.data))
    {
        return;
    }
    std::cout << "failed to use rga conversion yuv420sp to bgr" << std::endl;
#endif
	YUV420sp2BGR(yuvImg.data, width, height, rgbImg.data);
}

// -------------------------------------------------------


class RKMPPVideoDecoder: public VideoDecoder {
public:
    RKMPPVideoDecoder() {}

    int open_codec(int width, int height, int fps, int decode_id=CODEC_H264);

    int open_codec(int width, int height, int fps, std::string decoder_name="");

    int decodeFrame(uint8_t *inData, int inLen, cv::Mat &outMatV, bool readAll=false);

private:
    MppCtx ctx;
    MppApi *mpi = nullptr;
    MPP_RET err;

    bool init_ = false;

    int frame_count = 0;

    const std::unordered_map<int, MppCodingType> map_ = {
        {CODEC_AUTO, MPP_VIDEO_CodingAutoDetect},
        {CODEC_H264, MPP_VIDEO_CodingAVC},
        {CODEC_H265, MPP_VIDEO_CodingHEVC}
    };

};
    

int RKMPPVideoDecoder::open_codec(int width, int height, int fps, int decode_id)
{
    if (init_)
    {
        std::cerr << "decoder already init!" << std::endl;
        return MPP_OK;
    }
    if (map_.find(decode_id) == map_.end())
    {
        std::cerr << "mpp decoder not support code with id " << decode_id << std::endl;
        return -1;
    }

    width_ = width;
    height_ = height;
    MpiCmd mpi_cmd      = MPP_CMD_BASE;
    RK_U32 need_split   = 1;
    MppParam param = NULL;

    err = mpp_create(&ctx, &mpi);
    if (err != MPP_OK)
    {
        std::cerr << "failed to create MPP context with code " << err << std::endl;
        return err;
    }

    mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;
    param = &need_split;
    err = mpi->control(ctx, mpi_cmd, param);
    if (MPP_OK != err) {
        std::cerr << "mpi->control failed: MPP_DEC_SET_PARSER_SPLIT_MODE\n";
        return err;
    }

    mpi_cmd = MPP_SET_INPUT_TIMEOUT;
    param = &need_split;
    err = mpi->control(ctx, mpi_cmd, param);
    if (MPP_OK != err) {
        std::cerr << "mpi->control failed: MPP_SET_INPUT_BLOCK\n";
        return err;
    }

    err = mpp_init(ctx, MPP_CTX_DEC, map_.at(decode_id));
    if (err != MPP_OK)
    {
        std::cerr << "failed to init mpp decoder." << std::endl;
        mpp_destroy(ctx);
        return err;
    }

    init_ = true;
    fps_ = fps;
    frame_count = 0;

    return err;
}

int RKMPPVideoDecoder::open_codec(int width, int height, int fps, std::string decoder_name)
{
    if (init_)
    {
        std::cerr << "decoder already init!" << std::endl;
        return MPP_OK;
    }
    MppCodingType codec_type = MPP_VIDEO_CodingAutoDetect;

    if (decoder_name == "h264_rkmpp")
    {
        codec_type = MPP_VIDEO_CodingAVC;
    }
    else if (decoder_name == "hevc_rkmpp")
    {
        codec_type = MPP_VIDEO_CodingHEVC;
    }
    else if (decoder_name == "vp8_rkmpp")
    {
        codec_type = MPP_VIDEO_CodingVP8;
    }
    else if (decoder_name == "vp9_rkmpp")
    {
        codec_type = MPP_VIDEO_CodingVP9;
    }

    width_ = width;
    height_ = height;

    MpiCmd mpi_cmd      = MPP_CMD_BASE;
    RK_U32 need_split   = 1;
    MppParam param = NULL;

    err = mpp_create(&ctx, &mpi);
    if (err != MPP_OK)
    {
        std::cerr << "failed to create MPP context with code " << err << std::endl;
        return err;
    }

    mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;
    param = &need_split;
    err = mpi->control(ctx, mpi_cmd, param);
    if (MPP_OK != err) {
        std::cerr << "mpi->control failed: MPP_DEC_SET_PARSER_SPLIT_MODE\n";
        return err;
    }

    mpi_cmd = MPP_SET_INPUT_TIMEOUT;
    param = &need_split;
    err = mpi->control(ctx, mpi_cmd, param);
    if (MPP_OK != err) {
        std::cerr << "mpi->control failed: MPP_SET_INPUT_BLOCK\n";
        return err;
    }

    err = mpp_init(ctx, MPP_CTX_DEC, codec_type);
    if (err != MPP_OK)
    {
        std::cerr << "failed to init mpp decoder." << std::endl;
        mpp_destroy(ctx);
        return err;
    }

    init_ = true;
    fps_ = fps;
    frame_count = 0;

    return err;
}

int RKMPPVideoDecoder::decodeFrame(uint8_t *inData, int inLen, cv::Mat &outMatV, bool readAll)
{
    std::cout << __FILE__ << ":" << __LINE__ << " -> using rkmpp decoder" << std::endl;
    RK_U32 pkt_done = 0;
    RK_U32 pkt_eos  = 0;
    RK_U32 err_info = 0;
    err = MPP_OK;
    
    MppPacket packet = NULL;
    MppFrame  frame  = NULL;
    MppBufferGroup frm_grp = NULL;
    MppBufferGroup pkt_grp = NULL;
    size_t read_size = 0;
    size_t packet_size = inLen;

    std::cout << __LINE__ << std::endl;
    err = mpp_packet_init(&packet, inData, inLen);
    std::cout << __LINE__ << std::endl;
    mpp_packet_set_pts(packet, 1e9 / fps_ * (++frame_count));
    std::cout << __LINE__ << std::endl;

    do {
        RK_S32 get_frm = 0;
        RK_U32 frm_eos = 0;
        int times = 5;

        try_again:
        std::cout << __LINE__ << std::endl;
        err = mpi->decode_get_frame(ctx, &frame);
        std::cout << __LINE__ << std::endl;
        if (MPP_ERR_TIMEOUT == err) {
            if (times > 0) {
                times--;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                goto try_again;
            }
            std::cerr << "decode_get_frame failed too much time\n";
            break;
        }
        std::cout << __LINE__ << std::endl;
        if (MPP_OK != err) {
            fprintf(stderr, "decode_get_frame failed ret %d\n", err);
            break;
        }
        std::cout << __LINE__ << std::endl;

        if (frame) {
            if (mpp_frame_get_info_change(frame)) {
                RK_U32 width = mpp_frame_get_width(frame);
                RK_U32 height = mpp_frame_get_height(frame);
                RK_U32 hor_stride = mpp_frame_get_hor_stride(frame);
                RK_U32 ver_stride = mpp_frame_get_ver_stride(frame);
                RK_U32 buf_size = mpp_frame_get_buf_size(frame);

                width_ = width;
                height_ = height;

                printf("decode_get_frame get info changed found\n");
                printf("decoder require buffer w:h [%d:%d] stride [%d:%d] buf_size %d",
                        width, height, hor_stride, ver_stride, buf_size);

                err = mpp_buffer_group_get_internal(&frm_grp, MPP_BUFFER_TYPE_ION);
                if (err) {
                    fprintf(stderr, "get mpp buffer group  failed ret %d\n", err);
                    break;
                }
                mpi->control(ctx, MPP_DEC_SET_EXT_BUF_GROUP, frm_grp);
                mpi->control(ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
            } else {
                err_info = mpp_frame_get_errinfo(frame) | mpp_frame_get_discard(frame);
                if (err_info) {
                    fprintf(stderr, "decoder_get_frame get err info:%d discard:%d.\n",
                            mpp_frame_get_errinfo(frame), mpp_frame_get_discard(frame));
                }
                frame_count++;
                
                if (!err_info)
                {
                    YUV420SP2Mat(frame, outMatV);
                    if (!readAll) return MPP_OK;
                }
                else if (!readAll)
                {
                    return err_info;
                }
            }
            frm_eos = mpp_frame_get_eos(frame);
            mpp_frame_deinit(&frame);

            frame = NULL;
            get_frm = 1;
        }
        else
        {
            if (times > 0) {
                times--;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                goto try_again;
            }
            break;
        }
    } while (1);

    return err;
}
