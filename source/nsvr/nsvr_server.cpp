#include "nsvr/nsvr_server.hpp"
#include "nsvr_internal.hpp"

namespace nsvr {

Server::Server()
    : mSocketService(nullptr)
    , mListenPort(0)
{}

Server::~Server()
{
    if (isListening())
        shutdown();
}

bool Server::listen(const std::string& listen_address, short listen_port)
{
    if (isListening())
        shutdown();

    mListenAddress = listen_address;
    mListenPort = listen_port;

    GError *errors = nullptr;
    BIND_TO_SCOPE(errors);
    
    if (GSocketAddress *inet_addr = g_inet_socket_address_new_from_string(listen_address.c_str(), listen_port))
    {
        BIND_TO_SCOPE(inet_addr);

        if (mSocketService = g_socket_service_new())
        {
            if (g_socket_listener_add_address(
                reinterpret_cast<GSocketListener*>(mSocketService),
                inet_addr, GSocketType::G_SOCKET_TYPE_STREAM,
                GSocketProtocol::G_SOCKET_PROTOCOL_TCP,
                nullptr, nullptr, &errors) == FALSE)
            {
                NSVR_LOG("Unable to bind Server to address " << listen_address << ":" << listen_port);
                return false;
            }

            g_signal_connect(mSocketService, "incoming", G_CALLBACK(incoming), this);
            g_socket_service_start(mSocketService);
        }
        else
        {
            NSVR_LOG("Unable to construct socket service.");
            return false;
        }
    }
    else
    {
        NSVR_LOG("Invalid address passed to Server: " << listen_address << ":" << listen_port);
        return false;
    }

    return isListening();
}

void Server::iterate()
{
    while (g_main_context_pending(g_main_context_default()) != FALSE)
        g_main_context_iteration(g_main_context_default(), FALSE);
}

bool Server::isListening()
{
    return mSocketService != nullptr && g_socket_service_is_active(mSocketService) != FALSE;
}

void Server::shutdown()
{
    if (!isListening())
        return;

    g_socket_service_stop(mSocketService);
    mSocketService = nullptr;
    mListenPort = 0;
}

void Server::broadcastToClients(const std::string& message)
{
    if (mEndpoints.empty())
        return;

    for (const auto& endpoint : mEndpoints)
        endpoint.second.send(message);
}

short Server::getListenPort() const
{
    return mListenPort;
}

const std::string& Server::getListenAddress() const
{
    return mListenAddress;
}

gboolean Server::incoming(GSocketService *service, GSocketConnection *connection, GObject *source_object, gpointer user_data)
{
    if (GInputStream* istream = g_io_stream_get_input_stream(G_IO_STREAM(connection)))
    {
        GError* error = nullptr;
        const gssize count = 16;
        gchar message[count] {0};
        BIND_TO_SCOPE(error);

        if (g_input_stream_read(istream, message, count, nullptr, &error) >= 0)
        {
            if (auto server = static_cast<Server*>(user_data))
            {
                std::string ip = internal::getIp(connection);

                if (server->mEndpoints.find(ip) == server->mEndpoints.cend())
                {
                    server->mEndpoints[ip] = UdpEndpoint();
                    server->mEndpoints[ip].setup(ip, server->getListenPort());
                }

                server->onMessage(message);
            }
            else
            {
                NSVR_LOG("Unable to cast user data to Server.")
            }
        }
        else NSVR_LOG("Unable to read the input stream.")
    }

    return FALSE;
}

void Server::UdpEndpoint::setup(const std::string& ip, short port)
{
    if (mEndpointSocket)
        g_clear_object(&mEndpointSocket);

    if (mEndpointAddress)
        g_clear_object(&mEndpointAddress);


    GInetAddress *group = g_inet_address_new_from_string(ip.c_str());
    BIND_TO_SCOPE(group);

    mEndpointAddress = g_inet_socket_address_new(group, port);

    if (!mEndpointAddress)
    {
        NSVR_LOG("Endpoint address could not be constructed: " << ip);
        return;
    }

    GError *errors = nullptr;
    BIND_TO_SCOPE(errors);

    mEndpointSocket = g_socket_new(
        GSocketFamily::G_SOCKET_FAMILY_IPV4,
        GSocketType::G_SOCKET_TYPE_DATAGRAM,
        GSocketProtocol::G_SOCKET_PROTOCOL_UDP,
        &errors);

    if (!mEndpointSocket)
    {
        NSVR_LOG("Unable to construct send soccket [" << errors->message << "].");
        return;
    }

    g_socket_set_blocking(mEndpointSocket, FALSE);
}

Server::UdpEndpoint::UdpEndpoint()
    : mEndpointSocket(nullptr)
    , mEndpointAddress(nullptr)
{}

Server::UdpEndpoint::~UdpEndpoint()
{
    g_clear_object(&mEndpointSocket);
    g_clear_object(&mEndpointAddress);
}

void Server::UdpEndpoint::send(const std::string& message, unsigned timeout /*= 1*/) const
{
    if (!mEndpointAddress || !mEndpointSocket)
        return;

    GError *errors = nullptr;
    BIND_TO_SCOPE(errors);

    if (g_socket_get_timeout(mEndpointSocket) != timeout)
        g_socket_set_timeout(mEndpointSocket, timeout);

    if (g_socket_send_to(mEndpointSocket, mEndpointAddress, message.c_str(), message.size(), nullptr, &errors) < 0)
    {
        NSVR_LOG("Server was unable to send a message [" << errors->message << "].");
        return;
    }
}

}
