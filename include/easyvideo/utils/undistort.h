#ifndef UNDISTORT_H
#define UNDISTORT_H

#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include "Eigen/Eigen"
#include "yaml-cpp/yaml.h"

struct CAM_PARAMS {
    double k1, k2, k3, p1, p2, fx, fy, cx, cy, skew;
    cv::Size imgsz;
};

CAM_PARAMS loadCamParams(std::string file);

class Undistortion
{
public:

    Undistortion();
    void init(double k1, double k2, double k3, double p1, double p2, double fx, double fy, double cx, double cy, cv::Size image_size);
    void init(CAM_PARAMS cam_params);
    void init(YAML::Node config);
    void set_size(cv::Size ipt_sz);
    void undistort(cv::Mat image, cv::Mat &output, bool show);

    std::vector<std::pair<Eigen::Vector2i, Eigen::Vector2i>> uv_map;

    void ComputeMap();
    void run(const cv::Mat &input_image, cv::Mat &current_image_, int interpolation=cv::INTER_LINEAR);
    void oneUndistInit();
    cv::Mat current_image;
    cv::Mat input_image_resized;

    Eigen::Matrix<float, 3, 3> camera_K;
    Eigen::Matrix<float, 3, 3> camera_K_undis;
    Eigen::Matrix<float, 3, 4> camera_P;
    Eigen::Matrix<float, 1, 5> camera_distcoeff;

public:
    double k1;
    double k2;
    double k3;
    double p1;
    double p2;
    // 内参
    double fx;
    double fy;
    double cx;
    double cy;
    double skew;
    cv::Size imgsz, iptsz;
    cv::Mat map1, map2;
    bool init_already = false;


};

CAM_PARAMS loadCamParams(YAML::Node cfg){

    CAM_PARAMS cam_p;
    YAML::Node in_mat = cfg["intrinsic_matrix"],
               distort_params = cfg["distort_params"];
    cam_p.fx = in_mat[0].as<double>();
    cam_p.fy = in_mat[4].as<double>();
    cam_p.cx = in_mat[2].as<double>();
    cam_p.cy = in_mat[5].as<double>();
    cam_p.k1 = distort_params[0].as<double>();
    cam_p.k2 = distort_params[1].as<double>();
    cam_p.k3 = distort_params[2].as<double>();
    cam_p.p1 = distort_params[3].as<double>();
    cam_p.p2 = distort_params[4].as<double>();
    cam_p.imgsz = cv::Size(cfg["size"][0].as<int>(), cfg["size"][1].as<int>());
    cam_p.skew = cfg["skew"].as<double>();

    return cam_p;
}


Undistortion::Undistortion() {}

void Undistortion::init(CAM_PARAMS cam_params){
    this->k1 = cam_params.k1;
    this->k2 = cam_params.k2;
    this->k3 = cam_params.k3;
    this->p1 = cam_params.p1;
    this->p2 = cam_params.p2;
    // 内参
    this->fx = cam_params.fx;
    this->fy = cam_params.fy;
    this->cx = cam_params.cx;
    this->cy = cam_params.cy;

    this->imgsz = cam_params.imgsz;

    this->camera_K << fx, 0, cx,
                      0, fy, cy,
                      0, 0, 1;
    
    this->skew = cam_params.skew;
    this->camera_distcoeff << k1, k2, p1, p2, k3;
    this->init_already = true;
}

void Undistortion::init(YAML::Node config){
    CAM_PARAMS cam_params = loadCamParams(config);
    this->init(cam_params);
}

void Undistortion::init(double k1,double k2,double k3,double p1,double p2,double fx,double fy,double cx,double cy, cv::Size image_size)
{
    this->k1 = k1;
    this->k2 = k2;
    this->k3 = k3;
    this->p1 = p1;
    this->p2 = p2;
    // 内参
    this->fx = fx;
    this->fy = fy;
    this->cx = cx;
    this->cy = cy;

    this->imgsz = image_size;

    this->camera_K << fx, 0, cx,
                      0, fy, cy,
                      0, 0, 1;
    
    this->camera_distcoeff << k1, k2, p1, p2, k3;
    this->init_already = true;
}

void Undistortion::set_size(cv::Size ipt_sz) {
    double ratio = std::max(this->imgsz.height / ipt_sz.height, this->imgsz.width / ipt_sz.width);
    this->imgsz.height = (int)((double)this->imgsz.height / ratio);
    this->imgsz.width = (int)((double)this->imgsz.width / ratio);

    this->fx /= ratio;
    this->fy /= ratio;
    this->cx /= ratio;
    this->cy /= ratio;

    this->camera_K << fx, 0, cx,
                      0, fy, cy,
                      0, 0, 1;

    // this->init_already = true;
}

void Undistortion::oneUndistInit(){
    current_image.create(this->imgsz, CV_8UC3);
    current_image.setTo(0);
    ComputeMap();
    current_image.setTo(0);
}

void Undistortion::ComputeMap() {
    cv::Mat cameraMatrix = (cv::Mat_<double>(3,3) << fx, skew, cx, 0, fy, cy, 0, 0, 1);
    cv::Mat newCamMat = (cv::Mat_<double>(3,3) << fx, skew*fx, cx, 0, fy, cy, 0, 0, 1);
    cv::Mat distCoeffs = (cv::Mat_<double>(1,5) << k1, k2, p1, p2, k3);
    // 生成映射表
    
    cv::initUndistortRectifyMap(cameraMatrix, distCoeffs, cv::Mat(), newCamMat, current_image.size(), CV_16SC2, map1, map2);

    // 进行重映射去畸变
    // cv::Mat dest;
    // cv::remap(img, dest, map1, map2, cv::INTER_LINEAR);
}

void Undistortion::run(const cv::Mat &input_image, cv::Mat &current_image_, int interpolation)
{
    iptsz = input_image.size();
    bool need_resize = !(iptsz.height == this->imgsz.height && iptsz.width == this->imgsz.width);
    if (need_resize) cv::resize(input_image, input_image_resized, this->imgsz);
    else input_image_resized = input_image;
    cv::remap(input_image_resized, current_image_, map1, map2, interpolation);
    if (need_resize) cv::resize(current_image_, current_image_, iptsz);
}

#endif // UNDISTORT_H
