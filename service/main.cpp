#include <cstdint>
#include <sstream>
#include <memory>
#include <list>
#include <iostream>
#include <glib.h>
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
    GDateTime *dateTime;
    GstBuffer *buffer;
    GstMapInfo mapInfo;
    GstSample *sample;
    static GTimeZone *timeZone;

    HLSSegment();
    ~HLSSegment();
};

GTimeZone * HLSSegment::timeZone = NULL;

HLSSegment::HLSSegment()
{
    if (timeZone == NULL)
    {
        timeZone = g_time_zone_new_utc();
    }
    dateTime = g_date_time_new_now(timeZone);
}

HLSSegment::~HLSSegment()
{
    if (buffer)
        gst_buffer_unmap(buffer, &mapInfo);
    if (sample)
        gst_sample_unref(sample);
    if (dateTime)
    {
        g_date_time_unref(dateTime);
    }
}

#define SEGMENTS_COUNT 5

struct HLSOutput
{
    int lastIndex, lastSegmentNumber;
    std::list<std::shared_ptr<HLSSegment>> segments;
public:
    HLSOutput();
    void pushSegment(GstSample *sample);
    std::shared_ptr<HLSSegment> getSegment(int number) const;
    std::string getPlaylist() const;
};

HLSOutput::HLSOutput()
{
    lastIndex = 0;
    lastSegmentNumber = 0;
}

void HLSOutput::pushSegment(GstSample *sample)
{
    std::shared_ptr<HLSSegment> segment = std::make_shared<HLSSegment>();

    segment->sample = sample;
    segment->buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(segment->buffer, &segment->mapInfo, (GstMapFlags)(GST_MAP_READ));
    segment->number = lastSegmentNumber;

    segments.push_back(segment);
    
    lastSegmentNumber++;

    if (segments.size() > SEGMENTS_COUNT)
    {
        segments.pop_front();
    }

    std::cerr << "fmp4.";
}

std::shared_ptr<HLSSegment> HLSOutput::getSegment(int number) const
{
    for (const auto& segment: segments)
    {
        if (segment->number == number)
        {
            return segment;
        }
    }
    return NULL;
}

std::string HLSOutput::getPlaylist() const
{
    std::stringstream ss;
    ss << "#EXTM3U" << std::endl;
    ss << "#EXT-X-TARGETDURATION:4" << std::endl;
    ss << "#EXT-X-VERSION:6" << std::endl;
    ss << "#EXT-X-MEDIA-SEQUENCE:" << lastSegmentNumber << std::endl;
    // gchar *g_date_time_format_iso8601 (GDateTime *datetime);
    // ss << "#EXT-X-PROGRAM-DATE-TIME:2019-02-14T02:13:36.106Z" << std::endl;
    for (const auto& segment: segments)
    {
        if (segment->buffer)
        {
            ss << "#EXTINF:4," << std::endl;
            ss << "segments/" << segment->number << ".mp4" << std::endl;
        }
    }
    return ss.str();
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

// static GstFlowReturn tssink_new_sample(GstAppSink *appsink, gpointer user_data)
// {
//     if (GstSample *sample = gst_app_sink_pull_sample(appsink)) 
//     {
//         GstBuffer *buffer = gst_sample_get_buffer(sample);
//         GstMapInfo mapInfo;
//         gst_buffer_map(buffer, &mapInfo, (GstMapFlags)(GST_MAP_READ));
//         gst_buffer_unmap(buffer, &mapInfo);
//         gst_sample_unref(sample);
//     }
//     std::cerr << "ts.";
//     return GST_FLOW_OK;
// }

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
    g_object_set(mp4mux, "faststart", TRUE, "fragment-duration", 4000, "streamable", TRUE, NULL);
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
        set_error_message(msg, SOUP_STATUS_NOT_FOUND);
    }, NULL, NULL);
    soup_server_add_handler(http_server, "/api/plain.m3u8", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        HLSOutput *hlsOutput = reinterpret_cast<HLSOutput *>(user_data);
        std::string playlist = hlsOutput->getPlaylist();
        soup_message_set_response(msg, "vnd.apple.mpegURL", SOUP_MEMORY_COPY, playlist.c_str(), playlist.size());
        soup_message_set_status(msg, SOUP_STATUS_OK);
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
