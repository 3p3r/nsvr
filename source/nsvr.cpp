#include "nsvr.hpp"
#include "nsvr/nsvr_internal.hpp"

#include <memory>

#ifdef _WIN32
#    include <windows.h>
#    include <psapi.h>
#endif // _WIN32

namespace {

struct DefaultMulticastGroup
{
    DefaultMulticastGroup(const std::string& g, short p)
        : group(g), port(p), enabled(true)
    { /* no-op */ }

    std::string group;
    short       port;
    bool        enabled;
};

std::shared_ptr<DefaultMulticastGroup> getDefaultMulticastGroup()
{
    static std::shared_ptr<DefaultMulticastGroup> instance
        = std::make_shared<DefaultMulticastGroup>("239.0.0.1", 5000);

    return instance;
}

}

namespace nsvr
{

std::string getVersion()
{
#   define STRINGIFO(v) #v
#   define STRINGIFY(v) STRINGIFO(v)

    return
        STRINGIFY(NSVR_VERSION_MAJOR) "."
        STRINGIFY(NSVR_VERSION_MINOR) "."
        STRINGIFY(NSVR_VERSION_PATCH) "_"
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

std::string getDefaultMulticastIp()
{
    static auto handle = getDefaultMulticastGroup();
    return handle->group;
}

void setDefaultMulticastIp(const std::string& group)
{
    static auto handle = getDefaultMulticastGroup();
    handle->group = group;
}

short getDefaultMulticastPort()
{
    static auto handle = getDefaultMulticastGroup();
    return handle->port;
}

void setDefaultMulticastPort(short port)
{
    static auto handle = getDefaultMulticastGroup();
    handle->port = port;
}

bool defaultMulticastGroupEnabled()
{
    static auto handle = getDefaultMulticastGroup();
    return handle->enabled;
}

void enableDefaultMulticastGroup(bool on /*= true*/)
{
    static auto handle = getDefaultMulticastGroup();
    handle->enabled = on;
}

void listModules()
{
#ifdef _WIN32
    NSVR_LOG("******************* START MODULES");

    HMODULE hMods[2056] {0};
    DWORD   cbNeeded    {0};

    if (::EnumProcessModules(
        ::GetCurrentProcess(),
        hMods,
        sizeof(hMods),
        &cbNeeded))
    {
        for (int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
        {
            CHAR szModName[MAX_PATH] {0};
            if (GetModuleFileNameExA(
                ::GetCurrentProcess(),
                hMods[i],
                szModName,
                sizeof(szModName) / sizeof(szModName[0])))
            {
                NSVR_LOG(szModName);
            }
        }
    }

    NSVR_LOG("******************* END MODULES");
#endif
}

}
