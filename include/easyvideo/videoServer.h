#pragma once
#ifndef __VIDEO_SERVER_H__
#define __VIDEO_SERVER_H__

// #include "./opencv/baseCapture.h"
#include "./opencv/capture.h"
#include <yaml-cpp/yaml.h>
#include <boost/bind.hpp>
#include <boost/is_placeholder.hpp>
#include <boost/function.hpp>
#include <thread>
#include "./utils/sensor/info.h"


namespace easyvideo
{
    class VideoServer
    {
    public:
        typedef void (*Callback_f)(const cv::Mat& frame);
        using Callback_clsf=boost::function<void (const cv::Mat&)>;

        VideoServer() {}

        void setupSource(sensor::camera::Info::Device info)
        {
            setupSource(
                info.name,
                info.source,
                info.type,
                info.format,
                info.width,
                info.height,
                info.fps,
                info.reopen_times,
                info.reopen_delay,
                {info.crop.x, info.crop.y, info.crop.width, info.crop.height},
                info.decoder_name
            );
        }

        void setupSource(YAML::Node config)
        {
            std::string decoder_name = "auto";
            if (config["decoder_name"].IsDefined())
            {
                decoder_name = config["decoder_name"].as<std::string>();
            }
            setupSource(
                config["name"].as<std::string>(),
                config["source"].as<std::string>(),
                config["type"].as<std::string>(),
                config["format"].as<std::string>(),
                config["width"].as<int>(),
                config["height"].as<int>(),
                config["fps"].as<int>(),
                config["preprocess"]["reopen"]["times"].as<int>(),
                config["preprocess"]["reopen"]["delay"].as<int>(),
                config["preprocess"]["crop"].as<std::vector<int>>(),
                decoder_name
            );
        }

        void setupSource(
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
        )
        {
            // analyze apiPreference an type
            name_ = device_name;
            source_ = device_source;
            decoder_name_ = decoder_name;
            if (roi.size() == 4) 
            {
                roi_ = cv::Rect(roi[0], roi[1], roi[2], roi[3]);
                crop_ = roi_.area() > 0;
            }
            reopen_times_ = reopen_times;
            reopen_delay_ = reopen_delay;
            size_ = cv::Size(width, height);
            fps_ = fps;
            
            if("usb" == device_type)
            {
                apiPreference_ = cv::CAP_V4L2;
                if ("jpeg" == device_format)
                {
                    type_ = Capture::CAP_TYPE_JPEG;
                }
                else if ("yuyv" == device_format)
                {
                    type_ = Capture::CAP_TYPE_YUYV;
                }
            }
            if("stream" == device_type)
            {
                type_ = Capture::CAP_TYPE_STREAM;
            }
        }

        void clear()
        {
            if (cap_ != nullptr)
            {
                cap_->release();
                delete cap_;
                cap_ = nullptr;
            }
        }

        bool openSource()
        {
            clear();
            cap_ = Capture::createCapture(source_, apiPreference_, type_, false, decoder_name_);
            if (cap_->isOpened())
            {
                // setup width and height
                if(size_.width > 0 && size_.height > 0)
                {
                    cap_->set(cv::CAP_PROP_FRAME_WIDTH, size_.width);
                    cap_->set(cv::CAP_PROP_FRAME_HEIGHT, size_.height);
                }

                // setup fps
                if(fps_ > 0)
                {
                    cap_->set(cv::CAP_PROP_FPS, fps_);
                }
            }
            return cap_->isOpened();
        }

        std::string getDeviceName()
        {
            return name_;
        }


        void setCallback(void(*func)(const cv::Mat&))
        {
            callback_ = func;
            callback_set_ = true;
            callback_cls_set_ = false;
        }

        template <class T>
        void setCallback(void(T::*func)(const cv::Mat&), T* obj)
        {
            callback_clsf_ = boost::bind(func, obj, boost::placeholders::_1);
            callback_set_ = false;
            callback_cls_set_ = true;
        }

        bool* getRunFlag()
        {
            return &run_flag_;
        }

        void stop_service()
        {
            run_flag_ = false;
        }

        void start_service()
        {
            if (cap_ == nullptr)
            {
                openSource();
            }
            if (cap_ != nullptr)
            {
                run_flag_ = true;
                while (run_flag_)
                {
                    if (cap_->isOpened())
                    {
                        run_once();
                    }
                    else
                    {
                        if (reopen_times_ == 0) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(reopen_delay_));
                        std::cout << "reopen" << std::endl;
                        bool success = openSource();
                        if (success)
                            printf("reopen success\n");
                        else
                            printf("failed to reopen source\n");
                        reopen_times_--;
                    }
                }
            }
        }

        void run_once()
        {
            if (cap_->isOpened())
            {
                cv::Mat frame;
                if (cap_->read(frame))
                {
                    // cv::imshow("ori", frame);
                    if (crop_)
                    {
                        frame(roi_).copyTo(frame);
                    }
                    // std::cout << frame.size() << std::endl;
                    if (callback_set_)
                    {
                        
                        callback_(std::move(frame));
                    }
                    else if (callback_cls_set_)
                    {
                        callback_clsf_(std::move(frame));
                    }
                    
                }
                else
                {
                    cap_->release();
                }
            }
        }

        ~VideoServer()
        {
            clear();
        }

    private:
        bool run_flag_=true;    

        BaseCapture* cap_=nullptr;
        std::string name_="unknown";
        std::string source_;
        std::string decoder_name_="auto";
        cv::Rect roi_;
        cv::Size size_;
        int reopen_times_=-1;
        int reopen_delay_=-1;
        bool crop_=false;
        int fps_ = -1;

        bool callback_set_ = false;
        bool callback_cls_set_ = false;

        Callback_f callback_;
        Callback_clsf callback_clsf_;

        int apiPreference_ = cv::CAP_ANY;
        int type_ = Capture::CAP_TYPE_NORMAL;
    };
}


#endif