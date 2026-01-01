# easyvideo: 基于OpenCV和FFMPEG实现的快速视频处理工具

## 为何要做这么一个库

- OpenCV确实简单易用，小白用一个`cv::VideoCapture`就能搞定几乎一切的视频文件/视频流的读取与处理，问题在于这么直接使用经常耗费较多的CPU资源，并且也不支持硬解码。

- FFMPEG支持丰富的硬解码方式，但是高灵活性带来的是上手难度以及代码中大量的配置项，对新手不友好。

本项目在保留FFMPEG一定灵活性的同时，也保证了较好的易用性，接口与OpenCV基本一样。同时，即使是使用软解码，本项目的CPU占用率也在多种解码格式上优于OpenCV。

## 安装本项目

需要先安装好OpenCV和FFMPEG并在系统路径下能找到这两个库

为了编译demo，需要安装好yaml-cpp：`sudo apt install libyaml-cpp-dev`，或手动编译

```shell
mkdir build;cd build
cmake -DENABLE_RKMPP=OFF ..   # 如果在瑞芯微平台上使用，可以打开该选项通过mpp和rga库进行硬件加速
make -j8
```
将生成`libeasyvideo.so`和推流的demo可执行文件`pushRTSP`，将`include`和该动态库文件放到相应环境下即可使用

## 使用

### 1. 视频流捕获

接口

```cpp
// 创建视频流捕获器
BaseCapture* createCapture(
    std::string url,                 // 同cv::VideoCapture
    int apiPreference=cv::CAP_ANY,   // 同cv::VideoCapture
    int type=CAP_TYPE_JPEG,          // 视频流类型
    bool dropFrame=false             // 当为网络视频流时，若读取速度较慢是否丢弃中间未读取的图像帧，目前设置为true有bug，不建议使用
    std::string decoder_name="auto"  // 当为网络视频流时，手动选取ffmpeg解码器，rkmpp平台支持(h264/h265/vp8/vp9)_rkmpp
);

// 以下使用方法同cv::VideoCapture
virtual bool BaseCapture::open(std::string url, int apiPreference=::cv::CAP_ANY);
virtual bool BaseCapture::read(::cv::Mat &frame);
virtual void BaseCapture::release();
virtual void BaseCapture::operator >> (::cv::Mat &frame);
virtual void BaseCapture::set(int propId, double value);
virtual double BaseCapture::get(int propId);
```

示例

```cpp
#include <easyvideo/opencv/capture.h>

int main(int argc, char** argv)
{
    // 读取本地相机
    auto cap_local_camera = easyvideo::Capture::createCapture(
        "/dev/video0", cv::CAP_V4L2, 
        easyvideo::Capture::CAP_TYPE_JPEG  // 相机输出为JPEG格式
        // 该选项为CAP_TYPE_YUYV时相机输出为YUYV格式
        // 不知道相机输出为何种格式时填写CAP_TYPE_NORMAL，此时退化为cv::VideoCapture，无任何性能优化
    );

    // 读取本地视频文件，此处相当直接使用cv::VideoCapture
    auto cap_local_file = easyvideo::Capture::createCapture(
        "/path/to/your/file.mp4", cv::CAP_ANY,
        easyvideo::Capture::CAP_TYPE_NORMAL
    );

    // 读取网络视频流（目前支持rtsp）
    // 网络视频流会自动优先选择硬解码以降低CPU占用率
    // 目前支持的硬解方案：(h264/h265)_(cuvid/nvmpi/rkmpp)
    // 即：英伟达显卡/英伟达Jetson平台/瑞芯微平台硬解，需要在FFMPEG编译时打开相应选项
    auto cap_stream = easyvideo::Capture::createCapture(
        "rtsp://[url]:[port]/[stream path]", cv::CAP_ANY,
        easyvideo::Capture::CAP_TYPE_STREAM,
        false, "h264_rkmpp"
    );

    return 0;
}
```

### 2. 视频流单帧处理

当只想关注对视频单帧/每帧图像的处理，可以使用视频服务接口

