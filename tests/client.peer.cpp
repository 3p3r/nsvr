#include "cinder/app/RendererGl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"

#include "nsvr/nsvr_peer.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace nsvr;


class ClientPeer : public App, private Peer {
public:
    void    setup() override;
    void    update() override;
    void    mouseDown(MouseEvent event) override;
    void    onMessage(const std::string& message) override;
};

void ClientPeer::setup()
{
    connect("127.0.0.1", 5000);
}

void ClientPeer::mouseDown(MouseEvent event)
{
    std::stringstream ss;
    ss
        << "Thread heartbeat: "
        << std::this_thread::get_id()
        << " at Epoch timestamp: "
        << std::chrono::high_resolution_clock::now().time_since_epoch().count();

    send(ss.str());
}

void ClientPeer::onMessage(const std::string& message)
{
    std::cout << "Client message: " << message << std::endl;
}

void ClientPeer::update()
{
    iterate();
}

CINDER_APP(ClientPeer, RendererGl, [](App::Settings* settings)
{
    settings->setConsoleWindowEnabled();
})
