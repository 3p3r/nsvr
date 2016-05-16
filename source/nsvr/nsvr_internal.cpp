#include "nsvr_internal.hpp"

#include "nsvr/nsvr_discoverer.hpp"
#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_peer.hpp"

#include <gst/net/net.h>
#include <gst/gstregistry.h>
#include <gst/app/gstappsink.h>
#include <gst/pbutils/gstdiscoverer.h>

#ifdef _WIN32
#   include <windows.h>
#endif

namespace nsvr {
namespace internal {

template<> BindToScope<gchar>::~BindToScope()                   { g_free(pointer); pointer = nullptr; }
template<> BindToScope<GList>::~BindToScope()                   { gst_discoverer_stream_info_list_free(pointer); pointer = nullptr; }
template<> BindToScope<GError>::~BindToScope()                  { g_error_free(pointer); pointer = nullptr; }
template<> BindToScope<GstClock>::~BindToScope()                { gst_object_unref(pointer); pointer = nullptr; }
template<> BindToScope<GstMessage>::~BindToScope()              { gst_message_unref(pointer); pointer = nullptr; }
template<> BindToScope<GstAppSink>::~BindToScope()              { g_object_unref(pointer); pointer = nullptr; }
template<> BindToScope<GstDiscoverer>::~BindToScope()           { g_object_unref(pointer); pointer = nullptr; }
template<> BindToScope<GstDiscovererInfo>::~BindToScope()       { gst_discoverer_info_unref(pointer); pointer = nullptr; }
template<> BindToScope<GstNetTimeProvider>::~BindToScope()      { gst_object_unref(pointer); pointer = nullptr; }
template<> BindToScope<GstDiscovererStreamInfo>::~BindToScope() { gst_discoverer_stream_info_unref(pointer); pointer = nullptr; }

bool gstreamerInitialized()
{
    GError *init_error = nullptr;
    BIND_TO_SCOPE(init_error);

    if (gst_is_initialized() == FALSE &&
        gst_init_check(nullptr, nullptr, &init_error) == FALSE)
    {
        NSVR_LOG("GStreamer failed to initialize [" << init_error->message << "].");
        return false;
    }
    else
    {
        return true;
    }
}

std::string implode(const std::vector<std::string>& elements, const std::string& glue)
{
    switch (elements.size())
    {
    case 0:
        return "";
    case 1:
        return elements[0];
    default:
        std::ostringstream os;
        std::copy(elements.begin(), elements.end() - 1, std::ostream_iterator<std::string>(os, glue.c_str()));
        os << *elements.rbegin();
        return os.str();
    }
}

std::vector<std::string> explode(const std::string &input, char separator)
{
    std::vector<std::string> ret;
    std::string::const_iterator cur = input.begin();
    std::string::const_iterator beg = input.begin();
    bool added = false;
    while (cur < input.end())
    {
        if (*cur == separator)
        {
            ret.insert(ret.end(), std::string(beg, cur));
            beg = ++cur;
            added = true;
        }
        else
        {
            cur++;
        }
    }

    ret.insert(ret.end(), std::string(beg, cur));
    return ret;
}

std::string pathToUri(const std::string& path)
{
    if (path.empty())
    {
        NSVR_LOG("Cannot process an empty path.");
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

void log(const std::string& msg)
{
    static std::mutex guard;
    std::lock_guard<decltype(guard)> lock(guard);
#ifdef _WIN32
    ::OutputDebugStringA(msg.c_str());
#endif
    std::cout << msg;
}

}}
