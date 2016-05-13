#include "nsvr_internal.hpp"
#include "nsvr/nsvr_player_client.hpp"

#include <iostream>

namespace nsvr
{

PlayerClient::PlayerClient(const std::string& address, short port)
    : mClockAddress(address)
    , mBaseTime(0)
    , mClockPort(port)
    , mNetClock(nullptr)
{
    connect("239.0.0.1", 5000);
}

void PlayerClient::onMessage(const std::string& message)
{
    if (message.empty() || message[0] == 'c')
        return;

    if (message.size() > 1 && message[0] == 's' && message[1] == 'c' && mBaseTime == 0)
    {
        gdouble         pos = 0;
        mBaseTime       = 0;

        for (const auto& cmd : Internal::explode(message.substr(2), '|'))
        {
            if (cmd[0] == 'b')
                mBaseTime = std::stoull(cmd.substr(1));
        }

        setupClock();
    }
}

void PlayerClient::onError(const std::string& error)
{
    Player::onError(error.c_str());
}

void PlayerClient::setupClock()
{
    pause();
    clearClock();

    if (GstClock *net_clock = gst_net_client_clock_new(nullptr, mClockAddress.c_str(), mClockPort, mBaseTime))
    {
        mNetClock = net_clock;
        
        gst_element_set_start_time(mPipeline, GST_CLOCK_TIME_NONE);
        gst_element_set_base_time(mPipeline, mBaseTime);
        gst_pipeline_use_clock(GST_PIPELINE(mPipeline), net_clock);
    }

    mBaseTime = 0;
    play();
}

void PlayerClient::update()
{
    Player::update();
    iterate();
}

void PlayerClient::requestClock()
{
    send("cc");
}

void PlayerClient::clearClock()
{
    if (mNetClock != nullptr)
    {
        gst_object_unref(mNetClock);
        mNetClock = nullptr;
    }
}

void PlayerClient::close()
{
    clearClock();
    requestClock();
    Player::close();
}

}
