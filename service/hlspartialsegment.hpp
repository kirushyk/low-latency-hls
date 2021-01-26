#pragma once
#include <list>
#include <vector>
#include <cstdint>
#include <gst/gst.h>
#include "hlsbasesegment.hpp"

struct HLSPartialSegment: HLSBaseSegment
{
    bool independent;

    HLSPartialSegment();
};
