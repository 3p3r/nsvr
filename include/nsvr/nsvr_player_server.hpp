#pragma once

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_peer.hpp"

namespace nsvr
{

class PlayerServer
    : public Player
    , public Peer
{
public:
    PlayerServer(const std::string& address, short port);

protected:
    virtual void        onBeforeUpdate() override;
    virtual void        onBeforeClose() override;
    virtual void        setupClock() override;
    void                dispatchHeartbeat();
    void                adjustClock();
    void                clearClock();

private:
    std::string         mClockAddress;
    GstClock*           mNetClock;
    GstObject*          mNetProvider;
    GstClockTime        mClockOffset;
    unsigned            mHeartbeatCounter;
    unsigned            mHeartbeatFrequency;
    short               mClockPort;
};

}
