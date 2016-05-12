#include "nsvr_internal.hpp"

#include "nsvr/nsvr_discoverer.hpp"
#include "nsvr/nsvr_base.hpp"
#include "nsvr/nsvr_peer.hpp"

namespace {
struct ScopedSocketRef
{
    ScopedSocketRef(GSocket *socket) : mSocket(socket) { g_object_ref(mSocket); }
    ~ScopedSocketRef() { g_object_unref(mSocket); }
    GSocket *mSocket;
};
}

namespace nsvr
{

template<> BindToScope<gchar>::~BindToScope()                   { g_free(pointer); pointer = nullptr; }
template<> BindToScope<GList>::~BindToScope()                   { gst_discoverer_stream_info_list_free(pointer); pointer = nullptr; }
template<> BindToScope<GError>::~BindToScope()                  { g_error_free(pointer); pointer = nullptr; }
template<> BindToScope<GstMessage>::~BindToScope()              { gst_message_unref(pointer); pointer = nullptr; }
template<> BindToScope<GstAppSink>::~BindToScope()              { g_object_unref(pointer); pointer = nullptr; }
template<> BindToScope<GstDiscoverer>::~BindToScope()           { g_object_unref(pointer); pointer = nullptr; }
template<> BindToScope<GstDiscovererInfo>::~BindToScope()       { gst_discoverer_info_unref(pointer); pointer = nullptr; }
template<> BindToScope<GstDiscovererStreamInfo>::~BindToScope() { gst_discoverer_stream_info_unref(pointer); pointer = nullptr; }

bool Internal::gstreamerInitialized()
{
    GError *init_error = nullptr;
    BIND_TO_SCOPE(init_error);

    if (gst_is_initialized() == FALSE &&
        gst_init_check(nullptr, nullptr, &init_error) == FALSE) {
        g_debug("GStreamer failed to initialize: %s.", scoped_init_error.pointer->message);
        return false;
    } else {
        return true;
    }
}

void Internal::reset(Discoverer& discoverer)
{
    discoverer.mMediaUri    = "";
    discoverer.mWidth       = 0;
    discoverer.mHeight      = 0;
    discoverer.mFrameRate   = 0;
    discoverer.mHasAudio    = false;
    discoverer.mHasVideo    = false;
    discoverer.mSeekable    = false;
    discoverer.mDuration    = 0;
    discoverer.mSampleRate  = 0;
    discoverer.mBitRate     = 0;
}

void Internal::reset(Player& player)
{
    player.mState         = GST_STATE_NULL;
    player.mPipeline      = nullptr;
    player.mGstBus        = nullptr;
    player.mCurrentBuffer = nullptr;
    player.mCurrentSample = nullptr;
    player.mWidth         = 0;
    player.mHeight        = 0;
    player.mDuration      = 0;
    player.mTime          = 0.;
    player.mVolume        = 1.;
    player.mRate          = 1.;
    player.mPendingSeek   = 0.;
    player.mSeekingLock   = false;
    player.mBufferDirty   = false;
}

GstFlowReturn Internal::onPreroll(GstElement* appsink, Player* player)
{
    GstSample* sample = gst_app_sink_pull_preroll(GST_APP_SINK(appsink));

    // Here's our chance to get the actual dimension of the media.
    // The actual dimension might be slightly different from what
    // is passed into and requested from the pipeline.
    if (sample != nullptr)
    {
        if (GstCaps *caps = gst_sample_get_caps(sample))
        {
            if (gst_caps_is_fixed(caps) != FALSE)
            {
                if (const GstStructure *str = gst_caps_get_structure(caps, 0))
                {
                    if (gst_structure_get_int(str, "width", &player->mWidth) == FALSE ||
                        gst_structure_get_int(str, "height", &player->mHeight) == FALSE)
                    {
                        player->onError("No width/height information available.");
                    }
                }
            }
            else
            {
                player->onError("caps is not fixed for this media.");
            }
        }
    }

    processSample(player, sample);
    return GST_FLOW_OK;
}

GstFlowReturn Internal::onSampled(GstElement* appsink, Player* player)
{
    processSample(player, gst_app_sink_pull_sample(GST_APP_SINK(appsink)));
    return GST_FLOW_OK;
}

void Internal::processSample(Player *const player, GstSample* const sample)
{
    // Check if UI thread has consumed the last frame
    if (player->mBufferDirty)
    {
        // Simply, skip this sample. UI is not consuming fast enough.
        gst_sample_unref(sample);
    }
    else
    {
        // Acquire and hold onto the new frame (until UI consumes it)
        player->mCurrentSample = sample;
        player->mCurrentBuffer = gst_sample_get_buffer(sample);
        gst_buffer_map(player->mCurrentBuffer, &player->mCurrentMapInfo, GST_MAP_READ);

        // Signal UI thread it can consume
        player->mBufferDirty = true;
    }
}

void Internal::processDuration(Player& player)
{
    g_return_if_fail(player.mPipeline != nullptr);

    // Nanoseconds
    gint64 duration_ns = 0;
    if (gst_element_query_duration(GST_ELEMENT(player.mPipeline), GST_FORMAT_TIME, &duration_ns) != FALSE) {
        // Seconds
        player.mDuration = duration_ns / gdouble(GST_SECOND);
    }
}

gboolean Internal::onSocketDataAvailable(GSocket *socket, GIOCondition condition, gpointer user_data)
{
    ScopedSocketRef ss(socket);
    gchar buffer[1024] = { 0 };
    g_socket_receive(socket, buffer, sizeof(buffer) / sizeof(buffer[0]), NULL, NULL);
    if (auto peer = static_cast<nsvr::Peer*>(user_data)) peer->onMessage(buffer);
    return G_SOURCE_CONTINUE;
}

std::string Internal::processPath(const std::string& path)
{
    if (path.empty())
    {
        g_debug("Cannot process an empty path.");
        return "";
    }

    std::string processed_path;

    // This is NULL if path is already a valid URI
    gchar* uri = g_filename_to_uri(path.c_str(), nullptr, nullptr);

    if (!(uri == nullptr || !*uri)) {
        processed_path = uri;
    }
    else {
        processed_path = path;
    }

    return processed_path;
}

}
