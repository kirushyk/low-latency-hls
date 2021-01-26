#pragma once
#include <list>
#include <vector>
#include <cstdint>
#include <memory>
#include <gst/gst.h>
#include "hlspartialsegment.hpp"

struct HLSSegment
{
    int number;
    bool finished;

    static GTimeZone *timeZone;
    GDateTime *dateTime;

    GstClockTime duration;
    GstClockTime pts;
    std::list<std::vector<std::uint8_t>> data;

    int lastPartialSegmentNumber;
    std::list<std::shared_ptr<HLSPartialSegment>> partialSegments;
    
    HLSSegment();
    ~HLSSegment();

    std::shared_ptr<HLSPartialSegment> getPartialSegment(int number) const;
};
