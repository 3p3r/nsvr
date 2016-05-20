#pragma once

#include <string>

#include <glib.h>
#include <gio/gio.h>

namespace nsvr {

/*!
 * @class Peer
 * @brief Implements a UDP multicast peer
 * communication channel. Architecture is P2P.
 */
class Peer
{
public:
    //! Constructs an empty Peer. Ready to join the multicast group
    explicit        Peer();

    //! Closes the Peer and disconnects from the multicast group
    virtual         ~Peer();

    //! Attempts to connect to supplied UDP multicast group/port. Returns true on success
    bool            connect(const std::string& mutlicast_group, short multicast_port);

    //! Returns true if last call to connect() succeeded
    bool            isConnected() const;

    //! Disconnects and leaves the connected multicast group. no-op if already connected
    void            disconnect();

    //! Send a message to the connected multicast group. Timeout is optional (default: 1 second)
    void            send(const std::string& message, unsigned timeout = 1);

    //! Should be called periodically to process incoming messages. Call this in your Update loop.
    void            iterate();

protected:
    //! Called upon reception of messages from other Peers
    virtual void    onMessage(const std::string& message) { /* no-op */ }

private:
    //! Called by Socket Source (event processor) when data is available on mReceiveSocket
    static gboolean onSocketData(GSocket *socket, GIOCondition condition, gpointer self);

private:
    GSocket         *mSendSocket;
    GSocket         *mReceiveSocket;
    GInetAddress    *mListenGroup;
    GInetAddress    *mCastGroup;
    GSocketAddress  *mListenAddress;
    GSocketAddress  *mCastAddress;
    GMainContext    *mContext;
    GSource         *mSource;
    bool            mConnected;
};

class Server
{
public:
    explicit Server();
    virtual ~Server();

    bool listen(short port);
    void iterate();
    bool isConnected();
    void disconnect();
    void broadcast(const std::string& message);

protected:
    virtual void onMessage(const std::string& message) {}

private:
    static gboolean incoming(GSocketService *service, GSocketConnection *connection, GObject *source_object, gpointer user_data);

private:
    GSocketService* mSocketService;
};

}
