#include "hlssegment.hpp"

GTimeZone * HLSSegment::timeZone = NULL;

HLSSegment::HLSSegment()
{
    if (timeZone == NULL)
    {
        timeZone = g_time_zone_new_utc();
    }
    dateTime = g_date_time_new_now(timeZone);
    lastPartialSegmentNumber = 1;
}

HLSSegment::~HLSSegment()
{
    if (dateTime)
    {
        g_date_time_unref(dateTime);
    }
}

std::shared_ptr<HLSPartialSegment> HLSSegment::getPartialSegment(int number) const
{
    for (const auto& partialSegment: partialSegments)
    {
        if (partialSegment->number == number)
        {
            return partialSegment;
        }
    }
    return std::shared_ptr<HLSPartialSegment>();
}
