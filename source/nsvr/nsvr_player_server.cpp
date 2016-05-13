#include "nsvr_internal.hpp"
#include "nsvr/nsvr_player_server.hpp"

#include <sstream>
#include <gst/net/net.h>

namespace nsvr
{

PlayerServer::PlayerServer(const std::string& address, short port)
    : mClockAddress(address)
    , mClockPort(port)
    , mNetClock(nullptr)
    , mHeartbeatCounter(0)
    , mHeartbeatFrequency(30)
{
    connect("239.0.0.1", 5000);
}

void PlayerServer::onMessage(const std::string& message)
{
    if (message.empty() || message[0] == 's')
        return;

    if (message.size() > 1 && message[0] == 'c' && message[1] == 'c')
        dispatchClock(gst_element_get_base_time(mPipeline));
}

void PlayerServer::onError(const std::string& error)
{
    Player::onError(error.c_str());
}

void PlayerServer::setupClock()
{
    clearClock();

    if (GstClock* clock = gst_pipeline_get_clock(GST_PIPELINE(mPipeline)))
    {
        BIND_TO_SCOPE(clock);

        gst_pipeline_use_clock(GST_PIPELINE(mPipeline), clock);
        GstClockTime base_time = gst_clock_get_time(clock);
        
        if (mNetClock = GST_OBJECT(gst_net_time_provider_new(clock, mClockAddress.c_str(), mClockPort)))
        {
            gst_element_set_start_time(mPipeline, GST_CLOCK_TIME_NONE);
            gst_element_set_base_time(mPipeline, base_time);

            dispatchClock(base_time);
        }
    }
}

void PlayerServer::dispatchHeartbeat()
{
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
    dispatch_cmd << "b" << gst_clock_get_time(GST_CLOCK(mNetClock));

    send(dispatch_cmd.str());
}

void PlayerServer::dispatchClock(GstClockTime base)
{
    if (GstClock* clock = gst_pipeline_get_clock(GST_PIPELINE(mPipeline)))
    {
        BIND_TO_SCOPE(clock);

        std::stringstream dispatch_cmd;

        dispatch_cmd << "sc";
        dispatch_cmd << "|";
        dispatch_cmd << "b" << base;

        send(dispatch_cmd.str());
    }
}

void PlayerServer::clearClock()
{
    if (mNetClock != nullptr)
    {
        gst_object_unref(mNetClock);
        mNetClock = nullptr;
    }
}

void PlayerServer::close()
{
    clearClock();
    Player::close();
}

void PlayerServer::update()
{
    Player::update();
    iterate();

    if (mHeartbeatCounter > mHeartbeatFrequency)
    {
        dispatchHeartbeat();
        mHeartbeatCounter = 0;
    }

    mHeartbeatCounter++;
}

}
