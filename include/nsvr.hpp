#pragma once

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_player_client.hpp"
#include "nsvr/nsvr_player_server.hpp"

namespace nsvr
{

std::string getVersion();
void        addPluginPath(const std::string& path);

}
