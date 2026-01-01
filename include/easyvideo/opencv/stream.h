#ifndef STREAM_H
#define STREAM_H

#include <iostream>
#include <stdio.h>
#include <vector>

class Stream
{
public:
    struct StreamData
    {
        void* data=nullptr;
        size_t size=0;
    };
    

    Stream() {}

    int open(std::string url);

    int width();

    int height();

    int fps();

    int codec_id();

    int cur_dts();

    void* read(int& size);

    void* read(int& size, int64_t& pts, int64_t& dts, bool& isKeyFrame);

    void close();

    void setStep(size_t step);

private:

    struct Impl;
    Impl* impl=nullptr;

};

#endif