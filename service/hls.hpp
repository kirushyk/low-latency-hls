#pragma once
#define SEGMENTS_COUNT 7
#define SEGMENT_DURATION 6
#define PARTIAL_SEGMENT_DURATION 0.3
#include <sstream>
#include <memory>
#include <list>
#include <glib.h>
#include <vector>
#include <cstdint>
#include <gst/gst.h>

struct HLSPartialSegment
{
    int number;

    GstClockTime duration;
    GstClockTime pts;
    std::list<std::vector<std::uint8_t>> data;

    bool independent;
};

struct HLSSegment
{
    int number;
    bool finished;

    static GTimeZone *timeZone;
    GDateTime *dateTime;

    GstClockTime duration;
    GstClockTime pts;
    std::list<std::vector<std::uint8_t>> data;

    std::list<std::shared_ptr<HLSPartialSegment>> partialSegments;
    
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
    std::string getLowLatencyPlaylist() const;
};
