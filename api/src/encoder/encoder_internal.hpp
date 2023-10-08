#pragma once
#include <vector>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <iostream>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "media_library/encoder.hpp"

struct InputParams
{
    std::string format;
    uint32_t width;
    uint32_t height;
    uint32_t framerate;
};
class MediaLibraryEncoder::Impl final
{
private:
    InputParams m_input_params;
    std::shared_ptr<std::mutex> m_mutex;
    std::vector<AppWrapperCallback> m_callbacks;
    std::queue<GstBuffer *> m_queue;
    GstAppSrc *m_appsrc;
    GMainLoop *m_main_loop;
    std::shared_ptr<std::thread> m_main_loop_thread;
    GstElement *m_pipeline;
    guint m_send_buffer_id;
    std::string m_json_config; //this should be const

public:
    static tl::expected<std::shared_ptr<MediaLibraryEncoder::Impl>, media_library_return> create(std::string json_config);

    // static tl::expected<MediaLibraryEncoder::Impl, media_library_return> create(nlohmann::json encoder_config);
    ~Impl();
    Impl(std::string json_config, media_library_return &status);
    // Impl(nlohmann::json encoder_config, media_library_return &status);

public:
    media_library_return subscribe(AppWrapperCallback callback);
    media_library_return start();
    media_library_return stop();
    media_library_return add_buffer(HailoMediaLibraryBufferPtr ptr);
    void add_gst_buffer(GstBuffer *buffer);
    /**
     * Below are public functions that are not part of the public API
     * but are public for gstreamer callbacks.
     */
public:
    void on_need_data(GstAppSrc *appsrc, guint size);
    void on_enough_data(GstAppSrc *appsrc);
    void on_fps_measurement(GstElement *fpssink, gdouble fps, gdouble droprate, gdouble avgfps);
    GstFlowReturn on_new_sample(GstAppSink *appsink);
    gboolean on_idle_callback();
    gboolean on_bus_call(GstBus *bus, GstMessage *msg);

private:
    static void need_data(GstAppSrc *appsrc, guint size, gpointer user_data)
    {
        MediaLibraryEncoder::Impl *encoder = static_cast<MediaLibraryEncoder::Impl *>(user_data);
        encoder->on_need_data(appsrc, size);
    }
    static void enough_data(GstAppSrc *appsrc, gpointer user_data)
    {
        MediaLibraryEncoder::Impl *encoder = static_cast<MediaLibraryEncoder::Impl *>(user_data);
        encoder->on_enough_data(appsrc);
    }
    static void fps_measurement(GstElement *fpssink, gdouble fps, gdouble droprate,
                                gdouble avgfps, gpointer user_data)
    {
        MediaLibraryEncoder::Impl *encoder = static_cast<MediaLibraryEncoder::Impl *>(user_data);
        encoder->on_fps_measurement(fpssink, fps, droprate, avgfps);
    }
    static GstFlowReturn new_sample(GstAppSink *appsink, gpointer user_data)
    {
        MediaLibraryEncoder::Impl *encoder = static_cast<MediaLibraryEncoder::Impl *>(user_data);
        return encoder->on_new_sample(appsink);
    }
    static gboolean idle_callback(gpointer user_data)
    {
        MediaLibraryEncoder::Impl *encoder = static_cast<MediaLibraryEncoder::Impl *>(user_data);
        return encoder->send_buffer();
    }
    static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer user_data)
    {
        MediaLibraryEncoder::Impl *encoder = static_cast<MediaLibraryEncoder::Impl *>(user_data);
        return encoder->on_bus_call(bus, msg);
    }

private:
    void set_gst_callbacks(GstElement *pipeline);
    void add_buffer_internal(GstBuffer *buffer);
    gboolean send_buffer();
    gboolean push_buffer(GstBuffer *buffer);
    GstBuffer *dequeue_buffer();
    std::string create_pipeline_string(nlohmann::json osd_json_config);
};