接口

```cpp
easyvideo::VideoServer::VideoServer();

void easyvideo::VideoServer::setupSource(sensor::camera::Info::Device info);
void easyvideo::VideoServer::setupSource(YAML::Node config)
void easyvideo::VideoServer::setupSource(
    std::string device_name,
    std::string device_source,
    std::string device_type="auto",
    std::string device_format="auto",
    int width=-1,
    int height=-1,
    int fps=-1, 
    int reopen_times=-1,
    int reopen_delay=100,  // ms
    std::vector<int> roi={},
    std::string decoder_name="auto"
);


void easyvideo::VideoServer::setCallback(void(*func)(const cv::Mat&));

template <class T>
void easyvideo::VideoServer::setCallback(void(T::*func)(const cv::Mat&), T* obj);

void easyvideo::VideoServer::start_service();
void easyvideo::VideoServer::stop_service();

// 结构体定义
struct sensor::camera::Info::Device
{
    // name: 自己随便取名，source:即url，type: "usb/file/stream", format: "yuyv/jpeg/h264/h265/auto"
    std::string name, source, type, format;
    std::string decoder_name="auto";
    int fps, width, height;     // -1为自动
    cv::Rect crop;              // 设置后会取对应的矩形区域而不是整张图像
    int reopen_times=-1;        // 断连后重启次数，-1代表无限
    int reopen_delay=100;       // ms
    YAML::Node _config;         // 如果使用yaml文件加载则上述信息也会保存在该变量中
};
```

yaml配置文件示例
```yaml
name: my_usb_camera
source: /dev/video0
type: usb
format: jpeg
width: 1920
height: 1080
fps: 30
decoder_name: h264_rkmpp
preprocess:
  reopen:
    times: -1
    delay: 100  # ms
  crop: [0,0,0,0]  # 全0代表不进行裁剪
```

示例

```cpp
#include <easyvideo/videoServer.h>
#include <signal>

void callBack(const cv::Mat& image)
{
    // do something
    // ...
    cv::imshow("image view", image);
    int key = cv::waitKey(1);
    switch (key) {
    case 's':
        cv::imwrite("image.jpg", image);
        break
    // ...
    default:
        break;
    };
}

class ServerHandle
{
public:
    void callback(const cv::Mat& image)
    {
        // do something
    }
};

int main(int argc, char** argv)
{
    easyvideo::VideoServer server;
    server.setupSource("/path/to/your/config.yaml");

    // 以下二选一，不能同时设置，同时设置则以最后设置的为准
    // 1. 普通函数
    server.setCallback(&callBack);

    // 2. 成员函数
    ServerHandle handle;
    server.setCallBack(&ServerHandle::callback, &handle);

    signal(SIGINT, [server](int sig) {
        server.stop_service();
    });
    // 启动服务，阻塞
    server.start_service();
    return 0;
}
```

### 3. 推流

接口

```cpp
easyvideo::RTSPPusher::RTSPPusher(std::string url);

void easyvideo::RTSPPusher::start();  // 在子线程中启动推流
void easyvideo::RTSPPusher::stop();   // 停止当前子线程
void easyvideo::RTSPPusher::pushFrameData(cv::Mat &frame);  // 推流当前图像

// 打开指定编码器
int easyvideo::RTSPPusher::open_codec(int width, int height, int den, int kB=100, std::string encoder_name="");

```

使用步骤: 先定义，再打开编码器，再启动推流。

```cpp
#include <easyvideo/push.h>

int main(int argc, char** argv)
{

    int w=1920, h=1080, fps=30, kb=512; // 4Mbps
    auto pusher = new easyvideo::RTSPPusher("rtsp://localhost/stream/test");
    pusher->open_codec(w, h, fps, kb, "h264_cuvid");
    pusher->start();

    while (1)
    {
        cv::Mat mat(w, h, CV_8UC3);
        // 处理图像
        // ...

        pusher->pushFrameData(mat);
    }

    pusher->stop();
    delete pusher;
    pusher = nullptr;
    
    return 0;
}
```


