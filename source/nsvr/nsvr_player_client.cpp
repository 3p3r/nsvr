#include "nsvr_internal.hpp"
#include "nsvr/nsvr_player_client.hpp"

namespace nsvr
{

PlayerClient::PlayerClient(const std::string& address, short port)
    : mClockAddress(address)
    , mClockOffset(0)
    , mClockPort(port)
{
    connect("239.0.0.1", 5000);
}

void PlayerClient::onMessage(const std::string& message)
{
    if (message.empty())
        return;

    if (message.size() > 2)
    {
        if (message[0] == 's')
        {
            // this is a command sent from a server
            if (message[1] == 'o')
            {
                // we have received a new offset
                mClockOffset = std::stoi(message.substr(2));
                setupClock();
            }
        }
        else if (message[0] == 'c')
        {

        }
    }
}

void PlayerClient::onError(const std::string& error)
{
    Player::onError(error.c_str());
}

void PlayerClient::setupClock()
{
    if (mClockOffset == 0)
    {
        requestClock();
        return;
    }

    // reset my clock so it won't advance detached from net
    gst_element_set_start_time(mPipeline, GST_CLOCK_TIME_NONE);
    // get the net clock
    if (GstClock *clock = gst_net_client_clock_new("clock0", mClockAddress.c_str(), mClockPort, mClockOffset))
    {
        BIND_TO_SCOPE(clock);
        // set base time received from server
        gst_element_set_base_time(mPipeline, mClockOffset);
        // apply the net clock
        gst_pipeline_use_clock(GST_PIPELINE(mPipeline), clock);
    }
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

}
