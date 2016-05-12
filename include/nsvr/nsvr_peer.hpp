#pragma once

#include <gio/gio.h>
#include <string>

namespace nsvr {

class Peer
{
public:
    explicit        Peer();
    virtual         ~Peer();
    bool            connect(const std::string& mutlicast_group, short multicast_port);
    bool            isConnected() const;
    void            disconnect();
    void            send(const std::string& message, unsigned timeout = 1);
    void            iterate();

protected:
    virtual void    onMessage(const std::string& message) { /* no-op */ }
    virtual void    onError(const std::string& error) { /* no-op */ }

private:
    friend          class Internal;
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

}
