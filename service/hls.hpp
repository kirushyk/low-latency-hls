#pragma once
#define SEGMENTS_COUNT 7
#define SEGMENT_DURATION 6
#include <sstream>
#include <list>
#include <glib.h>
#include <vector>
#include <cstdint>
#include <gst/gst.h>

struct HLSSegment
{
    int number;
    bool finished;
    static GTimeZone *timeZone;
    GDateTime *dateTime;
    GstClockTime duration;
    GstClockTime pts;
    // std::vector<std::uint8_t> data;
    std::stringstream data;
    

    HLSSegment();
    ~HLSSegment();
};

struct HLSOutput
{
    int lastIndex, lastSegmentNumber, mediaSequenceNumber;
    std::list<std::shared_ptr<HLSSegment>> segments;
public:
    HLSOutput();
    void pushSample(GstSample *sample);
    std::shared_ptr<HLSSegment> getSegment(int number) const;
    std::string getPlaylist() const;
};
