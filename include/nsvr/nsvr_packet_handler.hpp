#pragma once

#include <gst/gst.h>
#include <string>

namespace nsvr
{

struct Packet
{
    gdouble         time    = 0;
    gdouble         volume  = 0;
    gboolean        mute    = FALSE;
    GstState        state   = GST_STATE_NULL;
    GstClockTime    base    = GST_CLOCK_TIME_NONE;
};

class PacketHandler
{
public:
    static bool parse(const std::string& buffer, Packet& packet);
    static std::string serialize(const Packet& packet);
};

}
