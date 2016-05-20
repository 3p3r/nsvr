#include "cinder/app/RendererGl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"

#include "nsvr/nsvr_client.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace nsvr;


class ClientPeer : public App, private Client {
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
    sendToServer("nsvr");
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
