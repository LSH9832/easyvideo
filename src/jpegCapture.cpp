#ifndef JPEGUTILS_CPP
#define JPEGUTILS_CPP

#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <string.h>
#include <jerror.h>

#include <easyvideo/opencv/jpegCapture.h>

// 自定义的内存源管理器结构体
struct my_memory_src_mgr {
    struct jpeg_source_mgr pub; // 公共的jpeg_source_mgr结构体，必须放在第一个位置
    unsigned char *data;        // 指向JPEG数据的指针
    size_t data_length;         // JPEG数据的长度
};

typedef struct my_memory_src_mgr *my_memory_src_ptr;

// 初始化内存源
static void init_source(j_decompress_ptr cinfo) {
    my_memory_src_ptr src = (my_memory_src_ptr)cinfo->src;
    src->pub.next_input_byte = src->data;
    src->pub.bytes_in_buffer = src->data_length;
}

// 填充输入缓冲区（在这个例子中，我们不需要重新填充，因为所有数据都在内存中）
static boolean fill_input_buffer(j_decompress_ptr cinfo) {
    // 我们已经把所有数据都加载到内存中了，所以不应该调用这个函数
    // 如果调用了，说明数据不完整或出错了
    ERREXIT(cinfo, JERR_INPUT_EMPTY);
    return FALSE; // 表示没有更多数据可以读取
}

// 跳过输入数据（简单地调整内部指针）
static void skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
    my_memory_src_ptr src = (my_memory_src_ptr)cinfo->src;

    if (num_bytes > (long)src->pub.bytes_in_buffer)
        ERREXIT(cinfo, JERR_INPUT_EOF);

    src->pub.next_input_byte += (size_t)num_bytes;
    src->pub.bytes_in_buffer -= (size_t)num_bytes;
}

// 终止输入（在这个例子中，我们不需要做特别的清理工作）
static void term_source(j_decompress_ptr cinfo) {
    // 不需要特别的清理，因为所有数据都在静态或动态分配的内存中
}

// 设置内存源管理器
static void set_memory_source(j_decompress_ptr cinfo, unsigned char *data, size_t data_length) {
    my_memory_src_ptr src;

    if (cinfo->src == NULL) { // 第一次调用，需要分配和初始化源管理器
        cinfo->src = (struct jpeg_source_mgr *)
            (*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT,
                                      sizeof(struct my_memory_src_mgr));
        src = (my_memory_src_ptr)cinfo->src;
        src->pub.init_source = init_source;
        src->pub.fill_input_buffer = fill_input_buffer;
        src->pub.skip_input_data = skip_input_data;
        src->pub.resync_to_restart = jpeg_resync_to_restart; // 使用默认的重同步函数
        src->pub.term_source = term_source;
    } else {
        src = (my_memory_src_ptr)cinfo->src;
    }

    src->data = data;
    src->data_length = data_length;
}

void easyvideo::jpg2rgb(unsigned char *data, unsigned char *buffer_, size_t& data_length) 
{
    // 创建JPEG解压对象并设置错误处理
    // std::cout << 1 << ":" << data_length << std::endl;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    // std::cout << data_length << std::endl;

    // 设置内存源
    set_memory_source(&cinfo, data, data_length);

    // std::cout << 3 << std::endl;

    // 读取JPEG头并准备解压
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    // std::cout << 4 << std::endl;

    // 获取解压后的图像信息
    int row_stride = cinfo.output_width * cinfo.output_components;
    unsigned char *buffer = (unsigned char *)malloc(row_stride * cinfo.output_height);
    if (!buffer) {
        fprintf(stderr, "can't allocate buffer\n");
        jpeg_destroy_decompress(&cinfo);
        return;
    }


    // std::cout << row_stride * cinfo.output_height << std::endl;

    // 读取解压后的图像数据
    while (cinfo.output_scanline < cinfo.output_height) {
        unsigned char *buffer_array[1];
        buffer_array[0] = &buffer[cinfo.output_scanline * row_stride];
        jpeg_read_scanlines(&cinfo, buffer_array, 1);
    }

    // std::cout << 6 << std::endl;

    // 清理并关闭
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    for(int i=0;i<row_stride * cinfo.output_height / 3;i++) 
    {
        buffer_[3*i] = buffer[3*i+2];
        buffer_[3*i+1] = buffer[3*i+1];
        buffer_[3*i+2] = buffer[3*i];
    }

    // std::cout << row_stride << ", " << cinfo.output_height << std::endl;
    // std::cout << (int)buffer[123] << std::endl;

    // std::cout << 6.5 << std::endl;
    free(buffer);

    // std::cout << 7 << std::endl;

}


easyvideo::MJPG2BGRCapture::MJPG2BGRCapture()
{

}

easyvideo::MJPG2BGRCapture::MJPG2BGRCapture(std::string source, int apiPreference)
{
    open(source, apiPreference);
}

bool easyvideo::MJPG2BGRCapture::open(std::string source, int apiPreference)
{
    bool success = cap.open(source, apiPreference);
    if(success)
    {  
        sz.height = cap.get(::cv::CAP_PROP_FRAME_HEIGHT);
        sz.width = cap.get(::cv::CAP_PROP_FRAME_WIDTH);
        setSize(sz);
        cap.set(::cv::CAP_PROP_FOURCC, ::cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
        cap.set(::cv::CAP_PROP_CONVERT_RGB, 0);
    }
    return success;
}

bool easyvideo::MJPG2BGRCapture::isOpened()
{
    return cap.isOpened();
}

void easyvideo::MJPG2BGRCapture::release()
{
    cap.release();
}

void easyvideo::MJPG2BGRCapture::set(int propId, double value)
{
    if (!isOpened()) return;
    cap.set(propId, value);
    if (propId == cv::CAP_PROP_FRAME_HEIGHT || 
        propId == cv::CAP_PROP_FRAME_WIDTH) 
    {
        sz.height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
        sz.width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    }
    std::cout << sz << std::endl;
    setSize(sz);
}

double easyvideo::MJPG2BGRCapture::get(int propId)
{
    if (!isOpened()) return -1;
    return cap.get(propId);
}

bool easyvideo::MJPG2BGRCapture::read(::cv::Mat& image)
{
    bool success = cap.read(recvImage);
    if (success)
    {
        if (first)
        {
            outImage = cv::Mat(sz, CV_8UC3);
            first = false;
        }
        size_t datasize = 0;
        for (;&recvImage.data[datasize] != &recvImage.dataend[0];datasize++);
        if (datasize != last_size) jpg2rgb(recvImage.data, outImage.data, datasize);
        // else std::cout << "skip" << std::endl;
        image = outImage;
        last_size = datasize;
    }
    return success;
}


void easyvideo::MJPG2BGRCapture::operator>>(::cv::Mat& image)
{
    read(image);
}

void easyvideo::MJPG2BGRCapture::setSize(cv::Size sz_)
{
    sz = sz_;
    area = sz.area();
}


#endif  // JPEGUTILS_CPP