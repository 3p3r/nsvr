#pragma once

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_peer.hpp"

namespace nsvr
{

class PlayerServer : public Player, protected Peer
{
public:
    PlayerServer(const std::string& address, short port);
    virtual void        update() override;
    virtual void        close() override;

protected:
    virtual void        onMessage(const std::string& message) override;
    virtual void        onError(const std::string& error) override;
    virtual void        setupClock() override;

protected:
    void                dispatchClock(GstClockTime base);
    void                clearClock();

private:
    std::string         mClockAddress;
    GstObject           *mNetClock;
    GstClockTime        mClockOffset;
    short               mClockPort;
};

}
