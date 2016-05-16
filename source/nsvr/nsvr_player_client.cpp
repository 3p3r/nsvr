#include "nsvr_internal.hpp"
#include "nsvr/nsvr_player_client.hpp"

#include <gst/net/net.h>

namespace nsvr
{

PlayerClient::PlayerClient(const std::string& address, short port)
    : mClockAddress(address)
    , mBaseTime(0)
    , mClockPort(port)
    , mNetClock(nullptr)
{
    connect("239.0.0.1", 5000);
}

void PlayerClient::onMessage(const std::string& message)
{
    if (message.empty() || message[0] == 'c')
        return;

    if (message.size() > 1 && message[0] == 's')
    {
        // server heartbeat received
        if (message[1] == 'h')
        {
            gdouble time        = 0;
            gdouble volume      = 0;
            bool mute           = true;
            GstState state      = GST_STATE_NULL;
            GstClockTime base   = 0;

            for (const auto& cmd : internal::explode(message.substr(2), '|'))
            {
                if (cmd[0] == 't')
                    time = std::stod(cmd.substr(1));
                else if (cmd[0] == 'v')
                    volume = std::stod(cmd.substr(1));
                else if (cmd[0] == 'm')
                    mute = (std::stoi(cmd.substr(1)) != FALSE);
                else if (cmd[0] == 's')
                    state = static_cast<GstState>(std::stoi(cmd.substr(1)));
                else if (cmd[0] == 'b')
                    base = std::stoull(cmd.substr(1));
            }

            if (getMute() != mute)
                setMute(mute);

            if (getVolume() != volume)
                setVolume(volume);
            
            if (gst_element_get_base_time(mPipeline) != base)
                mBaseTime = base;

            if (mBaseTime == 0 && getState() != state)
                setState(state);
        }
    }
}

void PlayerClient::setupClock()
{
    if (mBaseTime == 0)
        return;

    clearClock();

    if (GstClock *net_clock = gst_net_client_clock_new(nullptr, mClockAddress.c_str(), mClockPort, mBaseTime))
    {
        mNetClock = net_clock;
        
        gst_pipeline_use_clock(GST_PIPELINE(mPipeline), net_clock);
        gst_element_set_start_time(mPipeline, GST_CLOCK_TIME_NONE);
        gst_element_set_base_time(mPipeline, mBaseTime);
    }

    mBaseTime = 0;
}

void PlayerClient::onBeforeUpdate()
{
    iterate();

    if (mPipeline != nullptr &&
        mBaseTime != 0)
    {
        if (getState() != GST_STATE_READY)
            stop();
        else
            setupClock();
    }
}

void PlayerClient::clearClock()
{
    if (mNetClock != nullptr)
    {
        gst_object_unref(mNetClock);
        mNetClock = nullptr;
    }

    gst_element_set_base_time(mPipeline, GST_CLOCK_TIME_NONE);
}

void PlayerClient::onBeforeClose()
{
    clearClock();
}

}
