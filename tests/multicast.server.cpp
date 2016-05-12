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


class MulticastServer : public App, private Peer {
public:
    void    setup() override;
    void    update() override;
    void    draw() override;

    void onMessage(const std::string& message) override;
    void onError(const std::string& message) override;
};

void MulticastServer::onMessage(const std::string& message)
{
    std::cout << "Peer message: " << message << std::endl;
}

void MulticastServer::onError(const std::string& error)
{
    std::cout << "Peer error: " << error << std::endl;
}

void MulticastServer::setup()
{
    connect("225.0.0.37", 12345);
}

void MulticastServer::update()
{
    iterate();
}

void MulticastServer::draw()
{}

CINDER_APP(MulticastServer, RendererGl, [](App::Settings* settings)
{
    settings->setConsoleWindowEnabled();
})
