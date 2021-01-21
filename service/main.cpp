#include <cstdint>
#include <vector>
#include <map>
#include <iostream>
#include <libsoup/soup.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include "file-endpoint.hpp"

static gboolean on_message(GstBus *bus, GstMessage *message, gpointer user_data)
{
    // GMainLoop *main_loop = reinterpret_cast<GMainLoop *>(user_data);
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
    case GST_MESSAGE_ERROR:
        // g_main_loop_quit(main_loop);
        break;
    default:
        break; 
    }
    return TRUE;
}

struct HLSSegment
{
    int number;
    GstBuffer *buffer;
    GstMapInfo mapInfo;
    GstSample *sample;
};

#define SEGMENTS_COUNT 5

struct HLSOutput
{
    int lastIndex, lastSegment;
    HLSSegment segments[SEGMENTS_COUNT];
public:
    HLSOutput();
    void pushSegment(GstSample *sample);
    const HLSSegment * getSegment(int number);
};

HLSOutput::HLSOutput()
{
    lastIndex = 0;
    lastSegment = 0;
    for (int i = 0; i < SEGMENTS_COUNT; i++)
    {
        segments[i].buffer = NULL;
        segments[i].sample = NULL;
    }
}

void HLSOutput::pushSegment(GstSample *sample)
{
    if (segments[lastIndex].buffer)
        gst_buffer_unmap(segments[lastIndex].buffer, &segments[lastIndex].mapInfo);
    if (segments[lastIndex].sample)
        gst_sample_unref(segments[lastIndex].sample);
    segments[lastIndex].buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(segments[lastIndex].buffer, &segments[lastIndex].mapInfo, (GstMapFlags)(GST_MAP_READ));
    
    lastIndex++;
    if (lastIndex >= SEGMENTS_COUNT)
    {
        lastIndex = 0;
    }
    std::cerr << "fmp4.";
}

const HLSSegment * HLSOutput::getSegment(int number)
{
    for (int i = 0; i < SEGMENTS_COUNT; i++)
    {
        if (segments[i].buffer && segments[i].sample && segments[i].number == number)
        {
            return segments + i;
        }
    }
    return NULL;
}

static GstFlowReturn mp4sink_new_sample(GstAppSink *appsink, gpointer user_data)
{
    HLSOutput *hlsOutput = reinterpret_cast<HLSOutput *>(user_data);
    if (GstSample *sample = gst_app_sink_pull_sample(appsink))
    {
        hlsOutput->pushSegment(sample);
    }
    return GST_FLOW_OK;
}

static GstFlowReturn tssink_new_sample(GstAppSink *appsink, gpointer user_data)
{
    if (GstSample *sample = gst_app_sink_pull_sample(appsink)) 
    {
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo mapInfo;
        gst_buffer_map(buffer, &mapInfo, (GstMapFlags)(GST_MAP_READ));
        gst_buffer_unmap(buffer, &mapInfo);
        gst_sample_unref(sample);
    }
    std::cerr << "ts.";
    return GST_FLOW_OK;
}

int main(int argc, char *argv[])
{
    HLSOutput hlsOutput;

    gst_init(&argc, &argv);
    GstElement *pipeline = gst_pipeline_new(NULL);
    GstElement *videotestsrc = gst_element_factory_make("videotestsrc", NULL);
    g_object_set(videotestsrc, "is-live", TRUE, NULL);
    GstElement *timeoverlay = gst_element_factory_make("identity", NULL);
    GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);
    GstElement *h264enc = gst_element_factory_make("vtenc_h264", NULL);
    g_object_set(h264enc, "realtime", TRUE, NULL);
    GstElement *h264parse = gst_element_factory_make("h264parse", NULL);
    GstElement *h264tee = gst_element_factory_make("tee", NULL);
    GstElement *mp4queue = gst_element_factory_make("queue", NULL);
    GstElement *mp4mux = gst_element_factory_make("mp4mux", NULL);
    g_object_set(mp4mux, "faststart", TRUE, "fragment-duration", 200, "streamable", TRUE, NULL);
    GstElement *mp4sink = gst_element_factory_make("appsink", NULL);
    g_object_set(mp4sink, "emit-signals", TRUE, "sync", FALSE, NULL);
    g_signal_connect(mp4sink, "new-sample", G_CALLBACK(mp4sink_new_sample), &hlsOutput);

    // GstElement *tsqueue = gst_element_factory_make("queue", NULL);
    // GstElement *tsmux = gst_element_factory_make("mpegtsmux", NULL);
    // GstElement *tssink = gst_element_factory_make("appsink", NULL);
    // g_object_set(tssink, "emit-signals", TRUE, "sync", FALSE, NULL);
    // g_signal_connect(tssink, "new-sample", G_CALLBACK(tssink_new_sample), NULL);

    gst_bin_add_many(GST_BIN(pipeline), videotestsrc, timeoverlay, videoconvert, h264enc, h264parse, h264tee, NULL);
    gst_element_link_many(videotestsrc, timeoverlay, videoconvert, h264enc, h264parse, h264tee, NULL);
    gst_bin_add_many(GST_BIN(pipeline), mp4queue, mp4mux, mp4sink, NULL);
    gst_pad_link(gst_element_get_request_pad(h264tee, "src_%u"), gst_element_get_static_pad(mp4queue, "sink"));
    gst_element_link_many(mp4queue, mp4mux, mp4sink, NULL);

    // gst_bin_add_many(GST_BIN(pipeline), tsqueue, tsmux, tssink, NULL);
    // gst_pad_link(gst_element_get_request_pad(h264tee, "src_%u"), gst_element_get_static_pad(tsqueue, "sink"));
    // gst_element_link_many(tsqueue, tsmux, tssink, NULL);
    
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    const int port = 8080;
    SoupServer *http_server = soup_server_new(SOUP_SERVER_SERVER_HEADER, "HLS Demo ", NULL);
    soup_server_add_handler(http_server, NULL, [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer)
    {
        set_error_message(msg, SOUP_STATUS_NOT_FOUND);
    }, NULL, NULL);
    soup_server_add_handler(http_server, "/api/hack.ts", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer)
    {
        
    }, NULL, NULL);
    soup_server_add_handler(http_server, "/ui", file_callback, NULL, NULL);
    if (soup_server_listen_all(http_server, port, static_cast<SoupServerListenOptions>(0), 0))
        g_print("server listening on port %d\n", port);
    else
        g_printerr("port %d could not be bound\n", port);
    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, on_message, main_loop);
    gst_object_unref(bus);
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);
    soup_server_disconnect(http_server);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}
