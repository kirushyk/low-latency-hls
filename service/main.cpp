#include "config.h"
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <memory>
#include <list>
#include <iostream>
#include <glib.h>
#include <gst/gst.h>
#include "rtsp-input.hpp"
#include "http-api.hpp"
#include "hls.hpp"

int main(int argc, char *argv[])
{
    std::shared_ptr<HLSOutput> hlsOutput = std::make_shared<HLSOutput>();

    if (argc != 2)
    {
        std::cerr << "usage:\n\t" << argv[0] << " rtsp://..." << std::endl;
        return 0;
    }

    RTSPInput rtspInput(argv[1]);
    rtspInput.delegate = hlsOutput;

    HTTPAPI httpAPI(8080);
    httpAPI.hlsOutput = hlsOutput;

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);
    
    return 0;
}
