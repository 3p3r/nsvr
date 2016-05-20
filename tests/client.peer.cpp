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


class ClientPeer : public App {
public:
    void    setup() override;
    void    update() override;
    void    draw() override;
    void    mouseDown(MouseEvent event) override;
};

void ClientPeer::setup()
{
    
}

void ClientPeer::mouseDown(MouseEvent event)
{
    std::stringstream ss;
    ss
        << "Thread heartbeat: "
        << std::this_thread::get_id()
        << " at Epoch timestamp: "
        << std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

void ClientPeer::update()
{

}

void ClientPeer::draw()
{}

CINDER_APP(ClientPeer, RendererGl, [](App::Settings* settings)
{
    settings->setConsoleWindowEnabled();
})
