#pragma once
#include <list>
#include <vector>
#include <memory>
#include <glib.h>
#include "rtsp-input.hpp"
#include "partial-segment.hpp"

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
