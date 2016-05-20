#pragma once

#include <string>
#include <unordered_map>

#include <glib.h>
#include <gio/gio.h>

namespace nsvr {

class Peer
{
public:
    explicit        Peer();
    virtual         ~Peer();
    bool            connect(const std::string& mutlicast_group, short multicast_port);
    bool            isConnected() const;
    void            disconnect();
    void            send(const std::string& message);
    void            iterate();

protected:
    virtual void    onMessage(const std::string& message) { /* no-op */ }

private:
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

class Server
{
public:
    explicit            Server();
    virtual             ~Server();

    bool                listen(short port);
    void                iterate();
    bool                isConnected();
    void                disconnect();
    void                broadcast(const std::string& message);
    short               getPort() const;

protected:
    virtual void        onMessage(const std::string& message) { /* no-op */ }

private:
    static gboolean     incoming(GSocketService *service, GSocketConnection *connection, GObject *source_object, gpointer user_data);

private:
    class UdpEndpoint
    {
    public:
        UdpEndpoint();
        ~UdpEndpoint();
        void            connect(const std::string& ip, short port);
        void            send(const std::string& message, unsigned timeout = 1);

    private:
        GSocket         *mEndpointSocket;
        GSocketAddress  *mEndpointAddress;
    };

private:
    std::unordered_map<std::string, UdpEndpoint>    mEndpoints;
    GSocketService*                                 mSocketService;
    short                                           mPort;
};

}
