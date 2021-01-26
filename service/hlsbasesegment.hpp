#pragma once

struct HLSBaseSegment
{
    int number;
    bool finished;
    GstClockTime pts;
    GstClockTime duration;
    std::list<std::vector<std::uint8_t>> data;
};
