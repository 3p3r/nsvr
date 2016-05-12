#pragma once

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_peer.hpp"

namespace nsvr
{

class PlayerClient : public Player, protected Peer
{
public:
    PlayerClient(const std::string& address, short port);

protected:
    virtual void    onMessage(const std::string& message) override;
    virtual void    onError(const std::string& error) override;
    virtual void    setupClock() override;
    virtual void    update() override;

protected:
    void            requestClock();

private:
    std::string     mClockAddress;
    GstClockTime    mClockOffset;
    short           mClockPort;
};

}
