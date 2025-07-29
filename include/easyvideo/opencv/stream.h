#ifndef STREAM_H
#define STREAM_H

#include <iostream>
#include <stdio.h>

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

    void close();

private:

    struct Impl;
    Impl* impl=nullptr;

};

#endif