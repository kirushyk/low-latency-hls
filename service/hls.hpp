#pragma once
#define SEGMENTS_COUNT 5
#define SEGMENT_DURATION 4
#include <sstream>
#include <list>
#include <glib.h>
#include <gst/gst.h>

struct HLSSegment
{
    int number;
    bool finished;
    static GTimeZone *timeZone;
    GDateTime *dateTime;
    // GstBuffer *buffer;
    // GstMapInfo mapInfo;
    // GstSample *sample;
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
