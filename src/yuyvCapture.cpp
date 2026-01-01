#ifndef EASYVIDEO_YUYVCAPTURE_CPP
#define EASYVIDEO_YUYVCAPTURE_CPP

#include <easyvideo/opencv/yuyvCapture.h>

#ifdef ENABLE_RKMPP
#include "platform/rockchip/rockchip/mpp_buffer.h"
#include "platform/rockchip/rockchip/mpp_err.h"
#include "platform/rockchip/rockchip/mpp_frame.h"
#include "platform/rockchip/rockchip/mpp_packet.h"
#include "platform/rockchip/rockchip/mpp_task.h"
#include "platform/rockchip/rockchip/rk_mpi.h"
#include "platform/rockchip/rga/RgaApi.h"
#include "platform/rockchip/rga/im2d.hpp"
#include "platform/rockchip/rga/rga.h"


static inline bool yuyv2bgr_mpp(uchar* yuyv, int w, int h, uchar* bgr) {
    rga_buffer_t src_img, dst_img;
    rga_buffer_handle_t src_handle, dst_handle;

    auto src_format = RK_FORMAT_YUYV_422;
    auto dst_format = RK_FORMAT_BGR_888;

    src_handle = importbuffer_virtualaddr(yuyv, w * h * 2);
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

#define YUYV2BGR_RV 1.403
#define YUYV2BGR_GU -0.344
#define YUYV2BGR_GV -0.714
#define YUYV2BGR_BU 1.77
#define YUYV2BGR_BIAS 127.5

void yuyv2bgr(uchar* yuyv, uchar* bgr, size_t size)
{
    int y1=0, u=0, y2=0, v=0;
    for (int i=0; i<size/2; ++i)
    {
        y1 = yuyv[4 * i + 0];
        u = yuyv[4 * i + 1];
        y2 = yuyv[4 * i + 2];
        v = yuyv[4 * i + 3];
        
        bgr[6 * i + 0] = MAX(0, MIN((int)(YUYV2BGR_BU * (u - YUYV2BGR_BIAS) + y1), 255));  // b1
        bgr[6 * i + 1] = MAX(0, MIN(255, (int)(YUYV2BGR_GU * (u - YUYV2BGR_BIAS) + YUYV2BGR_GV * (v - YUYV2BGR_BIAS) + y1)));  // g1
        bgr[6 * i + 2] = MAX(0, MIN(255, (int)(YUYV2BGR_RV * (v - YUYV2BGR_BIAS) + y1)));  // r1

        bgr[6 * i + 3] = MAX(0, MIN((int)(YUYV2BGR_BU * (u - YUYV2BGR_BIAS) + y2), 255));  // b2
        bgr[6 * i + 4] = MAX(0, MIN(255, (int)(YUYV2BGR_GU * (u - YUYV2BGR_BIAS) + YUYV2BGR_GV * (v - YUYV2BGR_BIAS) + y2)));  // g2
        bgr[6 * i + 5] = MAX(0, MIN(255, (int)(YUYV2BGR_RV * (v - YUYV2BGR_BIAS) + y2)));  // r2
    }
}


easyvideo::YUYV2BGRCapture::YUYV2BGRCapture()
{

}

easyvideo::YUYV2BGRCapture::YUYV2BGRCapture(std::string source, int apiPreference)
{
    cap.open(source, apiPreference);
    cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V'));
// #ifdef ENABLE_RKMPP
//     c_RkRgaInit();
// #endif
}

bool easyvideo::YUYV2BGRCapture::open(std::string source, int apiPreference)
{
    bool success = cap.open(source, apiPreference);
    if(success)
    {
        cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
        cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    }
    return success;
}

bool easyvideo::YUYV2BGRCapture::isOpened()
{
    return cap.isOpened();
}

void easyvideo::YUYV2BGRCapture::release()
{
    cap.release();
}

void easyvideo::YUYV2BGRCapture::set(int propId, double value)
{
    if (propId >= 0)
        cap.set(propId, value);
    else
    {
        if (propId == -200)
        {
            step = value;
        }
    }
}

double easyvideo::YUYV2BGRCapture::get(int propId)
{
    return cap.get(propId);
}

bool easyvideo::YUYV2BGRCapture::read(cv::Mat& image)
{
    bool success = false;
    // std::cout << "step: " << step << std::endl;
    for(int i=0;i<step;++i) success = cap.read(recvImage);
    if (success)
    {
        if (first)
        {
            sz = recvImage.size();
            area = sz.area();
            outImage = cv::Mat(sz, CV_8UC3);
            first = false;
        }
#ifdef ENABLE_RKMPP
        // std::cout << "use mpp rga yuyv decoder" << std::endl;
        yuyv2bgr_mpp(recvImage.data, sz.width, sz.height, outImage.data);
#else
        yuyv2bgr(recvImage.data, outImage.data, area);
#endif
        image = outImage;
    }
    return success;
}

void easyvideo::YUYV2BGRCapture::operator>>(cv::Mat& image)
{
    read(image);
}

#endif