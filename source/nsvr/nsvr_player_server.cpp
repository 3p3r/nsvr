#include "nsvr_internal.hpp"
#include "nsvr/nsvr_player_server.hpp"

#include <sstream>
#include <gst/net/net.h>

namespace nsvr
{

PlayerServer::PlayerServer(const std::string& address, short port)
    : mClockAddress(address)
    , mClockOffset(0)
    , mClockPort(port)
{
    connect("239.0.0.1", 5000);
}

void PlayerServer::onMessage(const std::string& message)
{
    if (message.empty())
        return;

    if (message.size() > 2)
    {
        if (message[0] == 'c')
        {
            // this is a command sent from a client
            if (message[1] == 'r')
            {
                // we have received a request to dispatch
                dispatchClock();
            }
        }
        else if (message[0] == 's')
        {

        }
    }
}

void PlayerServer::onError(const std::string& error)
{
    Player::onError(error.c_str());
}

void PlayerServer::setupClock()
{
    // apply pipeline clock to itself, to make sure we're on charge
    if (GstClock* clock = gst_pipeline_get_pipeline_clock(GST_PIPELINE(mPipeline)))
    {
        BIND_TO_SCOPE(clock);
        gst_pipeline_use_clock(GST_PIPELINE(mPipeline), clock);
        // instantiate network clock once for everyone
        if (GstNetTimeProvider* clock_provider = gst_net_time_provider_new(clock, mClockAddress.c_str(), mClockPort))
        {
            BIND_TO_SCOPE(clock_provider);
            // get the time for clients to start based on...
            mClockOffset = gst_clock_get_time(clock);
            // reset my clock so it won't advance detached from net
            gst_element_set_start_time(mPipeline, GST_CLOCK_TIME_NONE);
            // set the net clock to start ticking from our base time
            gst_element_set_base_time(mPipeline, mClockOffset);
            // dispatch new clock offset to all clients
            dispatchClock();
        }
    }
}

void PlayerServer::dispatchClock()
{
    std::stringstream ss;
    ss << "so" << mClockOffset;
    send(ss.str());
}

void PlayerServer::update()
{
    Player::update();
    iterate();
}

}
