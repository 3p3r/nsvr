#pragma once

#include <string>

#include <gio/gio.h>

namespace nsvr {

/*!
 * @class Client
 * @brief Implements a UDP client. It can also
 * send TCP messages to server.
 */
class Client
{
public:
    explicit Client();
    virtual ~Client();

    //! connects to Server at given address and port
    bool connect(const std::string& server_address, short server_port);

    //! answers true if underlying connection is established with Server
    bool isConnected() const;

    //! disconnects from Server. No-op if disconnected already.
    void disconnect();

    //! sends a TCP message to Server
    void sendToServer(const std::string& message);

    //! processes pending events received from Server
    void iterate();

    //! returns the address of last server passed to connect
    const std::string& getServerAddress() const;
    
    //! returns the port of last server passed to connect
    short getServerPort() const;

protected:
    //! invoked when a UDP message is received from Server (member)
    virtual void onMessage(const std::string& message) { /* no-op */ }

private:
    //! invoked when a UDP message is received from Server (non-member)
    static gboolean incomming(GSocket *socket, GIOCondition condition, gpointer self);

private:
    std::string     mServerAddress;
    GSocket         *mReceiveSocket;
    GSocketAddress  *mListenAddress;
    GMainContext    *mContext;
    GSource         *mSource;
    short           mServerPort;
    bool            mConnected;
};

}
