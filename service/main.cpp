#include <gst/gst.h>

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    GstElement *pipeline = gst_pipeline_new(NULL);
    GstElement *videotestsrc = gst_element_factory_make("videotestsrc", NULL);
    GstElement *timeoverlay = gst_element_factory_make("timeoverlay", NULL);
    GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);
    GstElement *autovideosink = gst_element_factory_make("autovideosink", NULL);
    gst_bin_add_many(GST_BIN(pipeline), videotestsrc, timeoverlay, videoconvert, autovideosink, NULL);
    gst_element_link_many(videotestsrc, timeoverlay, videoconvert, autovideosink, NULL);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);
    gst_object_unref(pipeline);
    return 0;
}
