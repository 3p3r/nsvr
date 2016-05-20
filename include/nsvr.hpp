#pragma once

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_player_client.hpp"
#include "nsvr/nsvr_player_server.hpp"

#define NSVR_VERSION_MAJOR 1
#define NSVR_VERSION_MINOR 0
#define NSVR_VERSION_PATCH 0

namespace nsvr
{

//! Returns version of NSVR (1st.) + GStreamer (2nd.)
std::string getVersion();

//! Adds a plug-in path to GStreamer's search directory
void addPluginPath(const std::string& path);

//! Utility that prints all loaded DLLs on Windows
void listModules();

}
