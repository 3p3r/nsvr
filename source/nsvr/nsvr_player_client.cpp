#include "nsvr/nsvr_packet_handler.hpp"
#include "nsvr_internal.hpp"
#include "nsvr.hpp"

#include <gst/net/net.h>

namespace nsvr
{

PlayerClient::PlayerClient(const std::string& address, short port)
    : mBaseTime(0)
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

        if (mGstPipeline == nullptr || gst_element_get_base_time(mGstPipeline) != packet.base)
            mBaseTime = packet.base;

        if (mBaseTime == 0 && getState() != packet.state)
            setState(packet.state);
    }
}

void PlayerClient::onClockSetup()
{
    g_return_if_fail(mGstPipeline != nullptr);

    if (mBaseTime == 0)
        return;

    if (GstClock *net_clock = gst_net_client_clock_new(nullptr, getServerAddress().c_str(), internal::getClockPort(getServerPort()), mBaseTime))
    {
        mNetClock = net_clock;
        gst_clock_set_timeout(mNetClock, 100 * GST_MSECOND);
        
        gst_pipeline_use_clock(GST_PIPELINE(mGstPipeline), net_clock);
        gst_element_set_start_time(mGstPipeline, GST_CLOCK_TIME_NONE);
        gst_element_set_base_time(mGstPipeline, mBaseTime);
    }

    mBaseTime = 0;
}

void PlayerClient::onUpdate()
{
    g_return_if_fail(mGstPipeline != nullptr);

    iterate();

    if (mBaseTime != 0)
    {
        if (getState() != GST_PLAYER_STATE_STOPPED)
        {
            stop();
        }
        else
        {
            NSVR_LOG("Client receieved a new base time.");
            onClockClear();
            onClockSetup();
        }
    }
}

void PlayerClient::onClockClear()
{
    g_return_if_fail(mGstPipeline != nullptr);

    if (mNetClock != nullptr)
    {
        gst_object_unref(mNetClock);
        mNetClock = nullptr;
    }

    gst_element_set_base_time(mGstPipeline, GST_CLOCK_TIME_NONE);
}

}
