#pragma once
#include <list>
#include <vector>
#include <cstdint>
#include <gst/gst.h>

struct HLSBaseSegment
{
    int number;
    bool finished;
    GstClockTime pts;
    GstClockTime duration;
    std::list<std::vector<std::uint8_t>> data;
    HLSBaseSegment();
};
