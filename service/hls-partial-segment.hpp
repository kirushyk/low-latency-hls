#pragma once
#include "hls-base-segment.hpp"

struct HLSPartialSegment: HLSBaseSegment
{
    bool independent;

    HLSPartialSegment();
};
