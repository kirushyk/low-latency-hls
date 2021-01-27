#pragma once
#include <memory>
#include <libsoup/soup.h>
#include "../hls/output.hpp"

class HTTPAPI
{
public:
    HTTPAPI(const int port, std::shared_ptr<HLSOutput> hlsOutput);
    virtual ~HTTPAPI();
private:
    struct Private;
    std::shared_ptr<Private> priv;
};
