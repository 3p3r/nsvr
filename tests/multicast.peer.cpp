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


class MulticastPeer : public App, private Peer {
public:
    void    setup() override;
    void    update() override;
    void    draw() override;
    void    mouseDown(MouseEvent event) override;
    void    onMessage(const std::string& message) const override;
    void    onError(const std::string& message) const override;
};

void MulticastPeer::onMessage(const std::string& message) const
{
    std::cout << "Peer message: " << message << std::endl;
}

void MulticastPeer::onError(const std::string& error) const
{
    std::cout << "Peer error: " << error << std::endl;
}

void MulticastPeer::setup()
{
    connect("239.0.0.1", 5000);
}

void MulticastPeer::mouseDown(MouseEvent event)
{
    std::stringstream ss;
    ss
        << "Thread heartbeat: "
        << std::this_thread::get_id()
        << " at Epoch timestamp: "
        << std::chrono::high_resolution_clock::now().time_since_epoch().count();

    send(ss.str());
}

void MulticastPeer::update()
{
    iterate();
    mouseDown(MouseEvent());
}

void MulticastPeer::draw()
{}

CINDER_APP(MulticastPeer, RendererGl, [](App::Settings* settings)
{
    settings->setConsoleWindowEnabled();
})
