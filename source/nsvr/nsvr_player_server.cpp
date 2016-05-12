#include "nsvr_internal.hpp"
#include "nsvr/nsvr_player_server.hpp"

#include <sstream>
#include <gst/net/net.h>

namespace nsvr
{

PlayerServer::PlayerServer(const std::string& address, short port)
    : mClockAddress(address)
    , mClockPort(port)
{
    connect("239.0.0.1", 5000);
}

void PlayerServer::onMessage(const std::string& message)
{
    if (message.empty())
        return;

    if (message.size() > 1)
    {
        if (message[0] == 'c')
        {
            // this is a command sent from a client
            if (message[1] == 'r')
            {
                // we have received a request to dispatch
                dispatchClock();

                stop();
                setupClock();
                play();
            }
        }
    }
}

void PlayerServer::onError(const std::string& error)
{
    Player::onError(error.c_str());
}

void PlayerServer::setupClock()
{
    if (GstClock* clock = gst_pipeline_get_clock(GST_PIPELINE(mPipeline)))
    {
        BIND_TO_SCOPE(clock);
        gst_pipeline_use_clock(GST_PIPELINE(mPipeline), clock);
        
        if (GstNetTimeProvider* clock_provider = gst_net_time_provider_new(clock, mClockAddress.c_str(), mClockPort))
        {
            BIND_TO_SCOPE(clock_provider);
            
            gst_element_set_start_time(mPipeline, GST_CLOCK_TIME_NONE);
            gst_element_set_base_time(mPipeline, gst_clock_get_time(clock));

            dispatchClock();
        }
    }
}

void PlayerServer::dispatchClock()
{
    std::stringstream dispatch_cmd;
    dispatch_cmd << "so" << gst_element_get_base_time(mPipeline);
    send(dispatch_cmd.str());
}

void PlayerServer::update()
{
    Player::update();
    iterate();
}

}
