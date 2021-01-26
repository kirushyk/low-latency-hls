#pragma once
#include <list>
#include <vector>
#include <cstdint>
#include <memory>
#include <gst/gst.h>
#include "hlspartialsegment.hpp"

struct HLSSegment: HLSBaseSegment
{
    static GTimeZone *timeZone;
    GDateTime *dateTime;

    int lastPartialSegmentNumber;
    std::list<std::shared_ptr<HLSPartialSegment>> partialSegments;
    
    HLSSegment();
    ~HLSSegment();

    std::shared_ptr<HLSPartialSegment> getPartialSegment(int number) const;
};
