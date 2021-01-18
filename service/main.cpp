#include <gst/gst.h>

int main(int argc, char *argv[])
{
    GstElement *pipeline = gst_pipeline_new(NULL);
    gst_object_unref(pipeline);
    return 0;
}
