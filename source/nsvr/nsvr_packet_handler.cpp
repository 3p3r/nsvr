#include "nsvr_internal.hpp"
#include "nsvr/nsvr_packet_handler.hpp"

namespace {
const char PACKET_DATA_SEPARATOR        = '|';
const char SERVER_IDENTIFIER            = 's';
const char CLIENT_IDENTIFIER            = 'c';
const char SERVER_HEARTBEAT_IDENTIFIER  = 'h';
const char PACKET_TIME_ATT              = 't';
const char PACKET_MUTE_ATT              = 'm';
const char PACKET_VOLUME_ATT            = 'v';
const char PACKET_STATE_ATT             = 's';
const char PACKET_BASE_ATT              = 'b';
const int  PACKET_ATT_COUNT             = 5;
}

namespace nsvr
{

bool PacketHandler::parse(const std::string& buffer, Packet& packet)
{
    if (buffer.empty() || buffer[0] == CLIENT_IDENTIFIER)
        return false;

    bool succeed = false;
    int entities = 0;

    if (buffer.size() > 3 && buffer[0] == SERVER_IDENTIFIER)
    {
        if (buffer[1] == SERVER_HEARTBEAT_IDENTIFIER)
        {
            try
            {
                auto commands = internal::explode(buffer.substr(3), PACKET_DATA_SEPARATOR);

                for (const auto& command : commands)
                {
                    if (command.empty())
                        continue;

                    if (command[0] == PACKET_TIME_ATT)
                    {
                        packet.time = std::stod(command.substr(1));
                        entities++;
                    }
                    else if (command[0] == PACKET_VOLUME_ATT)
                    {
                        packet.volume = std::stod(command.substr(1));
                        entities++;
                    }
                    else if (command[0] == PACKET_MUTE_ATT)
                    {
                        packet.mute = std::stoi(command.substr(1));
                        entities++;
                    }
                    else if (command[0] == PACKET_STATE_ATT)
                    {
                        packet.state = static_cast<GstPlayerState>(std::stoi(command.substr(1)));
                        entities++;
                    }
                    else if (command[0] == PACKET_BASE_ATT)
                    {
                        packet.base = std::stoull(command.substr(1));
                        entities++;
                    }
                }
            }
            catch (...)
            {
                NSVR_LOG("Failed to parse buffer. Packet buffer is malformed.");
            }
        }
    }

    succeed = (entities == PACKET_ATT_COUNT);
    return succeed;
}

std::string PacketHandler::serialize(const Packet& packet)
{
    std::stringstream serialized_packet;

    serialized_packet << SERVER_IDENTIFIER;
    serialized_packet << SERVER_HEARTBEAT_IDENTIFIER;
    serialized_packet << PACKET_DATA_SEPARATOR;
    serialized_packet << PACKET_TIME_ATT << packet.time;
    serialized_packet << PACKET_DATA_SEPARATOR;
    serialized_packet << PACKET_VOLUME_ATT << packet.volume;
    serialized_packet << PACKET_DATA_SEPARATOR;
    serialized_packet << PACKET_MUTE_ATT << packet.mute;
    serialized_packet << PACKET_DATA_SEPARATOR;
    serialized_packet << PACKET_STATE_ATT << packet.state;
    serialized_packet << PACKET_DATA_SEPARATOR;
    serialized_packet << PACKET_BASE_ATT << packet.base;

    return serialized_packet.str();
}

}
