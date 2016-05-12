#include "nsvr/nsvr_peer.hpp"
#include "nsvr_internal.hpp"

namespace nsvr {

Peer::Peer()
    : mSocket(nullptr)
    , mListenGroup(nullptr)
    , mCastGroup(nullptr)
    , mListenAddress(nullptr)
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

    mSocket = g_socket_new(
        GSocketFamily::G_SOCKET_FAMILY_IPV4,
        GSocketType::G_SOCKET_TYPE_DATAGRAM,
        GSocketProtocol::G_SOCKET_PROTOCOL_UDP,
        &errors);

    if (!mSocket)
    {
        onError(errors->message);
        return false;
    }

    g_socket_set_multicast_loopback(mSocket, TRUE);
    g_socket_set_blocking(mSocket, FALSE);

    mListenGroup = g_inet_address_new_any(GSocketFamily::G_SOCKET_FAMILY_IPV4);
    mCastGroup = g_inet_address_new_from_string(mutlicast_group.c_str());
    mListenAddress = g_inet_socket_address_new(mListenGroup, multicast_port);

    if (!mListenGroup || !mCastGroup || !mListenAddress)
    {
        onError("One of the addresses could not be constructed.");
        return false;
    }

    if (g_socket_bind(mSocket, mListenAddress, TRUE, &errors) != FALSE &&
        g_socket_join_multicast_group(mSocket, mCastGroup, FALSE, nullptr, &errors) != FALSE)
    {
        mContext = g_main_context_new();
        mSource = g_socket_create_source(mSocket, G_IO_IN, nullptr);

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

    g_clear_object(&mListenAddress);
    g_clear_object(&mCastGroup);
    g_clear_object(&mListenGroup);
    g_clear_object(&mSocket);
}

void Peer::send(const std::string& message) const
{
    if (!isConnected())
        return;
}

void Peer::iterate()
{
    g_main_context_iteration(mContext, FALSE);
}

}
