#include "config.h"
#include "rtsp-input.hpp"
#include <gst/app/gstappsink.h>

RTSPInput::Delegate::~Delegate()
{

}


static gboolean on_message(GstBus *bus, GstMessage *message, gpointer user_data)
{
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
    case GST_MESSAGE_ERROR:
        break;
    default:
        break; 
    }
    return TRUE;
}

static GstFlowReturn tssink_new_sample(GstAppSink *appsink, gpointer user_data)
{
    RTSPInput *rtspInput = reinterpret_cast<RTSPInput *>(user_data);
    if (GstSample *sample = gst_app_sink_pull_sample(appsink))
    {
        if (auto delegate = rtspInput->delegate.lock())
        {
            delegate->onSample(sample);
        }
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

RTSPInput::RTSPInput(const char *url)
{
    gst_init(NULL, NULL);
    pipeline = gst_pipeline_new(NULL);
    
    GstElement *rtspsrc = gst_element_factory_make("rtspsrc", NULL);
    g_object_set(rtspsrc, "location", url, "user-agent", APPLICATION_NAME, NULL);
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
    g_signal_connect(tssink, "new-sample", G_CALLBACK(tssink_new_sample), this);

    gst_bin_add_many(GST_BIN(pipeline), rtspsrc, rtph264depay, h264parse, h264tee, NULL);
    gst_element_link_many(rtph264depay, h264parse, h264tee, NULL);
    gst_bin_add_many(GST_BIN(pipeline), tsqueue, tsmux, tsparse, tssink, NULL);
    gst_pad_link(gst_element_get_request_pad(h264tee, "src_%u"), gst_element_get_static_pad(tsqueue, "sink"));
    gst_element_link_many(tsqueue, tsmux, tsparse, tssink, NULL);
    
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, on_message, this);
    gst_object_unref(bus);
}

RTSPInput::~RTSPInput()
{
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}
