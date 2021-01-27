#pragma once
#include <memory>
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
