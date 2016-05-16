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
{
    if (defaultMulticastGroupEnabled() &&
        !connect(getDefaultMulticastIp(), getDefaultMulticastPort()))
    {
        NSVR_LOG("Player was unable to join the default multicast group.");
        return;
    }
}

void PlayerServer::setupClock()
{
    g_return_if_fail(mPipeline != nullptr);

    clearClock();

    if ((mNetClock = gst_pipeline_get_clock(GST_PIPELINE(mPipeline))) &&
        (mNetProvider = GST_OBJECT(gst_net_time_provider_new(mNetClock, mClockAddress.c_str(), mClockPort))))
    {
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

    mHeartbeatCounter++;
}

}
