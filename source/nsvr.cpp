#include "nsvr.hpp"
#include "nsvr/nsvr_internal.hpp"

namespace nsvr
{

std::string getVersion()
{
#   define STRINGIFO(v) #v
#   define STRINGIFY(v) STRINGIFO v

    return "1.0.0" "_"
        STRINGIFY(GST_VERSION_MAJOR) "."
        STRINGIFY(GST_VERSION_MINOR) "."
        STRINGIFY(GST_VERSION_MICRO) "."
        STRINGIFY(GST_VERSION_NANO);
}

void addPluginPath(const std::string& path)
{
    if (path.empty())
    {
        NSVR_LOG("Plug-in path supplied is empty.");
        return;
    }

    if (!internal::gstreamerInitialized())
    {
        NSVR_LOG("GStreamer could not be initialized.");
        return;
    }

    if (GstRegistry *registry = gst_registry_get())
    {
        gst_registry_scan_path(registry, path.c_str());
    }
}

}
