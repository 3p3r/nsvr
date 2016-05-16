#include "nsvr_internal.hpp"
#include "nsvr.hpp"

#include <gst/net/net.h>

namespace nsvr
{

PlayerServer::PlayerServer(const std::string& address, short port)
    : mClockAddress(address)
    , mClockPort(port)
    , mNetClock(nullptr)
    , mNetProvider(nullptr)
    , mHeartbeatCounter(0)
    , mHeartbeatFrequency(30)
    , mPendingSeek(-1.)
    , mPendingState(GST_STATE_NULL)
{
    if (defaultMulticastGroupEnabled() &&
        !connect(getDefaultMulticastIp(), getDefaultMulticastPort()))
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
    if (mPendingSeek != time)
        mPendingSeek = CLAMP(time, 0, getDuration());

    mPendingState = getState(false);
}

void PlayerServer::setupClock()
{
    g_return_if_fail(mPipeline != nullptr);

    clearClock();

    if ((mNetClock = gst_pipeline_get_clock(GST_PIPELINE(mPipeline))) &&
        (mNetProvider = GST_OBJECT(gst_net_time_provider_new(mNetClock, mClockAddress.c_str(), mClockPort))))
    {
        gst_clock_set_timeout(mNetClock, 100 * GST_MSECOND);
        adjustClock();
    }
}

void PlayerServer::dispatchHeartbeat()
{
    g_return_if_fail(mPipeline != nullptr);

    std::stringstream dispatch_cmd;

    dispatch_cmd << "sh";
    dispatch_cmd << "|";
    dispatch_cmd << "t" << getTime();
    dispatch_cmd << "|";
    dispatch_cmd << "v" << getVolume();
    dispatch_cmd << "|";
    dispatch_cmd << "m" << getMute() ? TRUE : FALSE;
    dispatch_cmd << "|";
    dispatch_cmd << "s" << getState();
    dispatch_cmd << "|";
    dispatch_cmd << "b" << gst_element_get_base_time(mPipeline);

    send(dispatch_cmd.str());
}

void PlayerServer::adjustClock()
{
    g_return_if_fail(mPipeline != nullptr && mNetClock != nullptr && mNetProvider != nullptr);

    GstClockTime base_time = gst_clock_get_time(mNetClock);

    gst_pipeline_use_clock(GST_PIPELINE(mPipeline), mNetClock);
    gst_element_set_start_time(mPipeline, GST_CLOCK_TIME_NONE);
    gst_element_set_base_time(mPipeline, base_time);
}

void PlayerServer::clearClock()
{
    g_return_if_fail(mPipeline != nullptr);

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

void PlayerServer::onBeforeClose()
{
    clearClock();
}

void PlayerServer::onBeforeUpdate()
{
    g_return_if_fail(mPipeline != nullptr);

    iterate();

    if (mHeartbeatCounter > mHeartbeatFrequency)
    {
        dispatchHeartbeat();
        mHeartbeatCounter = 0;
    }

    if (mPendingSeek >= 0.)
    {
        if (getState() != GST_STATE_PAUSED)
            pause();
        else
        {
            auto current_time = getTime();
            auto time_diff = current_time - mPendingSeek;
            auto time_base = gst_element_get_base_time(mPipeline) + GstClockTime(time_diff * GST_SECOND);

            gst_element_set_base_time(mPipeline, time_base);

            setState(mPendingState);

            mPendingSeek = -1.;
            mPendingState = GST_STATE_NULL;
        }
    }

    mHeartbeatCounter++;
}

}
