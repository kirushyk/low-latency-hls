#include "hls.hpp"
#include <gst/video/video.h>
#include <iostream>

GTimeZone * HLSSegment::timeZone = NULL;

HLSSegment::HLSSegment()
{
    finished = false;
    duration = 0;
    pts = 0;
    if (timeZone == NULL)
    {
        timeZone = g_time_zone_new_utc();
    }
    dateTime = g_date_time_new_now(timeZone);
}

HLSSegment::~HLSSegment()
{
    if (dateTime)
    {
        g_date_time_unref(dateTime);
    }
}

HLSOutput::HLSOutput()
{
    lastIndex = 0;
    lastSegmentNumber = 1;
    mediaSequenceNumber = 1;
}

void HLSOutput::pushSample(GstSample *sample)
{
    std::shared_ptr<HLSSegment> segment;
    
    GstBuffer *buffer = gst_sample_get_buffer(sample);

    std::shared_ptr<HLSSegment> recentSegment;
    if (segments.size())
    {
        recentSegment = segments.back();
        bool targetDurationSoon = (buffer->pts - recentSegment->pts) > ((SEGMENT_DURATION - 2) * GST_SECOND);
        bool sampleContainsIDR = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
        if (!(targetDurationSoon && sampleContainsIDR))
        {
            segment = recentSegment;
        }
    }

    if (!segment)
    {
        if (recentSegment)
        {
            recentSegment->duration = buffer->pts - recentSegment->pts;
            recentSegment->finished = true;
        }
        segment = std::make_shared<HLSSegment>();
        segment->pts = buffer->pts;
        segment->number = lastSegmentNumber++;
        segments.push_back(segment);
        if (segments.size() > SEGMENTS_COUNT)
        {
            mediaSequenceNumber++;
            segments.pop_front();
        }
    }

    GstMapInfo mapInfo;
    gst_buffer_map(buffer, &mapInfo, (GstMapFlags)(GST_MAP_READ));
    segment->data.push_back(std::vector<std::uint8_t>(mapInfo.data, mapInfo.data + mapInfo.size));
    gst_buffer_unmap(buffer, &mapInfo);
    gst_sample_unref(sample);
}

std::shared_ptr<HLSSegment> HLSOutput::getSegment(int number) const
{
    for (const auto& segment: segments)
    {
        if (segment->number == number)
        {
            return segment;
        }
    }
    return NULL;
}

std::string HLSOutput::getPlaylist() const
{
    std::stringstream ss;
    ss << "#EXTM3U" << std::endl;
    ss << "#EXT-X-TARGETDURATION:" << SEGMENT_DURATION << std::endl;
    ss << "#EXT-X-VERSION:4" << std::endl;
    ss << "#EXT-X-MEDIA-SEQUENCE:" << mediaSequenceNumber << std::endl;
    bool dateTimeReported = false;
    for (const auto& segment: segments)
    {
        if (segment->finished)
        {
            if (!dateTimeReported)
            {
                gchar *formattedDateTime = g_date_time_format_iso8601(segment->dateTime);
                ss << "#EXT-X-PROGRAM-DATE-TIME:" << formattedDateTime << std::endl;
                g_free(formattedDateTime);
                dateTimeReported = true;
            }
            ss << "#EXTINF:" << segment->duration * 0.000000001 << "," << std::endl;
            ss << "/api/segments/" << segment->number << ".ts" << std::endl;
        }
    }
    return ss.str();
}
