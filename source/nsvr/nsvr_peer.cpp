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
    : mSendSocket(nullptr)
    , mReceiveSocket(nullptr)
    , mListenGroup(nullptr)
    , mCastGroup(nullptr)
    , mListenAddress(nullptr)
    , mCastAddress(nullptr)
    , mContext(nullptr)
    , mSource(nullptr)
    , mConnected(false)
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
        NSVR_LOG("Unable to construct receive soccket [" << errors->message << "].");
        return false;
    }
    
    mSendSocket = g_socket_new(
        GSocketFamily::G_SOCKET_FAMILY_IPV4,
        GSocketType::G_SOCKET_TYPE_DATAGRAM,
        GSocketProtocol::G_SOCKET_PROTOCOL_UDP,
        &errors);

    if (!mSendSocket)
    {
        NSVR_LOG("Unable to construct send soccket [" << errors->message << "].");
        return false;
    }

    g_socket_set_multicast_loopback(mReceiveSocket, TRUE);
    g_socket_set_blocking(mReceiveSocket, FALSE);
    g_socket_set_blocking(mSendSocket, FALSE);
    g_socket_set_broadcast(mSendSocket, TRUE);

    mListenGroup = g_inet_address_new_any(GSocketFamily::G_SOCKET_FAMILY_IPV4);
    mListenAddress = g_inet_socket_address_new(mListenGroup, multicast_port);

    if (!mListenGroup || !mListenAddress)
    {
        NSVR_LOG("Listens address(es) could not be constructed.");
        return false;
    }

    mCastGroup = g_inet_address_new_from_string(mutlicast_group.c_str());
    mCastAddress = g_inet_socket_address_new(mCastGroup, multicast_port);

    if (!mCastGroup || !mCastAddress)
    {
        NSVR_LOG("Cast address(es) could not be constructed.");
        return false;
    }

    if (g_socket_bind(mReceiveSocket, mListenAddress, TRUE, &errors) != FALSE &&
        g_socket_join_multicast_group(mReceiveSocket, mCastGroup, FALSE, nullptr, &errors) != FALSE)
    {
        mContext = g_main_context_new();
        mSource = g_socket_create_source(mReceiveSocket, G_IO_IN, nullptr);

        g_source_set_callback(mSource, reinterpret_cast<GSourceFunc>(onSocketData), this, nullptr);
        g_source_set_priority(mSource, G_PRIORITY_HIGH);
        g_source_attach(mSource, mContext);
    }
    else
    {
        NSVR_LOG("Peer failed to join the multicast group [" << errors->message << "].");
        return false;
    }

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

    g_clear_object(&mCastAddress);
    g_clear_object(&mListenAddress);
    g_clear_object(&mCastGroup);
    g_clear_object(&mListenGroup);
    g_clear_object(&mReceiveSocket);
    g_clear_object(&mSendSocket);

    mConnected = false;
    NSVR_LOG("Peer is disconnected.");
}

void Peer::send(const std::string& message, unsigned timeout /*= 1*/)
{
    if (!isConnected())
        return;

    GError *errors = nullptr;
    BIND_TO_SCOPE(errors);

    if (g_socket_get_timeout(mSendSocket) != timeout)
        g_socket_set_timeout(mSendSocket, timeout);

    if (g_socket_send_to(mSendSocket, mCastAddress, message.c_str(), message.size(), nullptr, &errors) < 0)
    {
        NSVR_LOG("Peer was unable to send a message [" << errors->message << "].");
        return;
    }
}

void Peer::iterate()
{
    if (!isConnected())
        return;

    while (g_main_context_pending(mContext) != FALSE)
        g_main_context_iteration(mContext, FALSE);
}

gboolean Peer::onSocketData(GSocket *socket, GIOCondition condition, gpointer self)
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
}

void Server::broadcast(const std::string& message)
{
    // todo
}

gboolean Server::incoming(GSocketService *service, GSocketConnection *connection, GObject *source_object, gpointer user_data)
{
    if (GInputStream* istream = g_io_stream_get_input_stream(G_IO_STREAM(connection)))
    {
        GError* error = nullptr;
        const gssize count = 16;
        gchar message[count] {0};

        if (g_input_stream_read(istream, message, count, nullptr, &error) >= 0)
        {
            if (auto server = static_cast<Server*>(user_data))
                server->onMessage(message);
            else
                NSVR_LOG("Unable to cast user data to Server.")
        }
        else NSVR_LOG("Unable to read the input stream.")
    }

    return FALSE;
}

}
