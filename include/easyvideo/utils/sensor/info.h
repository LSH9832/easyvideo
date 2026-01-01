#ifndef SENSOR_INFO_H
#define SENSOR_INFO_H

#include <iostream>
#include <opencv2/opencv.hpp>
#include <yaml-cpp/yaml.h>
// #include "easycpp/logger.h"


namespace sensor {
namespace camera {
    
    struct Info {
        
        struct Device
        {
            std::string name, source, type, format;
            int fps, width, height;
            cv::Rect crop;
            int reopen_times=-1;
            int reopen_delay=100; // ms
            YAML::Node _config;
        } device;

        struct Params
        {
            bool enable_undistortion;
            cv::Mat intrinsic;
            cv::Mat intrinsic_undis;
            cv::Mat extrinsic_lidar2cam;
            cv::Mat extrinsic_lidar2vehicle;
            cv::Mat distcoeff;
            double skew;
            std::vector<double> extrinsic_tune;
            cv::Size img_size;
            YAML::Node _config;
        } params;

        struct Publish {
            struct Topics
            {
                std::string base;
                std::string origin_img;
                std::string undis_img;
                std::string resized_img;
                std::string result;
            } topics;
            bool compressed, multi_thread, pub_ori, pub_undis, pub_resized;
            int compress_quality = 90;
            cv::Size resize_size;
            YAML::Node _config;
        } publish;

        YAML::Node userdata;
    };

    cv::Mat decode_matrix(YAML::Node cfg) {
        std::vector<double> data = cfg["data"].as<std::vector<double>>();
        int rows = cfg["rows"].as<int>();
        int cols = cfg["cols"].as<int>();
        cv::Mat mat = cv::Mat(rows, cols, CV_64F);
        for (int row=0; row<rows;row++)
            for (int col=0;col<cols;col++)
                mat.at<double>(row, col) = data[col + row * cols];
        return mat;
    }

    cv::Mat decode_matrix(YAML::Node cfg, int rows, int cols=-1) {
        if (cols == -1) cols = rows;
        std::vector<double> data = cfg.as<std::vector<double>>();
        cv::Mat mat = cv::Mat(rows, cols, CV_64F);
        for (int row=0; row<rows;row++)
            for (int col=0;col<cols;col++)
                mat.at<double>(row, col) = data[col + row * cols];
        return mat;
    }

    cv::Size decode_size(YAML::Node cfg) {
        std::vector<int> data = cfg.as<std::vector<int>>();
        assert(data.size()==2);
        return cv::Size(data.at(0), data.at(1));
    }

    Info load(std::string _yaml, bool show_info=false) {
        Info info;
        YAML::Node cfg = YAML::LoadFile(_yaml);

        auto publish = cfg["publish"];
        auto undistortion = cfg["undistortion"];
        auto params = undistortion["params"];
        auto device = cfg["device"];

        info.device._config = device;
        info.params._config = params;
        info.publish._config = publish;

        info.userdata = cfg["userdata"];

        std::string base_topic = publish["topic"].as<std::string>();
        info.publish.compressed = publish["compressed"].as<bool>();
        info.publish.compress_quality = publish["compress_quality"].as<int>();
        
        info.publish.topics.base = base_topic;
        info.publish.topics.origin_img  = base_topic; // + "/origin";
        info.publish.topics.undis_img   = base_topic + "/undistort";
        info.publish.topics.resized_img = base_topic + "/resize";
        if (info.publish.compressed)
        {
            info.publish.topics.origin_img  += "/compressed";
            info.publish.topics.undis_img   += "/compressed";
            info.publish.topics.resized_img += "/compressed";
        }

        info.publish.topics.result = publish["box_topic"].as<std::string>();

        // std::cout << "resize_size" << std::endl;
        info.publish.resize_size = decode_size(publish["size"]);
        // std::cout << "resize_size end" << std::endl;
        info.publish.pub_ori = publish["pub_ori"].as<bool>();
        info.publish.pub_undis = publish["pub_undis"].as<bool>();
        info.publish.pub_resized = publish["pub_resized"].as<bool>();
        info.publish.multi_thread = publish["multi_thread"].as<bool>();
        
        info.params.enable_undistortion = undistortion["enable"].as<bool>();
        info.params.intrinsic = decode_matrix(params["intrinsic_matrix"], 3);
        info.params.intrinsic_undis = decode_matrix(params["intrinsic_matrix_undis"], 3);
        info.params.extrinsic_lidar2cam = decode_matrix(params["extrinsic_matrix"], 4);
        info.params.extrinsic_tune = params["extrinsic_matrix_tune"].as<std::vector<double>>();
        info.params.distcoeff = decode_matrix(params["distort_params"], 1, 5);
        info.params.skew = params["skew"].as<double>();
        // std::cout << "img_size" << std::endl;
        info.params.img_size = decode_size(params["size"]);
        // std::cout << "img_size end" << std::endl;
        if (params["lidar2vehicle_ex_mat"].IsDefined())
        {
            info.params.extrinsic_lidar2vehicle = decode_matrix(cfg["lidar2vehicleExMat"], 4);
        }
        else
        {
            info.params.extrinsic_lidar2vehicle = cv::Mat::eye(4, 4, CV_64F);
        }

        info.device.name = device["name"].as<std::string>();
        info.device.source = device["source"].as<std::string>();
        info.device.type = device["type"].as<std::string>();
        info.device.format = device["format"].as<std::string>();
        info.device.fps = device["fps"].as<int>();
        info.device.width = device["width"].as<int>();
        info.device.height = device["height"].as<int>();

        // std::cout << "crop" << std::endl;
        auto crop = device["preprocess"]["crop"].as<std::vector<int>>();
        // std::cout << "crop end" << std::endl;
        info.device.crop.x = crop[0];
        info.device.crop.y = crop[1];
        info.device.crop.width = crop[2];
        info.device.crop.height = crop[3];

        info.device.reopen_times = device["preprocess"]["reopen"]["times"].as<int>();
        info.device.reopen_delay = device["preprocess"]["reopen"]["delay"].as<int>();

        if (show_info) {
            std::cout << "\n--------------------------------------------------------------------------" 
                      << "\nconfig file path           : " << _yaml
                      << "\ndetect result topic        : " << info.publish.topics.result
                      << "\ncamera topic               : " << info.publish.topics.origin_img
                      << "\ncamera intrinsic matrix    : \n" << info.params.intrinsic
                      << "\nlidar2cam entrinsic matrix : \n" << info.params.extrinsic_lidar2cam << "\n" << std::endl;
        }

        return info;
    }

}
}

#endif 