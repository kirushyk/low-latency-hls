#include <cstdint>
#include <cstdio>
#include <sstream>
#include <memory>
#include <list>
#include <iostream>
#include <glib.h>
#include <libsoup/soup.h>
#include <gst/gst.h>
#include <zlib.h>
#include <gst/app/gstappsink.h>
#include "file-endpoint.hpp"
#include "hls.hpp"

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

static GstFlowReturn tssink_new_sample(GstAppSink *appsink, gpointer user_data)
{
    HLSOutput *hlsOutput = reinterpret_cast<HLSOutput *>(user_data);
    if (GstSample *sample = gst_app_sink_pull_sample(appsink))
    {
        hlsOutput->pushSample(sample);
    }
    return GST_FLOW_OK;
}

int main(int argc, char *argv[])
{
    HLSOutput hlsOutput;

    gst_init(&argc, &argv);
    GstElement *pipeline = gst_pipeline_new(NULL);
    GstElement *videotestsrc = gst_element_factory_make("videotestsrc", NULL);
    g_object_set(videotestsrc, "is-live", TRUE, "do-timestamp", TRUE, "pattern", 18, NULL);
    GstElement *timeoverlay = gst_element_factory_make("identity", NULL);
    GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);
    GstElement *h264enc = gst_element_factory_make("vtenc_h264", NULL);
    // GstElement *h264enc = gst_element_factory_make("x264enc", NULL);
    // g_object_set(h264enc, "key-int-max", 30, NULL);
    g_object_set(h264enc, "max-keyframe-interval", 30, NULL);
    GstElement *h264parse = gst_element_factory_make("h264parse", NULL);
    g_object_set(h264parse, "config-interval", -1, NULL);
    GstElement *h264tee = gst_element_factory_make("tee", NULL);
    GstElement *tsqueue = gst_element_factory_make("queue", NULL);
    GstElement *tsmux = gst_element_factory_make("mpegtsmux", NULL);
    GstElement *tsparse = gst_element_factory_make("tsparse", NULL);
    g_object_set(tsparse, "set-timestamps", TRUE, "split-on-rai", TRUE, NULL);
    
    GstElement *tssink = gst_element_factory_make("appsink", NULL);
    g_object_set(tssink, "emit-signals", TRUE, "sync", FALSE, NULL);
    g_signal_connect(tssink, "new-sample", G_CALLBACK(tssink_new_sample), &hlsOutput);

    gst_bin_add_many(GST_BIN(pipeline), videotestsrc, timeoverlay, videoconvert, h264enc, h264parse, h264tee, NULL);
    gst_element_link_many(videotestsrc, timeoverlay, videoconvert, h264enc, h264parse, h264tee, NULL);
    gst_bin_add_many(GST_BIN(pipeline), tsqueue, tsmux, tsparse, tssink, NULL);
    gst_pad_link(gst_element_get_request_pad(h264tee, "src_%u"), gst_element_get_static_pad(tsqueue, "sink"));
    gst_element_link_many(tsqueue, tsmux, tsparse, tssink, NULL);
    
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    const int port = 8080;
    SoupServer *http_server = soup_server_new(SOUP_SERVER_SERVER_HEADER, "HLS Demo ", NULL);
    soup_server_add_handler(http_server, NULL, [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer)
    {
        set_error_message(msg, SOUP_STATUS_NOT_FOUND);
    }, NULL, NULL);
    soup_server_add_handler(http_server, "/api/hack.ts", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer)
    {
        set_error_message(msg, SOUP_STATUS_NOT_FOUND);
    }, NULL, NULL);
    soup_server_add_handler(http_server, "/api/segments/", [](SoupServer *, SoupMessage *msg, const char *path, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        int segmentNumber = 0;
        sscanf(path + 14, "%d.ts", &segmentNumber);
        HLSOutput *hlsOutput = reinterpret_cast<HLSOutput *>(user_data);
        std::shared_ptr<HLSSegment> segment = hlsOutput->getSegment(segmentNumber);
        soup_message_headers_append(msg->response_headers, "Cache-Control", "no-cache, no-store, must-revalidate");
        soup_message_headers_append(msg->response_headers, "Pragma", "no-cache");
        if (segment)
        {
            soup_message_headers_append(msg->response_headers, "Content-Type", "video/mp2t");
            soup_message_set_status(msg, SOUP_STATUS_OK);
            for (const auto &b: segment->data)
            {
                soup_message_body_append(msg->response_body, SOUP_MEMORY_COPY, (gchar *)b.data(), b.size());
            }
            soup_message_body_complete(msg->response_body);
        }
        else
        {
            set_error_message(msg, SOUP_STATUS_NOT_FOUND);
        }
    }, &hlsOutput, NULL);
    soup_server_add_handler(http_server, "/api/plain.m3u8", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        HLSOutput *hlsOutput = reinterpret_cast<HLSOutput *>(user_data);
        std::string playlist = hlsOutput->getPlaylist();
        soup_message_headers_append(msg->response_headers, "Cache-Control", "no-cache, no-store, must-revalidate");
        soup_message_headers_append(msg->response_headers, "Pragma", "no-cache");
        soup_message_headers_append(msg->response_headers, "Content-Type", "application/vnd.apple.mpegURL");
        soup_message_headers_append(msg->response_headers, "Content-Encoding", "gzip");

        z_stream zs;
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = (uInt)playlist.size();
        zs.next_in = (Bytef *)playlist.c_str();
        const int CHUNK_SIZE = 4096;
        char chunk[CHUNK_SIZE];
        deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
        do
        {
            zs.avail_out = (uInt)CHUNK_SIZE;
            zs.next_out = (Bytef *)chunk;
            deflate(&zs, Z_FINISH);
            int have = CHUNK_SIZE - zs.avail_out;
            soup_message_body_append(msg->response_body, SOUP_MEMORY_COPY, chunk, have);
        }
        while (zs.avail_out == 0);
        deflateEnd(&zs);
        soup_message_set_status(msg, SOUP_STATUS_OK);
	    soup_message_body_complete(msg->response_body);
    }, &hlsOutput, NULL);
    soup_server_add_handler(http_server, "/ui", file_callback, NULL, NULL);
    soup_server_add_handler(http_server, "/", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer)
    {
        const char *phrase = soup_status_get_phrase(SOUP_STATUS_MOVED_PERMANENTLY);
        soup_message_headers_append(msg->response_headers, "Location", "/ui");
        soup_message_set_response(msg, "text/plain", SOUP_MEMORY_STATIC, phrase, strlen(phrase));
        soup_message_set_status_full(msg, SOUP_STATUS_MOVED_PERMANENTLY, phrase);
    }, NULL, NULL);
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
