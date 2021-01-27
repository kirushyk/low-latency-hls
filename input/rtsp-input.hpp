#pragma once
#include <memory>
#include <gst/gst.h>

class RTSPInput
{
public:
    RTSPInput(const char *url);
    ~RTSPInput();
    class Delegate
    {
    public:
        virtual void onSample(GstSample *sample) = 0;
        virtual ~Delegate();
    };
    std::weak_ptr<Delegate> delegate;
    GstElement *pipeline;
};
