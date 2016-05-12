#include "nsvr/nsvr_peer.hpp"
#include "nsvr_internal.hpp"

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

    mConnected = false;
    GError *errors = nullptr;
    BIND_TO_SCOPE(errors);

    mReceiveSocket = g_socket_new(
        GSocketFamily::G_SOCKET_FAMILY_IPV4,
        GSocketType::G_SOCKET_TYPE_DATAGRAM,
        GSocketProtocol::G_SOCKET_PROTOCOL_UDP,
        &errors);

    if (!mReceiveSocket)
    {
        onError(errors->message);
        return false;
    }
    
    mSendSocket = g_socket_new(
        GSocketFamily::G_SOCKET_FAMILY_IPV4,
        GSocketType::G_SOCKET_TYPE_DATAGRAM,
        GSocketProtocol::G_SOCKET_PROTOCOL_UDP,
        &errors);

    if (!mSendSocket)
    {
        onError(errors->message);
        return false;
    }

    g_socket_set_multicast_loopback(mReceiveSocket, TRUE);
    g_socket_set_blocking(mReceiveSocket, FALSE);
    
    g_socket_set_blocking(mSendSocket, TRUE);
    g_socket_set_broadcast(mSendSocket, TRUE);

    mListenGroup = g_inet_address_new_any(GSocketFamily::G_SOCKET_FAMILY_IPV4);
    mCastGroup = g_inet_address_new_from_string(mutlicast_group.c_str());
    mListenAddress = g_inet_socket_address_new(mListenGroup, multicast_port);
    mCastAddress = g_inet_socket_address_new(mCastGroup, multicast_port);

    if (!mListenGroup || !mCastGroup || !mListenAddress || !mCastAddress)
    {
        onError("One of the addresses could not be constructed.");
        return false;
    }

    if (g_socket_bind(mReceiveSocket, mListenAddress, TRUE, &errors) != FALSE &&
        g_socket_join_multicast_group(mReceiveSocket, mCastGroup, FALSE, nullptr, &errors) != FALSE)
    {
        mContext = g_main_context_new();
        mSource = g_socket_create_source(mReceiveSocket, G_IO_IN, nullptr);

        g_source_set_callback(mSource, reinterpret_cast<GSourceFunc>(Internal::onSocketDataAvailable), this, nullptr);
        g_source_set_priority(mSource, G_PRIORITY_HIGH);
        g_source_attach(mSource, mContext);
    }
    else
    {
        onError(errors->message);
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
}

void Peer::send(const std::string& message, unsigned timeout /*= 1*/) const
{
    if (!isConnected())
        return;

    GError *errors = nullptr;
    BIND_TO_SCOPE(errors);

    if (g_socket_get_timeout(mSendSocket) != timeout)
        g_socket_set_timeout(mSendSocket, timeout);

    if (g_socket_send_to(mSendSocket, mCastAddress, message.c_str(), message.size(), nullptr, &errors) < 0)
    {
        onError(errors->message);
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

}
