#include "nsvr_internal.hpp"
#include "nsvr/nsvr_player_client.hpp"

namespace nsvr
{

PlayerClient::PlayerClient(const std::string& address, short port)
    : mClockAddress(address)
    , mBaseTime(0)
    , mClockPort(port)
{
    connect("239.0.0.1", 5000);
}

void PlayerClient::onMessage(const std::string& message)
{
    if (message.empty())
        return;

    if (message.size() > 1)
    {
        if (message[0] == 's')
        {
            // this is a command sent from a server
            if (message[1] == 'o')
            {
                // we have received a new offset
                mBaseTime = std::stoull(message.substr(2));
                setupClock();
            }
        }
    }
}

void PlayerClient::onError(const std::string& error)
{
    Player::onError(error.c_str());
}

void PlayerClient::setupClock()
{
    stop();
    
    if (GstClock *clock = gst_net_client_clock_new(nullptr, mClockAddress.c_str(), mClockPort, mBaseTime))
    {
        BIND_TO_SCOPE(clock);
        
        gst_element_set_start_time(mPipeline, GST_CLOCK_TIME_NONE);
        gst_element_set_base_time(mPipeline, mBaseTime);
        gst_pipeline_use_clock(GST_PIPELINE(mPipeline), clock);

    }

    play();
}

void PlayerClient::update()
{
    Player::update();
    iterate();
}

void PlayerClient::requestClock()
{
    send("cr");
}

void PlayerClient::close()
{
    requestClock();
    Player::close();
}

}
