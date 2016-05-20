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


class ServerPeer : public App, private Server {
public:
    void    setup() override;
    void    update() override;
    void    onMessage(const std::string& message) override;
};

void ServerPeer::onMessage(const std::string& message)
{
    std::cout << "Peer message: " << message << std::endl;
}

void ServerPeer::setup()
{
    listen(5000);
}

void ServerPeer::update()
{
    iterate();
}

CINDER_APP(ServerPeer, RendererGl, [](App::Settings* settings)
{
    settings->setConsoleWindowEnabled();
})
