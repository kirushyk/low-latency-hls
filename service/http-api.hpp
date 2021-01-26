#pragma once
#include <libsoup/soup.h>
#include "hls.hpp"

class HTTPAPI: public HLSOutput::Delegate
{
    SoupServer *http_server;
public:
    HTTPAPI(const int port);
    virtual ~HTTPAPI();
    virtual void onPartialSegment() override final;
    virtual void onSegment() override final;
    std::shared_ptr<HLSOutput> hlsOutput;
};
