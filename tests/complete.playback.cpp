#include "cinder/app/RendererGl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"

#include "nsvr.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;

class NgwCinderApp : public App {
public:
    void    setup() override;
    void    update() override;
    void    draw() override;
};

void NgwCinderApp::setup()
{}

void NgwCinderApp::update()
{}

void NgwCinderApp::draw()
{}

CINDER_APP(NgwCinderApp, RendererGl, [](App::Settings* settings)
{
    settings->setConsoleWindowEnabled();
})
