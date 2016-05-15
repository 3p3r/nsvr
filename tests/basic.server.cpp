#include "cinder/app/RendererGl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"

#include "nsvr.hpp"

// simple BGRA to RGBA shader
namespace {
const std::string VERTEX_SHADER = "#version 330 core\n"
    "in vec4 ciPosition;\n"
    "in vec2 ciTexCoord0;\n"
    "uniform mat4 ciModelViewProjection;\n"
    "smooth out vec2 vTexCoord;\n"
    "void main() {\n"
    "  gl_Position = ciModelViewProjection * ciPosition;\n"
    "  vTexCoord = vec2(ciTexCoord0.x, 1.0 - ciTexCoord0.y);\n"
    "}";

const std::string FRAG_SHADER = "#version 330 core\n"
    "uniform sampler2D uTex0;\n"
    "smooth in vec2 vTexCoord;\n"
    "out vec4 fColor;\n"
    "void main()\n"
    "{\n"
    "  fColor = texture( uTex0, vTexCoord ).bgra;\n"
    "}\n";
}

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace nsvr;

class BasicServer : public App {
public:
    void                setup() override;
    void                update() override;
    void                draw() override;
    void                keyDown(KeyEvent event) override;
    void                fileDrop(FileDropEvent event) override;
    void                openVideo(const std::string& path);

private:
    class CinderPlayer : public PlayerServer
    {
    public:
        CinderPlayer();
        gl::TextureRef  mTexture;
        virtual void    onFrame(guchar* buf, gsize size) const override;
        virtual void    onError(const gchar* msg) const override;
        virtual void    onState(GstState old) const override;
        virtual void    onStreamEnd() const override;
        virtual void    onMessage(const std::string& message) override;
    };

private:
    gl::GlslProgRef     mBgra2RgbaShader;
    gl::TextureRef      mTexture;
    CinderPlayer        mPlayer;
};

BasicServer::CinderPlayer::CinderPlayer()
    : PlayerServer("127.0.0.1", 5000)
{}

void BasicServer::CinderPlayer::onFrame(guchar* buf, gsize size) const
{
    if (mTexture)
        mTexture->update(buf, GL_RGBA, GL_UNSIGNED_BYTE, 0, getWidth(), getHeight());
}

void BasicServer::CinderPlayer::onError(const gchar* msg) const
{
    std::cerr << "Error received from GStreamer: " << msg << std::endl;
}

void BasicServer::CinderPlayer::onStreamEnd() const
{
    std::cout << "EOS event received from GStreamer." << std::endl;
}

void BasicServer::CinderPlayer::onMessage(const std::string& message)
{
    PlayerServer::onMessage(message);

    if (!message.empty())
        std::cout << "Message: " << message << std::endl;
}

void BasicServer::CinderPlayer::onState(GstState old) const
{
    std::cout << "GStreamer state changed from: " << old << " to: " << getState() << std::endl;
}

void BasicServer::setup()
{
    std::cout << "Media Player Usage: Drag-Drop a video to play." << std::endl;
    std::cout << "[p]: play" << std::endl;
    std::cout << "[s]: stop" << std::endl;
    std::cout << "[m]: mute" << std::endl;
    std::cout << "[l]: loop" << std::endl;
    std::cout << "[c]: close" << std::endl;
    std::cout << "[f]: full-screen" << std::endl;
    std::cout << "[r]: reverse-playback" << std::endl;

    mBgra2RgbaShader = gl::GlslProg::create(VERTEX_SHADER, FRAG_SHADER);
}

void BasicServer::keyDown(KeyEvent event)
{
    switch (event.getChar())
    {
    case 'p': case 'P':
        mPlayer.getState() == GST_STATE_PLAYING ? mPlayer.pause() : mPlayer.play();
        break;
    case 's': case 'S':
        mPlayer.stop();
        break;
    case 'm': case 'M':
        mPlayer.setMute(!mPlayer.getMute());
        break;
    case 'l': case 'L':
        mPlayer.setLoop(!mPlayer.getLoop());
        break;
    case 'c': case 'C':
        mPlayer.close();
        break;
    case 'f': case 'F':
        setFullScreen(!isFullScreen());
        break;
    case 'r': case 'R':
        mPlayer.setRate(-1. * mPlayer.getRate());
        break;
    default:
        break;
    }

    if (event.getCode() == event.KEY_ESCAPE)
        quit();

    if (event.getCode() == event.KEY_UP) {
        mPlayer.setVolume(mPlayer.getVolume() + .2);
    } else if (event.getCode() == event.KEY_DOWN) {
        mPlayer.setVolume(mPlayer.getVolume() - .2);
    } else if (event.getCode() == event.KEY_LEFT) {
        mPlayer.setTime( mPlayer.getTime() - 5. );
    } else if (event.getCode() == event.KEY_RIGHT) {
        mPlayer.setTime( mPlayer.getTime() + 5. );
    }
}

void BasicServer::fileDrop(FileDropEvent event)
{
    if (event.getNumFiles() > 0) { openVideo(event.getFile(0).generic_string()); }
}

void BasicServer::openVideo(const std::string& path)
{
    mPlayer.mTexture    = nullptr;
    mTexture            = nullptr;

    if (mPlayer.open(path.c_str()))
    {
        if (mPlayer.getWidth() > 0 && mPlayer.getHeight() > 0)
        {
            mTexture = gl::Texture::create(mPlayer.getWidth(), mPlayer.getHeight());
            mPlayer.mTexture = mTexture;
        }

        mPlayer.play();
        mPlayer.setMute(true);
    }
}

void BasicServer::update()
{
    mPlayer.update();
}

void BasicServer::draw()
{
    gl::clear();

    if (mBgra2RgbaShader && mTexture)
    {
        gl::ScopedGlslProg      bind(mBgra2RgbaShader);
        gl::ScopedTextureBind   texb(mTexture);
        mBgra2RgbaShader->uniform("uTex0", 0);
        gl::drawSolidRect(getWindowBounds());
    }

    if (mPlayer.getState() == GST_STATE_NULL)
    {
        ci::gl::drawString("Drag and drop a media file to play.", glm::vec2(10, 10));
    }
    else
    {
        ci::gl::drawString(std::to_string(getAverageFps()), glm::vec2(10, 10));
    }
}

CINDER_APP(BasicServer, RendererGl, [](App::Settings* settings)
{
    settings->setConsoleWindowEnabled();
})
