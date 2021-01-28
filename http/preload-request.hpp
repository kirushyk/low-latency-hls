#pragma once
#include <libsoup/soup.h>

struct PreloadRequest
{
    bool processed;
    SoupMessage *msg;
    int mediaSequenceNumber;
    int partIndex;
    bool skip;
};
