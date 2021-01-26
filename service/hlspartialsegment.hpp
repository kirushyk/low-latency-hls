#pragma once
#include <list>
#include <vector>
#include <cstdint>
#include <gst/gst.h>
#include "hlsbasesegment.hpp"

struct HLSPartialSegment
{
    int number;
    bool finished;

    GstClockTime duration;
    GstClockTime pts;
    std::list<std::vector<std::uint8_t>> data;

    bool independent;

    HLSPartialSegment();
};
