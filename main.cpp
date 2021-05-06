#include <iostream>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <gio/gio.h>
#define AUDIO_TYPE char

using namespace std;
using namespace cv;

static vector<vector<AUDIO_TYPE>> data_vec;
static vector<AUDIO_TYPE> data_vec_full;
static bool disp_switch = false;
static cv::Mat labels;
static int cnt;
static int num_frame;

// Seek pipeline to target position
static void
seek_to_time (GstElement *pipeline,
          gint64      time_nanoseconds)
{
  if (!gst_element_seek (pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                         GST_SEEK_TYPE_SET, time_nanoseconds,
                         GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
    g_print ("Seek failed!\n");
  }
}

GstFlowReturn cb_new_sample(GstElement * elt, gpointer gdata)
{
    GstSample *sample;
    GstBuffer *buffer;
    GstMapInfo map;
    vector<AUDIO_TYPE> vec;
    gpointer out_data;

    /* get the sample from appsink */
    g_signal_emit_by_name (elt, "pull-sample", &sample);
    buffer = gst_sample_get_buffer (sample);

    if (sample)
    {
        if (gst_buffer_map (buffer, &map, GST_MAP_READ))
        {
            vec.clear();
            out_data = map.data;

            // Display k-means label
            if(disp_switch == TRUE)
            {
                int kind = labels.at<int32_t>(cnt,0);
#ifdef FLASE
                switch(kind)
                {
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    g_print("%02d ",kind);
                break;
                default:
                    break;
                }
#else
                g_print("%02d ",kind);
#endif
                cnt++;
                if((cnt % 40) == 0)
                    g_print("\n");

            }

            // Store audio frame data
            for (unsigned long i = 0; i < map.size; i++)
            {
                AUDIO_TYPE data0 = ((AUDIO_TYPE*)out_data)[i];
                data_vec_full.push_back(data0);
            }
            num_frame++;
        }
        gst_sample_unref (sample);
        return GST_FLOW_OK;
    }

    if (sample == NULL)
    {
        return GST_FLOW_EOS;
    }
}

// Bus call back function for exit the mainloop
gboolean on_message(GstBus *bus, GstMessage *message, gpointer p)
{
    GMainLoop *mainloop = (GMainLoop*)p;

    (void)bus;
    if ((GST_MESSAGE_TYPE(message) & GST_MESSAGE_EOS))
        g_main_loop_quit(mainloop);

    return TRUE;
}



int main(int argc, char *argv[])
{
    GMainLoop *mainloop;
    GstElement *pipeline;
    GError *error = NULL;
    GstBus *bus;
    GstElement *appsink;
    GstElement *source;

    cnt = 0;
    num_frame = 0;

    // Initialize Gstreamer
    gst_init(&argc, &argv);

    // Initialize Gstreamer main loop
    mainloop = g_main_loop_new(NULL, FALSE);

    // Construct pipeline
    char pipe1[256],pipe2[256],pipe3[256],pipe4[256],pipe5[256];
    sprintf(pipe1, "filesrc name=src location=./sound-20210330-1.m4a ! decodebin ! volume volume=3 ! tee name=t ! queue ! audioconvert ! autoaudiosink");
    sprintf(pipe2, " t. ! queue ! audioconvert ! wavescope ! videoconvert ! autovideosink");
    sprintf(pipe3, " t. ! queue ! audioconvert ! spectrascope ! videoconvert ! autovideosink");
    sprintf(pipe4, " t. ! queue ! audioconvert ! synaescope ! videoconvert ! autovideosink");
    sprintf(pipe5, " t. ! queue ! audioresample ! audio/x-raw, rate=32000 ! audioconvert ! audio/x-raw,format=U8,channels=1 ! appsink name=sink emit-signals=true ");
    strcat(pipe1,pipe2);
    strcat(pipe1,pipe3);
    strcat(pipe1,pipe4);
    strcat(pipe1,pipe5);
    pipeline = gst_parse_launch(pipe1,&error);

    // error check for pipeline
    if (error != NULL) {
        g_print ("could not construct pipeline: %s\n", error->message);
        g_error_free (error);
        exit (-1);
    }

    // Get appsink object from pipeline by name
    appsink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    if (appsink == NULL)
    {
        g_print("appsink is NULL\n");
    }

    // Get filesrc object from pipeline by name
    source = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    if (source == NULL)
    {
        g_print("source is NULL\n");
    }

    // Register 'new-sample' call back function
    g_signal_connect(appsink, "new-sample", G_CALLBACK(cb_new_sample), pipeline);
    gst_object_unref(appsink);

    // Get bus from pipeline
    bus = gst_element_get_bus(pipeline);

    // Register bus call back function
    gst_bus_add_watch(bus, on_message, mainloop);

    // Start 1st play
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(mainloop);

    printf("************************************************************************\n");

    // Calculate audio data matrix size
    printf("num_frame  : %d\n",num_frame);
    int width = data_vec_full.size() / num_frame;
    int height = data_vec_full.size() / width;
    printf("width  : %d\n",width);
    printf("height : %d\n",height);
    vector<AUDIO_TYPE> vec;
    unsigned long vec_cnt = 0;

    // Convert 1D audio data to 2D audio data
    for (int j = 0; j < height; j++)
    {
        vec.clear();
        for (int i = 0; i < width; i++)
        {
            vec.push_back(data_vec_full[vec_cnt]);
            vec_cnt++;

        }
        data_vec.push_back(vec);
    }
    data_vec_full.clear();

    // Convert 2D audio data to matrix (opencv)
    Mat img(height,width,CV_8U);
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            img.at<char>(j,i) = data_vec[j][i];
        }
    }

    // Save audio data matrix
    imwrite("test.bmp",img);


    //Mat img = imread("test.bmp");

    // Convert audio matrix to floating point matrix and normalize
    cv::Mat reshaped_image32f;
    img.convertTo(reshaped_image32f, CV_32FC1, 1.0 / 256);

    /* k-means++を使用 */
    cv::Mat centers {};
    int cluster_number = 20;
    cv::TermCriteria criteria {TermCriteria::EPS + TermCriteria::COUNT, 10, 1.0};
    double compactness;
    cv::theRNG() = 19771228; //クラスタリングの結果を固定する場合

    compactness = cv::kmeans(reshaped_image32f, cluster_number, labels, criteria, 1, cv::KMEANS_RANDOM_CENTERS, centers);
    //compactness = cv::kmeans(reshaped_image32f, cluster_number, labels, criteria, 1, cv::KMEANS_PP_CENTERS, centers);
    printf("compactness : %f\n",compactness);
    printf("cluster_number : %d\n",cluster_number);

    disp_switch = TRUE;
    seek_to_time(pipeline,0);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(mainloop);

    g_print("\n");
    return 0;
}
