#include "nsvr/nsvr_client.hpp"
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

Client::Client()
    : mReceiveSocket(nullptr)
    , mListenAddress(nullptr)
    , mContext(nullptr)
    , mSource(nullptr)
    , mConnected(false)
    , mServerPort(0)
{}

Client::~Client()
{
    if (isConnected())
        disconnect();
}

bool Client::connect(const std::string& server_address, short server_port)
{
    if (isConnected())
        disconnect();

    mServerAddress  = server_address;
    mServerPort     = server_port;
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

    mListenAddress = g_inet_socket_address_new(listen_group, server_port);

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

    mConnected = true;
    return true;
}

bool Client::isConnected() const
{
    return mConnected;
}

void Client::disconnect()
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

void Client::sendToServer(const std::string& message)
{
    if (!(!mServerAddress.empty() && mServerPort > 0))
        return;

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

void Client::iterate()
{
    if (!isConnected())
        return;

    while (g_main_context_pending(mContext) != FALSE)
        g_main_context_iteration(mContext, FALSE);
}

const std::string& Client::getServerAddress() const
{
    return mServerAddress;
}

gboolean Client::incomming(GSocket *socket, GIOCondition condition, gpointer self)
{
    gboolean source_action = G_SOURCE_CONTINUE;

    if (auto peer = static_cast<nsvr::Client*>(self))
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

short Client::getServerPort() const
{
    return mServerPort;
}

}
