#pragma once
#define SEGMENTS_COUNT 7
#define SEGMENT_DURATION 6
#define PARTIAL_SEGMENT_MAX_DURATION 0.3
#define PARTIAL_SEGMENT_MIN_DURATION 0.2
#include <sstream>
#include <memory>
#include <list>
#include <glib.h>
#include <vector>
#include <cstdint>
#include <gst/gst.h>
#include "segment.hpp"
#include "../input/rtsp-input.hpp"

struct HLSOutput: public RTSPInput::Delegate
{
    int lastIndex, lastSegmentNumber, mediaSequenceNumber;
    std::list<std::shared_ptr<HLSSegment>> segments;
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
    std::weak_ptr<Delegate> delegate;
    virtual void onSample(GstSample *sample) override final;
    std::shared_ptr<HLSSegment> getSegment(int number) const;
    std::string getPlaylist() const;
    std::string getLowLatencyPlaylist() const;
};
