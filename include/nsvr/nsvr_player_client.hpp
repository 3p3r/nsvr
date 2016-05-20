#pragma once

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_peer.hpp"

namespace nsvr
{

class PlayerClient
    : public Player
    , public Client
{
public:
    //! Constructs a client player with its clock listening from "address" and "port"
    PlayerClient(const std::string& address, short port);

protected:
    virtual void    onBeforeOpen() override;
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
