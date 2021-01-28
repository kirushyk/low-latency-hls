#include "output.hpp"
#include <list>
#include <vector>
#include <iostream>
#include <gst/video/video.h>

struct HLSOutput::Private
{
    int lastIndex, mediaSequenceNumber;
    std::list<std::shared_ptr<HLSSegment>> segments;
    std::weak_ptr<Delegate> delegate;
    GstClockTime recentPTS;
};

HLSOutput::Delegate::~Delegate()
{

}

HLSOutput::HLSOutput():
    priv(std::make_shared<Private>())
{
    priv->lastIndex = 0;
    priv->mediaSequenceNumber = 0;
    priv->recentPTS = GST_CLOCK_TIME_NONE;
}

HLSOutput::~HLSOutput()
{

}

void HLSOutput::setDelegate(std::shared_ptr<Delegate> delegate)
{
    priv->delegate = delegate;
}

void HLSOutput::onSample(GstSample *sample)
{
    std::shared_ptr<HLSSegment> segment;
    
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstClockTime pts = priv->recentPTS;
    bool sampleContainsIDR = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
    if (buffer->pts != GST_CLOCK_TIME_NONE)
    {
        pts = buffer->pts;
        priv->recentPTS = pts;
    }

    std::shared_ptr<HLSSegment> recentSegment;
    if (priv->segments.size())
    {
        recentSegment = priv->segments.back();
        bool targetDurationSoon = (pts - recentSegment->pts) > ((SEGMENT_DURATION - PARTIAL_SEGMENT_MAX_DURATION) * GST_SECOND);
        if (!(targetDurationSoon && sampleContainsIDR))
        {
            segment = recentSegment;
        }
    }

    if (!segment)
    {
        if (recentSegment)
        {
            recentSegment->duration = pts - recentSegment->pts;
            recentSegment->finished = true;
            if (recentSegment->partialSegments.size())
            {
                std::shared_ptr<HLSPartialSegment> recentPartialSegment = recentSegment->partialSegments.back();
                recentPartialSegment->duration = pts - recentPartialSegment->pts;
                recentPartialSegment->finished = true;
                if (auto delegate = priv->delegate.lock())
                {
                    delegate->onPartialSegment();
                }
            }
            if (auto delegate = priv->delegate.lock())
            {
                delegate->onSegment();
            }
        }
        segment = std::make_shared<HLSSegment>();
        segment->pts = pts;
        segment->number = priv->mediaSequenceNumber++;
        priv->segments.push_back(segment);
        if (priv->segments.size() > SEGMENTS_COUNT)
        {
            priv->segments.pop_front();
        }
    }

    std::shared_ptr<HLSPartialSegment> partialSegment;
    std::shared_ptr<HLSPartialSegment> recentPartialSegment;
    if (segment->partialSegments.size())
    {
        recentPartialSegment = segment->partialSegments.back();
    }
    if (recentPartialSegment)
    {
        if ((pts - recentPartialSegment->pts) < PARTIAL_SEGMENT_MIN_DURATION * GST_SECOND)
        {
            partialSegment = recentPartialSegment;
        }
    }
    if (!partialSegment)
    {
        if (recentPartialSegment)
        {
            recentPartialSegment->duration = pts - recentPartialSegment->pts;
            recentPartialSegment->finished = true;
            if (auto delegate = priv->delegate.lock())
            {
                delegate->onPartialSegment();
            }
        }
        partialSegment = std::make_shared<HLSPartialSegment>();
        partialSegment->pts = pts;
        partialSegment->number = segment->lastPartialSegmentNumber++;
        if (sampleContainsIDR)
        {
            partialSegment->independent = true;
        }
        segment->partialSegments.push_back(partialSegment);
    }

    GstMapInfo mapInfo;
    gst_buffer_map(buffer, &mapInfo, (GstMapFlags)(GST_MAP_READ));
    segment->data.push_back(std::vector<std::uint8_t>(mapInfo.data, mapInfo.data + mapInfo.size));
    partialSegment->data.push_back(std::vector<std::uint8_t>(mapInfo.data, mapInfo.data + mapInfo.size));
    gst_buffer_unmap(buffer, &mapInfo);
    gst_sample_unref(sample);

    for (auto &oldSegment: priv->segments)
    {
        if ((pts - oldSegment->pts) > 3 * SEGMENT_DURATION * GST_SECOND)
        {
            oldSegment->partialSegments.clear();
        }
    }
}

std::shared_ptr<HLSSegment> HLSOutput::getSegment(int number) const
{
    for (const auto& segment: priv->segments)
    {
        if (segment->number == number)
        {
            return segment;
        }
    }
    return std::shared_ptr<HLSSegment>();
}

std::string HLSOutput::getPlaylist(bool lowLatency, bool skip) const
{
    std::stringstream ss;
    ss << "#EXTM3U" << std::endl;
    ss << "#EXT-X-TARGETDURATION:" << SEGMENT_DURATION << std::endl;
    ss << "#EXT-X-VERSION:" << (lowLatency ? 6 : 3) << std::endl;
    if (lowLatency)
    {
        ss << "#EXT-X-SERVER-CONTROL:CAN-BLOCK-RELOAD=YES,PART-HOLD-BACK=" << PARTIAL_SEGMENT_MAX_DURATION * 3;
        // if (priv->segments.size() > 4)
        // {
        //     ss << ",CAN-SKIP-UNTIL=" << 6 * SEGMENT_DURATION;
        // }
        ss << std::endl;
        bool partInfReported = false;
        for (const auto& segment: priv->segments)
        {
            for (const auto &partialSegment: segment->partialSegments)
            {
                (void)partialSegment;
                ss << "#EXT-X-PART-INF:PART-TARGET=" << PARTIAL_SEGMENT_MAX_DURATION << std::endl;
                partInfReported = true;
                break;
            }
            if (partInfReported)
            {
                break;
            }
        }
    }
    bool msnReported = false;
    bool dateTimeReported = false;
    for (const auto& segment: priv->segments)
    {
        if (!msnReported)
        {
            ss << "#EXT-X-MEDIA-SEQUENCE:" << segment->number << std::endl;
            msnReported = true;
        }
        if (lowLatency) for (const auto &partialSegment: segment->partialSegments)
        {
            if (partialSegment->finished)
            {
                ss << "#EXT-X-PART:DURATION=" << partialSegment->duration * 0.000000001;
                ss << ",URI=\"/api/partial/" << segment->number << "." << partialSegment->number << ".ts\"";
                if (partialSegment->independent)
                {
                    ss << ",INDEPENDENT=YES";
                }
                ss << std::endl;
            }
            else
            {
                ss << "#EXT-X-PRELOAD-HINT:TYPE=PART,URI=\"/api/partial/" << segment->number << "." << partialSegment->number << ".ts\"" << std::endl;
            }
        }
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
    
bool HLSOutput::msnWrong(int msn) const
{
    return (msn - priv->mediaSequenceNumber) > 2;
}

bool HLSOutput::segmentReady(int msn) const
{
    return (msn + 1) < priv->mediaSequenceNumber;
}

bool HLSOutput::partialSegmentReady(int msn, int partIndex) const
{
    if (priv->segments.size())
    {
        return segmentReady(msn + 1) ||
            ((msn < priv->mediaSequenceNumber) && ((partIndex + 1) < priv->segments.back()->number));
    }
    return false;
}
