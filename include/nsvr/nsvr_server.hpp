#pragma once

#include <string>
#include <unordered_map>

#include <gio/gio.h>

namespace nsvr {

/*!
 * @class Server
 * @brief Implements a TCP server. It can also
 * send UDP messages to clients.
 */
class Server
{
public:
    explicit Server();
    virtual ~Server();

    //! start listening for TCP packets on given ports
    bool listen(short port);

    //! processes pending events received from Clients
    void iterate();

    //! answers true if underlying TCP listener is running
    bool isListening();

    //! stops listening on TCP port given to listen() last time
    void shutdown();

    //! broadcast a UDP message to known Clients
    void broadcastToClients(const std::string& message);

    //! gets the TCP port we are listening on
    short getPort() const;

protected:
    //! invoked when a TCP message is received from Clients (member)
    virtual void onMessage(const std::string& message) { /* no-op */ }

private:
    //! invoked when a TCP message is received from Clients (non-member)
    static gboolean incoming(GSocketService *service, GSocketConnection *connection, GObject *source, gpointer self);

private:
    /*
     * @class Server::UdpEndpoint
     * @brief Implements a UDP endpoint which can be used to broadcast messages to
     */
    class UdpEndpoint
    {
    public:
        UdpEndpoint();
        ~UdpEndpoint();

        //! sets up this endpoint to point to given ip address and port
        void setup(const std::string& ip, short port);

        //! sends a message to the endpoint last set up with setup()
        void send(const std::string& message, unsigned timeout = 1) const;

    private:
        GSocket         *mEndpointSocket;
        GSocketAddress  *mEndpointAddress;
    };

private:
    using EndpointMap   = std::unordered_map < std::string, UdpEndpoint > ;
    EndpointMap         mEndpoints;
    GSocketService*     mSocketService;
    short               mPort;
};

}
