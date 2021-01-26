#pragma once
#include "hlsbasesegment.hpp"

struct HLSPartialSegment: HLSBaseSegment
{
    bool independent;

    HLSPartialSegment();
};
