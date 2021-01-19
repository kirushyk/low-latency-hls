#include <iostream>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

static gboolean on_message(GstBus *bus, GstMessage *message, gpointer user_data)
{
    GMainLoop *main_loop = reinterpret_cast<GMainLoop *>(user_data);
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
    case GST_MESSAGE_ERROR:
        g_main_loop_quit(main_loop);
        break;
    default:
        break; 
    }
    return TRUE;
}

static GstFlowReturn appsink_new_sample(GstAppSink *appsink, gpointer user_data)
{
    if (GstSample *sample = gst_app_sink_pull_sample(appsink)) 
    {
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo mapInfo;
        gst_buffer_map(buffer, &mapInfo, (GstMapFlags)(GST_MAP_READ));
        gst_buffer_unmap(buffer, &mapInfo);
        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    GstElement *pipeline = gst_pipeline_new(NULL);
    GstElement *videotestsrc = gst_element_factory_make("videotestsrc", NULL);
    g_object_set(videotestsrc, "num-buffers", 100, NULL);
    GstElement *timeoverlay = gst_element_factory_make("timeoverlay", NULL);
    GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);
    GstElement *h264enc = gst_element_factory_make("vtenc_h264", NULL);
    GstElement *mp4mux = gst_element_factory_make("mp4mux", NULL);
    g_object_set(mp4mux, "faststart", TRUE, "fragment-duration", 200, NULL);
    GstElement *sink = gst_element_factory_make("appsink", NULL);
    g_object_set(sink, "emit-signals", TRUE, "sync", FALSE, NULL);
    g_signal_connect(sink, "new-sample", G_CALLBACK(appsink_new_sample), NULL);
    gst_bin_add_many(GST_BIN(pipeline), videotestsrc, timeoverlay, videoconvert, h264enc, mp4mux, sink, NULL);
    gst_element_link_many(videotestsrc, timeoverlay, videoconvert,  h264enc, mp4mux, sink, NULL);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, on_message, main_loop);
    gst_object_unref(bus);
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}
