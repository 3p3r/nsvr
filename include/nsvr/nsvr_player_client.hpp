#pragma once

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_client.hpp"

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
    virtual void    onUpdate() override;
    virtual void    onMessage(const std::string& message) override;
    virtual void    onClockSetup() override;
    virtual void    onClockClear();

private:
    GstClock*       mNetClock;
    GstClockTime    mBaseTime;
};

}
