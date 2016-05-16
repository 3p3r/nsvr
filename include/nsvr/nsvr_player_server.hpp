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
    //! Constructs a server player with its clock dispatched at "address" and "port"
    PlayerServer(const std::string& address, short port);

    //! Sets the heartbeat frequency of server. Default: 30 seconds
    void                setHeartbeatFrequency(unsigned freq);

    //! Gets the heartbeat frequency of server. Default: 30 seconds
    unsigned            getHeartbeatFrequency() const;

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
