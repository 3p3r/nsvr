#include "nsvr/nsvr_packet_handler.hpp"
#include "nsvr_internal.hpp"
#include "nsvr.hpp"

#include <gst/net/net.h>

namespace nsvr
{

PlayerServer::PlayerServer(const std::string& address, short port)
    : mNetClock(nullptr)
    , mNetProvider(nullptr)
    , mHeartbeatCounter(0)
    , mHeartbeatFrequency(30)
    , mPendingStateSeek(-1.)
    , mPendingSeek(-1.)
    , mPendingState(GST_PLAYER_STATE_STOPPED)
    , mPendingCurrentTime(GST_CLOCK_TIME_NONE)
{
    if (!listen(address, port))
    {
        NSVR_LOG("Player was unable to join the default multicast group.");
        return;
    }
}

void PlayerServer::setHeartbeatFrequency(unsigned freq)
{
    if (freq == mHeartbeatFrequency)
        return;

    mHeartbeatFrequency = freq;
    mHeartbeatCounter = 0;
}

unsigned PlayerServer::getHeartbeatFrequency() const
{
    return mHeartbeatFrequency;
}

void PlayerServer::setTime(gdouble time)
{
    mPendingCurrentTime = gst_clock_get_time(mNetClock);
    mPendingSeek = CLAMP(time, 0, getDuration());
    mPendingState = getState();
}

void PlayerServer::onClockSetup()
{
    g_return_if_fail(mGstPipeline != nullptr);

    if ((mNetClock = gst_pipeline_get_clock(GST_PIPELINE(mGstPipeline))) &&
        (mNetProvider = GST_OBJECT(gst_net_time_provider_new(mNetClock, getListenAddress().c_str(), internal::getClockPort(getListenPort())))))
    {
        GstClockTime base_time = gst_clock_get_time(mNetClock);

        gst_clock_set_timeout(mNetClock, 100 * GST_MSECOND);
        gst_pipeline_use_clock(GST_PIPELINE(mGstPipeline), mNetClock);
        gst_element_set_start_time(mGstPipeline, GST_CLOCK_TIME_NONE);
        gst_element_set_base_time(mGstPipeline, base_time);
    }
}

void PlayerServer::onStateChanged()
{
    if (getState() == GST_STATE_PLAYING && mPendingStateSeek >= 0.)
    {
        setTime(mPendingStateSeek);
        mPendingStateSeek = -1.;
    }
    else if (getState() == GST_PLAYER_STATE_STOPPED)
        mPendingStateSeek = 0.;
    else if (getState() == GST_PLAYER_STATE_PAUSED)
        mPendingStateSeek = getTime();
}

void PlayerServer::dispatchHeartbeat()
{
    g_return_if_fail(mGstPipeline != nullptr);

    Packet packet;
    packet.time     = getTime();
    packet.volume   = getVolume();
    packet.mute     = getMute() ? TRUE : FALSE;
    packet.state    = getState();
    packet.base     = gst_element_get_base_time(mGstPipeline);

    broadcastToClients(PacketHandler::serialize(packet));
}

void PlayerServer::onClockClear()
{
    g_return_if_fail(mGstPipeline != nullptr);

    if (mNetClock != nullptr)
    {
        gst_object_unref(mNetClock);
        mNetClock = nullptr;
    }

    if (mNetProvider != nullptr)
    {
        gst_object_unref(mNetProvider);
        mNetProvider = nullptr;
    }
}

void PlayerServer::onUpdate()
{
    g_return_if_fail(mGstPipeline != nullptr);

    iterate();

    if (mHeartbeatCounter > mHeartbeatFrequency)
    {
        dispatchHeartbeat();
        mHeartbeatCounter = 0;
    }

    if (mPendingSeek >= 0.)
    {
        if (getState() != GST_STATE_READY)
        {
            if (gst_element_set_state(mGstPipeline, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE)
                NSVR_LOG("Server failed to put pipeline into READY state for a pending seek.")
        }
        else
        {
            auto time_curr = (mPendingCurrentTime - gst_element_get_base_time(mGstPipeline)) / gdouble(GST_SECOND);
            auto time_diff = time_curr - mPendingSeek;
            auto time_base = gst_element_get_base_time(mGstPipeline) + GstClockTime(time_diff * GST_SECOND);

            gst_element_set_base_time(mGstPipeline, time_base);
            setState(mPendingState);

            mPendingSeek = -1.;
            mPendingState = GST_PLAYER_STATE_STOPPED;
            mPendingCurrentTime = GST_CLOCK_TIME_NONE;
        }
    }

    mHeartbeatCounter++;
}

}
