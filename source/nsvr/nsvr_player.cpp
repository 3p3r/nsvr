#include "nsvr_internal.hpp"

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_discoverer.hpp"

#include <gst/app/gstappsink.h>

namespace nsvr
{

Player::Player()
    : mPlayerState(GST_PLAYER_STATE_STOPPED)
    , mGstPipeline(nullptr)
    , mGstPlayer(nullptr)
    , mReady(false)
{
    if (!internal::gstreamerInitialized())
    {
        NSVR_LOG("Player requires GStreamer to be initialized.");
        return;
    }
}

bool Player::isReady() const
{
    return mReady;
}

bool Player::isGstPlayerValid() const
{
    return (mGstPlayer != nullptr) && (mGstPipeline != nullptr);
}

bool Player::isVideoSinkValid() const
{
    if (!isGstPlayerValid())
        return false;

    gpointer current_videosink = nullptr;
    g_object_get(mGstPipeline, "video-sink", &current_videosink, nullptr);

    return (current_videosink != nullptr);
}

void Player::freeGstPlayer()
{
    if (!isGstPlayerValid())
        return;

    BIND_TO_SCOPE(mGstPipeline);
    BIND_TO_SCOPE(mGstPlayer);
}

bool Player::makeGstPlayer()
{
    bool success = true;

    if (!(mGstPlayer = gst_player_new(nullptr, nullptr)))
    {
        NSVR_LOG("Unable to construct GstPlayer instance.");
        success = false;
    }

    if (!(mGstPipeline = gst_player_get_pipeline(mGstPlayer)))
    {
        NSVR_LOG("Unable to obtain GstPlayer's pipeline.");
        success = false;
    }

    return success;
}

void Player::freeVideoSink()
{
    if (!isGstPlayerValid() || !isVideoSinkValid())
        return;

    gpointer current_videosink = nullptr;
    g_object_get(mGstPipeline, "video-sink", &current_videosink, nullptr);

    if (current_videosink != nullptr)
    {
        if (auto current_videosink_element = GST_ELEMENT(current_videosink))
        {
            gst_object_unref(current_videosink_element);
            g_object_set(mGstPipeline, "video-sink", nullptr, nullptr);
        }
        else
        {
            NSVR_LOG("FreeVideoSink was called but element cast failed.");
            return;
        }
    }
    else
    {
        NSVR_LOG("FreeVideoSink was called but no video-sink is set.");
        return;
    }
}

bool Player::makeVideoSink(int width, int height, const std::string& fmt)
{
    bool success = false;

    if (auto appsink = gst_element_factory_make("appsink", nullptr))
    {
        if (auto videosink = GST_APP_SINK(appsink))
        {
            gst_app_sink_set_drop(videosink, TRUE);
            gst_app_sink_set_max_buffers(videosink, 8);
            gst_app_sink_set_emit_signals(videosink, FALSE);

            GstAppSinkCallbacks     callbacks;
            callbacks.eos           = nullptr;
            callbacks.new_preroll   = reinterpret_cast<decltype(callbacks.new_preroll)>(onPreroll);
            callbacks.new_sample    = reinterpret_cast<decltype(callbacks.new_sample)>(onSample);

            gst_app_sink_set_callbacks(videosink, &callbacks, this, nullptr);

            if (auto video_caps = gst_caps_new_simple(
                "video/x-raw",
                "format", G_TYPE_STRING, (fmt.empty() ? "BGRA" : fmt.c_str()),
                "width", G_TYPE_INT, width,
                "height", G_TYPE_INT, height,
                nullptr))
            {
                BIND_TO_SCOPE(video_caps);
                gst_app_sink_set_caps(videosink, video_caps);
            }
            else
            {
                NSVR_LOG("GstCaps construction failed.");
                success = false;
            }

            g_object_set(mGstPipeline, "video-sink", appsink, nullptr);
            success = true;
        }
        else
        {
            NSVR_LOG("Element cast to AppSink failed.");
            success = false;
        }

        if (auto videosink = GST_BASE_SINK(appsink))
        {
            gst_base_sink_set_sync(videosink, TRUE);
            gst_base_sink_set_qos_enabled(videosink, TRUE);
            gst_base_sink_set_async_enabled(videosink, FALSE);
            gst_base_sink_set_max_lateness(videosink, GST_CLOCK_TIME_NONE);
        }
        else
        {
            NSVR_LOG("Element cast to BaseSink failed.");
            success = false;
        }
    }
    else
    {
        NSVR_LOG("Unable to construct video AppSink.");
        success = false;
    }

    return success;
}

Player::~Player()
{
    close();
}

bool Player::open(const std::string& path, gint width, gint height, const std::string& fmt)
{
    if (!internal::gstreamerInitialized())
    {
        NSVR_LOG("Player requires GStreamer to be initialized.");
        return false;
    }

    bool gst_player_valid = isGstPlayerValid() || makeGstPlayer();

    if (isVideoSinkValid())
        freeVideoSink();

    bool video_sink_valid = makeVideoSink(width, height, fmt);

    mReady = gst_player_valid && video_sink_valid;

    if (!isReady())
    {
        NSVR_LOG("Player is not ready to play. Construction failed.");
        return false;
    }

    if (path.empty())
    {
        NSVR_LOG("Path given to Player is empty.");
        return false;
    }

    onBeforeOpen();

    bool success = false;
    Discoverer discoverer;

    if (discoverer.open(path))
    {
        gst_player_set_uri(mGstPlayer, discoverer.getMediaUri().c_str());

        pause();
        setupClock();

        mWidth = discoverer.getWidth();
        mHeight = discoverer.getHeight();

        success = true;
    }
    else
    {
        NSVR_LOG("Unable to discover the media: " << path);
        success = false;
    }

    return success;
}

bool Player::open(const std::string& path, gint width, gint height)
{
    return open(path, width, height, "BGRA");
}

bool Player::open(const std::string& path, const std::string& fmt)
{
    Discoverer discoverer;
    return discoverer.open(path) && open(path, discoverer.getWidth(), discoverer.getHeight(), fmt);
}

bool Player::open(const std::string& path)
{
    Discoverer discoverer;
    return discoverer.open(path) && open(path, discoverer.getWidth(), discoverer.getHeight());
}

void Player::close()
{
    onBeforeClose();

    stop();

    if (isVideoSinkValid())
        freeVideoSink();

    if (isGstPlayerValid())
        freeGstPlayer();

    if (mCurrentBuffer != nullptr) gst_buffer_unmap(mCurrentBuffer, &mCurrentMapInfo);
    if (mCurrentSample != nullptr) gst_sample_unref(mCurrentSample);

    reset();
}

void Player::setState(GstPlayerState state)
{
    if (state == GST_PLAYER_STATE_PAUSED)
        pause();
    else if (state == GST_PLAYER_STATE_PLAYING)
        play();
    else if (state == GST_PLAYER_STATE_STOPPED)
        stop();
}

GstPlayerState Player::getState() const
{
    return mPlayerState;
}

void Player::stop()
{
    gst_player_stop(mGstPlayer);
}

void Player::play()
{
    gst_player_play(mGstPlayer);
}

void Player::replay()
{
    stop();
    play();
}

void Player::pause()
{
    if (!isReady())
        return;

    gst_player_pause(mGstPlayer);
}

void Player::update()
{
    onBeforeUpdate();

    if (mBufferDirty)
    {
        onVideoFrame(
            mCurrentMapInfo.data,
            mCurrentMapInfo.size);

        // free current resources on previous frame
        if (mCurrentBuffer) gst_buffer_unmap(mCurrentBuffer, &mCurrentMapInfo);
        if (mCurrentSample) gst_sample_unref(mCurrentSample);

        mCurrentBuffer = nullptr;
        mCurrentSample = nullptr;

        // Signal Streaming thread it can produce
        mBufferDirty = false;
    }
}

gdouble Player::getDuration() const
{
    return static_cast<gdouble>(GST_TIME_AS_SECONDS(gst_player_get_duration(mGstPlayer)));
}

void Player::setLoop(bool on)
{
    g_return_if_fail(mLoop != on);
    mLoop = on;
}

bool Player::getLoop() const
{
    return mLoop;
}

void Player::setTime(gdouble time)
{
    gst_player_seek(mGstPlayer, static_cast<GstClockTime>(CLAMP(time, 0, getDuration()) * GST_SECOND));
}

gdouble Player::getTime() const
{
    return static_cast<gdouble>(GST_TIME_AS_SECONDS(gst_player_get_position(mGstPlayer)));
}

void Player::setVolume(gdouble vol)
{
    gst_player_set_volume(mGstPlayer, CLAMP(vol, 0., 1.));
}

gdouble Player::getVolume() const
{
    return gst_player_get_volume(mGstPlayer);
}

void Player::setMute(bool on)
{
    gst_player_set_mute(mGstPlayer, (on ? TRUE : FALSE));
}

bool Player::getMute() const
{
    return gst_player_get_mute(mGstPlayer) != FALSE;
}

gint Player::getWidth() const
{
    return mWidth;
}

gint Player::getHeight() const
{
    return mHeight;
}

void Player::reset()
{
    mPlayerState    = GST_PLAYER_STATE_STOPPED;
    mCurrentBuffer  = nullptr;
    mCurrentSample  = nullptr;
    mWidth          = 0;
    mHeight         = 0;
    mBufferDirty    = false;
}

GstFlowReturn Player::onPreroll(GstElement* appsink, Player* player)
{
    // Here's our chance to get the actual dimension of the media.
    // The actual dimension might be slightly different from what
    // is passed into and requested from the pipeline.

    if (GstSample* sample = gst_app_sink_pull_preroll(GST_APP_SINK(appsink)))
    {
        if (GstCaps *caps = gst_sample_get_caps(sample))
        {
            if (gst_caps_is_fixed(caps) != FALSE)
            {
                if (GstStructure *str = gst_caps_get_structure(caps, 0))
                {
                    if (gst_structure_get_int(str, "width", &player->mWidth) == FALSE ||
                        gst_structure_get_int(str, "height", &player->mHeight) == FALSE)
                    {
                        NSVR_LOG("No width/height information available.");
                    }
                }
            }
            else
            {
                NSVR_LOG("caps is not fixed for this media.");
            }
        }

        if (player)
            player->processSample(sample);
    }

    return GST_FLOW_OK;
}

GstFlowReturn Player::onSample(GstElement* appsink, Player* player)
{
    if (GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink)))
    {
        if (player)
            player->processSample(sample);
    }

    return GST_FLOW_OK;
}

void Player::processSample(GstSample* const sample)
{
    // Check if UI thread has consumed the last frame
    if (mBufferDirty)
    {
        // Simply, skip this sample. UI is not consuming fast enough.
        gst_sample_unref(sample);
    }
    else
    {
        // Acquire and hold onto the new frame (until UI consumes it)
        mCurrentSample = sample;
        mCurrentBuffer = gst_sample_get_buffer(sample);
        gst_buffer_map(mCurrentBuffer, &mCurrentMapInfo, GST_MAP_READ);

        // Signal UI thread it can consume
        mBufferDirty = true;
    }
}

}
