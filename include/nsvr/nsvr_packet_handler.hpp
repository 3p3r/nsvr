#pragma once

#include <gst/player/player.h>
#include <string>

namespace nsvr
{

struct Packet
{
    gdouble         time    = 0;
    gdouble         volume  = 0;
    gboolean        mute    = FALSE;
    GstPlayerState  state   = GST_PLAYER_STATE_STOPPED;
    GstClockTime    base    = GST_CLOCK_TIME_NONE;
};

class PacketHandler
{
public:
    static bool parse(const std::string& buffer, Packet& packet);
    static std::string serialize(const Packet& packet);
};

}
