#pragma once

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_peer.hpp"

namespace nsvr
{

class PlayerServer : public Player, protected Peer
{
public:
    PlayerServer(const std::string& address, short port);
    virtual void    update() override;

protected:
    virtual void    onMessage(const std::string& message) override;
    virtual void    onError(const std::string& error) override;
    virtual void    setupClock() override;

protected:
    void            dispatchClock();

private:
    std::string     mClockAddress;
    GstClockTime    mClockOffset;
    short           mClockPort;
};

}
