#include <iostream>

#define SOURCE "/mnt/d/test.mp4"
#define DESTINATION "rtsp://127.0.0.1:8554/stream/test"
#define ENCODER ""
#define FPS 30
#define KBPS 300  // Kb/s
#define HEIGHT 720
#define WIDTH 1280

#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

#include "pylike/argparse.h"
#include "easyvideo/opencv/capture.h"
#include "easyvideo/push.h"


argparse::ArgumentParser get_args(int argc, char** argv)
{
    argparse::ArgumentParser parser("rtsp pusher parser", argc, argv);
    parser.add_argument({"-s", "--source"}, SOURCE, "source video");
    parser.add_argument({"-d", "--dest"}, DESTINATION, "rtsp address");
    parser.add_argument({"-kb", "--kbyterate"}, KBPS, "kbyte rate");
    parser.add_argument({"-f", "--fps"}, FPS, "default fps if fps can not be read from video source");
    parser.add_argument({"-c", "--compressed"}, STORE_TRUE, "true: jpeg camera; false: yuyv camera");
    parser.add_argument({"-e", "--encoder"}, ENCODER, "encoder name");
    parser.add_argument({"--show"}, STORE_TRUE, "show push images");
    parser.parse_args();
    return parser;
}

int main(int argc, char** argv)
{
    // utils::logging::setLogLevel(utils::logging::LOG_LEVEL_SILENT);
    auto args = get_args(argc, argv);
    std::cout << args.showParams(false) << std::endl;
    pystring path = args["source"];
    pystring dest = args["dest"];
    pystring encoder_name = args["encoder"];
    bool isCamera = path.startswith("/dev/video") || path.isdigit();
    bool compressed = args["compressed"];
    bool show = args["show"];
    int default_fps = args["fps"];
    int kbyterate = args["kbyterate"];
    if (isCamera && path.isdigit())
    {
        path = pystring("/dev/video") + path;
    }

    // std::cout << "path: " << path << std::endl;

    auto cap = easyvideo::Capture::createCapture(
        path, isCamera?cv::CAP_V4L2:cv::CAP_ANY, 
        isCamera?(compressed?
                  easyvideo::Capture::CAP_TYPE_JPEG:
                  easyvideo::Capture::CAP_TYPE_YUYV):
        easyvideo::Capture::CAP_TYPE_NORMAL
    );
    
    if(!cap->isOpened()){
        std::cout<<"无法打开摄像头！"<< std::endl;
        return -1;
    }

    easyvideo::RTSPPusher *pushUtils = new easyvideo::RTSPPusher(dest);
    int fps = cap->get(cv::CAP_PROP_FPS);
    fps = fps>0?fps:default_fps;
    // if (isCamera)
    // {
    //     cap->set(cv::CAP_PROP_FRAME_WIDTH, WIDTH);
    //     cap->set(cv::CAP_PROP_FRAME_HEIGHT, HEIGHT);
    //     cap->set(cv::CAP_PROP_FPS, fps);
    // }
    pushUtils->open_codec(
        cap->get(cv::CAP_PROP_FRAME_WIDTH),
        cap->get(cv::CAP_PROP_FRAME_HEIGHT),
        fps,
        kbyterate, encoder_name
    );
    pushUtils->start();

    // namedWindow("test", WINDOW_AUTOSIZE);

    
    int count = 1;
    int ms2wait = 1000 / fps;
    std::cout << "fps: " << fps << ", delay(ms): " << ms2wait << std::endl;
    
    double t0 = pytime::time();
    std::cout << "start push" << std::endl;
    while (true)
    {
        cv::Mat frame;
        if (!cap->read(frame))
        {
            std::cout << "failed to read frame" << std::endl;
            break;
        }
        // std::cout << "read frame success" << std::endl;

        // flip(frame,frame,1);
        
        pushUtils->pushFrameData(frame);

        int restTime2wait = ms2wait - 1000 * (pytime::time() - t0);
        t0 = pytime::time();
        if (restTime2wait > 0)
        {
            // std::cout << restTime2wait << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(restTime2wait));
            // pytime::sleep((double)restTime2wait/1000.);
        }
        
        if(show)
        {
            cv::imshow("test", frame);
            if (cv::waitKey(1) == 27)
            {
                break;
            }
        }
    }
    pushUtils->stop();
    cap->release();
    cv::destroyAllWindows();
    delete cap;
    cap = nullptr;
    delete pushUtils;
    pushUtils = nullptr;
    return 0;
}
