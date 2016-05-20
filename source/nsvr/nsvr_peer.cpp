#include "nsvr/nsvr_peer.hpp"
#include "nsvr_internal.hpp"

namespace {
struct ScopedSocketRef
{
    ScopedSocketRef(GSocket *socket) : mSocket(socket) { g_object_ref(mSocket); }
    ~ScopedSocketRef() { g_object_unref(mSocket); }
    GSocket *mSocket;
};
}

namespace nsvr {

Peer::Peer()
    : mReceiveSocket(nullptr)
    , mListenAddress(nullptr)
    , mContext(nullptr)
    , mSource(nullptr)
    , mConnected(false)
    , mServerPort(0)
{}

Peer::~Peer()
{
    if (isConnected())
        disconnect();
}

bool Peer::connect(const std::string& mutlicast_group, short multicast_port)
{
    if (isConnected())
        disconnect();

    mServerAddress  .clear();
    mServerPort     = 0;
    mConnected      = false;
    GError *errors  = nullptr;

    BIND_TO_SCOPE(errors);

    mReceiveSocket = g_socket_new(
        GSocketFamily::G_SOCKET_FAMILY_IPV4,
        GSocketType::G_SOCKET_TYPE_DATAGRAM,
        GSocketProtocol::G_SOCKET_PROTOCOL_UDP,
        &errors);

    if (mReceiveSocket == nullptr)
    {
        NSVR_LOG("Unable to construct receive socket [" << errors->message << "].");
        return false;
    }

    g_socket_set_blocking(mReceiveSocket, FALSE);

    auto listen_group = g_inet_address_new_any(GSocketFamily::G_SOCKET_FAMILY_IPV4);
    BIND_TO_SCOPE(listen_group);

    mListenAddress = g_inet_socket_address_new(listen_group, multicast_port);

    if (!mListenAddress)
    {
        NSVR_LOG("Listen address could not be constructed.");
        return false;
    }

    if (g_socket_bind(mReceiveSocket, mListenAddress, TRUE, &errors) != FALSE)
    {
        mContext = g_main_context_new();
        mSource = g_socket_create_source(mReceiveSocket, G_IO_IN, nullptr);

        g_source_set_callback(mSource, reinterpret_cast<GSourceFunc>(incomming), this, nullptr);
        g_source_set_priority(mSource, G_PRIORITY_HIGH);
        g_source_attach(mSource, mContext);
    }
    else
    {
        NSVR_LOG("Client failed to bind [" << errors->message << "].");
        return false;
    }

    mServerAddress = mutlicast_group;
    mServerPort = multicast_port;
    mConnected = true;
    return true;
}

bool Peer::isConnected() const
{
    return mConnected;
}

void Peer::disconnect()
{
    if (!isConnected())
        return;

    g_source_unref(mSource);
    g_main_context_unref(mContext);
    
    mSource = nullptr;
    mContext = nullptr;

    g_clear_object(&mListenAddress);
    g_clear_object(&mReceiveSocket);

    mConnected = false;
    NSVR_LOG("Peer is disconnected.");
}

void Peer::send(const std::string& message)
{
    if (GSocketClient *client = g_socket_client_new())
    {
        GError *error = nullptr;
        BIND_TO_SCOPE(error);
        BIND_TO_SCOPE(client);

        if (GSocketConnection *connection = g_socket_client_connect_to_host(client, mServerAddress.c_str(), mServerPort, nullptr, &error))
        {
            BIND_TO_SCOPE(connection);
            if (GOutputStream *ostream = g_io_stream_get_output_stream(G_IO_STREAM(connection)))
            {
                if (g_output_stream_write(ostream, message.c_str(), message.size(), nullptr, &error) < 0)
                {
                    NSVR_LOG("Client was unable to write to output stream.");
                }
            }
            else
            {
                NSVR_LOG("Client was unable to obtain output stream.");
            }
        }
        else
        {
            NSVR_LOG("Client was unable to connect to host.");
        }
    }
    else
    {
        NSVR_LOG("Client was unable to construct socket client.");
    }
}

void Peer::iterate()
{
    if (!isConnected())
        return;

    while (g_main_context_pending(mContext) != FALSE)
        g_main_context_iteration(mContext, FALSE);
}

gboolean Peer::incomming(GSocket *socket, GIOCondition condition, gpointer self)
{
    gboolean source_action = G_SOURCE_CONTINUE;

    if (auto peer = static_cast<nsvr::Peer*>(self))
    {
        ScopedSocketRef ss(socket);

        gchar buffer[1024]  = { 0 };
        GError *errors      = nullptr;
        BIND_TO_SCOPE(errors);

        gssize received_bytes = g_socket_receive(socket, buffer, sizeof(buffer) / sizeof(buffer[0]), nullptr, &errors);

        if (received_bytes == 0)
        {
            NSVR_LOG("Receiver Peer connection closed by sender Peer [" << errors->message << "].");
            source_action = G_SOURCE_CONTINUE; // client closed, not our fault
        }
        else if (received_bytes < 0)
        {
            NSVR_LOG("Unable to receive socket data [" << errors->message << "].");
            source_action = G_SOURCE_REMOVE;
            peer->disconnect();
        }
        else if (peer->isConnected())
        {
            peer->onMessage(buffer);
            source_action = G_SOURCE_CONTINUE;
        }
        else
        {
            NSVR_LOG("Peer received a message but it is disconnected.");
            return G_SOURCE_REMOVE;
        }
    }

    return source_action;
}

Server::Server()
    : mSocketService(nullptr)
    , mPort(0)
{}

Server::~Server()
{
    if (isConnected())
        disconnect();
}

bool Server::listen(short port)
{
    if (isConnected())
        disconnect();

    GError *errors = nullptr;
    
    mSocketService = g_socket_service_new();
    
    if (g_socket_listener_add_inet_port((GSocketListener*)mSocketService, port, nullptr, &errors) == FALSE)
    {
        NSVR_LOG("Unable to bind Server to port " << port);
        disconnect();
        return false;
    }

    g_signal_connect(mSocketService, "incoming", G_CALLBACK(incoming), this);
    g_socket_service_start(mSocketService);

    mPort = port;
    return isConnected();
}

void Server::iterate()
{
    while (g_main_context_pending(g_main_context_default()) != FALSE)
        g_main_context_iteration(g_main_context_default(), FALSE);
}

bool Server::isConnected()
{
    return mSocketService != nullptr && g_socket_service_is_active(mSocketService) != FALSE;
}

void Server::disconnect()
{
    if (!isConnected())
        return;

    g_socket_service_stop(mSocketService);
    mSocketService = nullptr;
    mPort = 0;
}

void Server::broadcast(const std::string& message)
{
    for (auto& endpoint : mEndpoints)
        endpoint.second.send(message);
}

short Server::getPort() const
{
    return mPort;
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
                    server->mEndpoints[ip].connect(ip, server->getPort());
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

void Server::UdpEndpoint::connect(const std::string& ip, short port)
{
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

void Server::UdpEndpoint::send(const std::string& message, unsigned timeout /*= 1*/)
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
