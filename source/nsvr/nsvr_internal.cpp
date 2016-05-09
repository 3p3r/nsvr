#include "nsvr_internal.hpp"

#include "nsvr/nsvr_discoverer.hpp"

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

bool Internal::isNullOrEmpty(const gchar* const str)
{
    return str == nullptr || !*str;
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

std::string Internal::processPath(const std::string& path)
{
    if (path.empty())
    {
        g_debug("Cannot process an empty path.");
        return nullptr;
    }

    std::string processed_path;

    // This is NULL if path is already a valid URI
    gchar* uri = g_filename_to_uri(path.c_str(), nullptr, nullptr);

    if (!isNullOrEmpty(uri)) {
        processed_path = uri;
    }
    else {
        processed_path = path;
    }

    return processed_path;
}

}
