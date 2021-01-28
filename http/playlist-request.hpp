#pragma once
#include <libsoup/soup.h>

struct PlaylistRequest
{
    SoupMessage *msg;
    int mediaSequenceNumber;
    int partIndex;
    bool skip;
    bool processed;
};
