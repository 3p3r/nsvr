#pragma once

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_peer.hpp"

namespace nsvr
{

class PlayerClient : public Player, protected Peer
{
public:
    PlayerClient(const std::string& address, short port);
    virtual void    close() override;
    virtual void    update() override;

protected:
    virtual void    onMessage(const std::string& message) override;
    virtual void    onError(const std::string& error) override;
    virtual void    setupClock() override;

protected:
    void            clearClock();

private:
    std::string     mClockAddress;
    GstClock        *mNetClock;
    GstClockTime    mBaseTime;
    short           mClockPort;
};

}
