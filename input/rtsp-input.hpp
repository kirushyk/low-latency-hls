#pragma once
#include <memory>
#include <gst/gst.h>

class RTSPInput
{
public:
    class Delegate
    {
    public:
        virtual void onSample(GstSample *sample) = 0;
        virtual ~Delegate();
    };
    RTSPInput(const char *url, std::shared_ptr<Delegate> delegate);
    ~RTSPInput();
    struct Private;
private:
    std::shared_ptr<Private> priv;
};
