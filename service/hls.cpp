#include "hls.hpp"

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
    // if (buffer)
    //     gst_buffer_unmap(buffer, &mapInfo);
    // if (sample)
    //     gst_sample_unref(sample);
    if (dateTime)
    {
        g_date_time_unref(dateTime);
    }
}

HLSOutput::HLSOutput()
{
    lastIndex = 0;
    lastSegmentNumber = 0;
}

void HLSOutput::pushSample(GstSample *sample)
{
    std::shared_ptr<HLSSegment> segment;
    
    GstBuffer *buffer = gst_sample_get_buffer(sample);

    std::shared_ptr<HLSSegment> recentSegment;
    if (segments.size())
    {
        recentSegment = segments.back();
        // if ((buffer->duration == GST_CLOCK_TIME_NONE) || ((recentSegment->duration + buffer->duration) < (SEGMENT_DURATION * GST_SECOND)))
        if ((buffer->pts - recentSegment->pts) < (SEGMENT_DURATION * GST_SECOND))
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
            segments.pop_front();
        }
    }

    GstMapInfo mapInfo;
    if (buffer->duration != GST_CLOCK_TIME_NONE)
        segment->duration += buffer->duration;
    else
        segment->duration = buffer->pts - segment->pts + 0;
    gst_buffer_map(buffer, &mapInfo, (GstMapFlags)(GST_MAP_READ));
    segment->data.write(reinterpret_cast<const char *>(mapInfo.data), mapInfo.size);
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
    ss << "#EXT-X-VERSION:6" << std::endl;
    ss << "#EXT-X-MEDIA-SEQUENCE:" << lastSegmentNumber << std::endl;
    // gchar *g_date_time_format_iso8601 (GDateTime *datetime);
    // ss << "#EXT-X-PROGRAM-DATE-TIME:2019-02-14T02:13:36.106Z" << std::endl;
    for (const auto& segment: segments)
    {
        if (segment->finished && segment->data)
        {
            ss << "#EXTINF:" << segment->duration * 0.000000001 << "," << std::endl;
            ss << "/api/segments/" << segment->number << ".ts" << std::endl;
        }
    }
    return ss.str();
}
