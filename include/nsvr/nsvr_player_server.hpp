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
    virtual void        onSeekFinished() override;

protected:
    void                dispatchHeartbeat();
    void                adjustClock();
    void                clearClock();

private:
    std::string         mClockAddress;
    GstClock            *mNetClock;
    GstObject           *mNetProvider;
    GstClockTime        mClockOffset;
    unsigned            mHeartbeatCounter;
    unsigned            mHeartbeatFrequency;
    short               mClockPort;
};

}
