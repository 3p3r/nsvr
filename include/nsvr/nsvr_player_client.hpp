#pragma once

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_peer.hpp"

namespace nsvr
{

class PlayerClient
    : public Player
    , public Peer
{
public:
    PlayerClient(const std::string& address, short port);

protected:
    virtual void    onBeforeClose() override;
    virtual void    onBeforeUpdate() override;
    virtual void    onMessage(const std::string& message) override;
    virtual void    setupClock() override;
    void            clearClock();

private:
    std::string     mClockAddress;
    GstClock*       mNetClock;
    GstClockTime    mBaseTime;
    short           mClockPort;
};

}
