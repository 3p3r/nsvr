#include "nsvr_internal.hpp"
#include "nsvr/nsvr_player_client.hpp"

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

    if (message.size() > 1 && message[0] == 's')
    {
        // server clock received
        if (message[1] == 'c')
        {
            for (const auto& cmd : Internal::explode(message.substr(2), '|'))
            {
                if (cmd[0] == 'b')
                    mBaseTime = std::stoull(cmd.substr(1));
            }

            setupClock();
        }
        // server heartbeat received
        else if (message[1] == 'h')
        {
            gdouble time        = 0;
            gdouble volume      = 0;
            bool mute           = true;
            GstState state      = GST_STATE_NULL;
            GstClockTime base   = 0;

            for (const auto& cmd : Internal::explode(message.substr(2), '|'))
            {
                if (cmd[0] == 't')
                    time = std::stod(cmd.substr(1));
                else if (cmd[0] == 'v')
                    volume = std::stod(cmd.substr(1));
                else if (cmd[0] == 'm')
                    mute = (std::stoi(cmd.substr(1)) != FALSE);
                else if (cmd[0] == 's')
                    state = static_cast<GstState>(std::stoi(cmd.substr(1)));
                else if (cmd[0] == 'b')
                    base = std::stoull(cmd.substr(1));
            }

            if (getMute() != mute)
                setMute(mute);

            if (getVolume() != volume)
                setVolume(volume);

            if (getState() != state)
                setState(state);

            if (std::abs(getTime() - time) > 0.1)
                setTime(time);
        }
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
