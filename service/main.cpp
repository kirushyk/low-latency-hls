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

#define APPLICATION_NAME "HLS Demo"
#define PLAYLIST_CHUNK_SIZE 4096

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

static void rtspsrc_pad_added(GstElement *src, GstPad *new_pad, GstElement *rtph264depay)
{
    GstPad *sinkPad = gst_element_get_static_pad(rtph264depay, "sink");

    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;
    const gchar *new_pad_media = NULL;

    g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    /* Check the new pad's type */
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    new_pad_media = gst_structure_get_string(new_pad_struct, "media");
    if (!g_str_has_prefix(new_pad_type, "application/x-rtp")
        || !g_str_equal(new_pad_media, "video"))
    {
        g_print("It has type '%s' which is not RTP stream OR media '%s' which is not video. "
                "Ignoring.\n",
                new_pad_type, new_pad_media);
        goto exit;
    }

    /* Attempt the link */
    ret = gst_pad_link(new_pad, sinkPad);
    if (GST_PAD_LINK_FAILED(ret))
    {
        g_print("Type is '%s' but link failed.\n", new_pad_type);
    }

exit:
    /* Unreference the new pad's caps, if we got them */
    if (new_pad_caps != NULL)
    {
        gst_caps_unref(new_pad_caps);
    }
}


int main(int argc, char *argv[])
{
    HLSOutput hlsOutput;

    if (argc != 2)
    {
        std::cerr << "usage:\n\t" << argv[0] << " rtsp://..." << std::endl;
        return 0;
    }

    gst_init(&argc, &argv);
    GstElement *pipeline = gst_pipeline_new(NULL);
    // GstElement *videotestsrc = gst_element_factory_make("videotestsrc", NULL);
    // g_object_set(videotestsrc, "is-live", TRUE, "do-timestamp", TRUE, "pattern", 18, NULL);
    // GstElement *timeoverlay = gst_element_factory_make("identity", NULL);
    // GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);
    // GstElement *h264enc = gst_element_factory_make("vtenc_h264", NULL);
    // GstElement *h264enc = gst_element_factory_make("x264enc", NULL);
    // g_object_set(h264enc, "key-int-max", 30, NULL);
    // g_object_set(h264enc, "max-keyframe-interval", 30, NULL);
    GstElement *rtspsrc = gst_element_factory_make("rtspsrc", NULL);
    g_object_set(rtspsrc, "location", argv[1], "user-agent", APPLICATION_NAME, NULL);
    GstElement *rtph264depay = gst_element_factory_make("rtph264depay", NULL);
    g_signal_connect(rtspsrc, "pad-added", G_CALLBACK(rtspsrc_pad_added), rtph264depay);
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

    gst_bin_add_many(GST_BIN(pipeline), rtspsrc, rtph264depay, /*videotestsrc, timeoverlay, videoconvert, h264enc,*/ h264parse, h264tee, NULL);
    gst_element_link_many(rtph264depay, /*videotestsrc, timeoverlay, videoconvert, h264enc,*/ h264parse, h264tee, NULL);
    gst_bin_add_many(GST_BIN(pipeline), tsqueue, tsmux, tsparse, tssink, NULL);
    gst_pad_link(gst_element_get_request_pad(h264tee, "src_%u"), gst_element_get_static_pad(tsqueue, "sink"));
    gst_element_link_many(tsqueue, tsmux, tsparse, tssink, NULL);
    
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    const int port = 8080;
    SoupServer *http_server = soup_server_new(SOUP_SERVER_SERVER_HEADER, APPLICATION_NAME, NULL);
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
    soup_server_add_handler(http_server, "/api/partial/", [](SoupServer *, SoupMessage *msg, const char *path, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        int segmentNumber = 0;
        int partialSegmentNumber = 0;
        sscanf(path + 13, "%d.%d.ts", &segmentNumber, &partialSegmentNumber);
        HLSOutput *hlsOutput = reinterpret_cast<HLSOutput *>(user_data);
        std::shared_ptr<HLSSegment> segment = hlsOutput->getSegment(segmentNumber);
        soup_message_headers_append(msg->response_headers, "Cache-Control", "no-cache, no-store, must-revalidate");
        soup_message_headers_append(msg->response_headers, "Pragma", "no-cache");
        if (segment)
        {
            std::shared_ptr<HLSPartialSegment> partialSegment = segment->getPartialSegment(partialSegmentNumber);
            if (partialSegment)
            {
                soup_message_headers_append(msg->response_headers, "Content-Type", "video/mp2t");
                soup_message_set_status(msg, SOUP_STATUS_OK);
                for (const auto &b: partialSegment->data)
                {
                    soup_message_body_append(msg->response_body, SOUP_MEMORY_COPY, (gchar *)b.data(), b.size());
                }
                std::cerr << "" << path << ", sent " << partialSegment->data.size() << " chunks" << std::endl;
                soup_message_body_complete(msg->response_body);
            }
            else
            {
                set_error_message(msg, SOUP_STATUS_NOT_FOUND);
            }
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
        char chunk[PLAYLIST_CHUNK_SIZE];
        deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
        do
        {
            zs.avail_out = (uInt)PLAYLIST_CHUNK_SIZE;
            zs.next_out = (Bytef *)chunk;
            deflate(&zs, Z_FINISH);
            int have = PLAYLIST_CHUNK_SIZE - zs.avail_out;
            soup_message_body_append(msg->response_body, SOUP_MEMORY_COPY, chunk, have);
        }
        while (zs.avail_out == 0);
        deflateEnd(&zs);
        soup_message_set_status(msg, SOUP_STATUS_OK);
	    soup_message_body_complete(msg->response_body);
    }, &hlsOutput, NULL);
    soup_server_add_handler(http_server, "/api/lhls.m3u8", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        HLSOutput *hlsOutput = reinterpret_cast<HLSOutput *>(user_data);
        std::string playlist = hlsOutput->getLowLatencyPlaylist();
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
        char chunk[PLAYLIST_CHUNK_SIZE];
        deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
        do
        {
            zs.avail_out = (uInt)PLAYLIST_CHUNK_SIZE;
            zs.next_out = (Bytef *)chunk;
            deflate(&zs, Z_FINISH);
            int have = PLAYLIST_CHUNK_SIZE - zs.avail_out;
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
