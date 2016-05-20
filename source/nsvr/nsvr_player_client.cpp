#include "nsvr/nsvr_packet_handler.hpp"
#include "nsvr_internal.hpp"
#include "nsvr.hpp"

#include <gst/net/net.h>

namespace nsvr
{

PlayerClient::PlayerClient(const std::string& address, short port)
    : mClockAddress(address)
    , mBaseTime(0)
    , mClockPort(port)
    , mNetClock(nullptr)
{
    if (!connect(address, port))
    {
        NSVR_LOG("Client was unable to connect to server at construction.");
        return;
    }

    sendToServer("nsvr");
}

void PlayerClient::onMessage(const std::string& message)
{
    if (message.empty())
        return;

    Packet packet;

    if (PacketHandler::parse(message, packet))
    {
        if (getMute() != (packet.mute != FALSE))
            setMute(packet.mute != FALSE);

        if (getVolume() != packet.volume)
            setVolume(packet.volume);

        if (mPipeline == nullptr || gst_element_get_base_time(mPipeline) != packet.base)
            mBaseTime = packet.base;

        if (mBaseTime == 0 && queryState() != packet.state)
            setState(packet.state);
    }
}

void PlayerClient::setupClock()
{
    g_return_if_fail(mPipeline != nullptr);

    if (mBaseTime == 0)
        return;

    clearClock();

    if (GstClock *net_clock = gst_net_client_clock_new(nullptr, mClockAddress.c_str(), mClockPort, mBaseTime))
    {
        mNetClock = net_clock;
        gst_clock_set_timeout(mNetClock, 100 * GST_MSECOND);
        
        gst_pipeline_use_clock(GST_PIPELINE(mPipeline), net_clock);
        gst_element_set_start_time(mPipeline, GST_CLOCK_TIME_NONE);
        gst_element_set_base_time(mPipeline, mBaseTime);
    }

    mBaseTime = 0;
}

void PlayerClient::onBeforeUpdate()
{
    g_return_if_fail(mPipeline != nullptr);

    iterate();

    if (mBaseTime != 0)
    {
        if (getState() != GST_STATE_READY)
        {
            if (gst_element_set_state(mPipeline, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE)
                NSVR_LOG("Client failed to put pipeline into READY state for a pending base.")
        }
        else
        {
            NSVR_LOG("Client receieved a new base time.");
            setupClock();
        }
    }
}

void PlayerClient::clearClock()
{
    g_return_if_fail(mPipeline != nullptr);

    if (mNetClock != nullptr)
    {
        gst_object_unref(mNetClock);
        mNetClock = nullptr;
    }

    gst_element_set_base_time(mPipeline, GST_CLOCK_TIME_NONE);
}

void PlayerClient::onBeforeOpen()
{
    sendToServer("nsvr");
}

void PlayerClient::onBeforeClose()
{
    clearClock();
}

}
