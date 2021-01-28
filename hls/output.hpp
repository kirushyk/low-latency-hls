#pragma once
#define SEGMENTS_COUNT 7
#define SEGMENT_DURATION 6
#define PARTIAL_SEGMENT_MAX_DURATION 0.3
#define PARTIAL_SEGMENT_MIN_DURATION 0.2
#include <cstdint>
#include <sstream>
#include <memory>
#include <gst/gst.h>
#include "segment.hpp"
#include "../input/rtsp-input.hpp"

class HLSOutput: public RTSPInput::Delegate
{
public:
    HLSOutput();
    virtual ~HLSOutput();
    class Delegate
    {
    public:
        virtual void onPartialSegment() = 0;
        virtual void onSegment() = 0;
        virtual ~Delegate();
    };
    void setDelegate(std::shared_ptr<Delegate>);
    virtual void onSample(GstSample *sample) override final;
    std::shared_ptr<HLSSegment> getSegment(int number) const;
    std::string getPlaylist(bool lowLatency, bool skip) const;
    bool msnWrong(int msn) const;
    bool segmentReady(int msn) const;
    bool partialSegmentReady(int msn, int partIndex) const;
    struct Private;
private:
    std::shared_ptr<Private> priv;
};
