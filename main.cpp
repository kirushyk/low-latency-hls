#include "config.h"
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <memory>
#include <list>
#include <iostream>
#include <glib.h>
#include <gst/gst.h>
#include "input/rtsp-input.hpp"
#include "http/api.hpp"
#include "hls/output.hpp"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "usage:\n\t" << argv[0] << " rtsp://..." << std::endl;
        return 0;
    }

    std::shared_ptr<HLSOutput> hlsOutput = std::make_shared<HLSOutput>();
    std::shared_ptr<RTSPInput> rtspInput = std::make_shared<RTSPInput>(argv[1], hlsOutput);
    std::shared_ptr<HTTPAPI> httpAPI = std::make_shared<HTTPAPI>(8080, hlsOutput);

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);
    
    return 0;
}
