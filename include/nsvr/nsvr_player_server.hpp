#pragma once

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_server.hpp"

namespace nsvr
{

class PlayerServer
    : public Player
    , public Server
{
public:
    //! Constructs a server player with its clock dispatched at "address" and "port"
    PlayerServer(const std::string& address, short port);

    //! Sets the heartbeat frequency of server. Default: 30 seconds
    void setHeartbeatFrequency(unsigned freq);

    //! Gets the heartbeat frequency of server. Default: 30 seconds
    unsigned getHeartbeatFrequency() const;

    //! Override of Player's setTime to perform network seek
    virtual void setTime(gdouble time) override;

protected:
    virtual void    onUpdate() override;
    virtual void    onClockSetup() override;
    virtual void    onClockClear() override;
    virtual void    onStateChanged() override;
    void            dispatchHeartbeat();

private:
    GstClock*       mNetClock;
    GstObject*      mNetProvider;
    GstClockTime    mClockOffset;
    GstClockTime    mPendingCurrentTime;
    gdouble         mPendingStateSeek;
    gdouble         mPendingSeek;
    GstPlayerState  mPendingState;
    unsigned        mHeartbeatCounter;
    unsigned        mHeartbeatFrequency;
};

}
