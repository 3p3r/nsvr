#include "nsvr_internal.hpp"

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_discoverer.hpp"

#include <gst/app/gstappsink.h>

namespace nsvr
{

Player::Player()
{
    reset_state();

    if (!internal::gstreamerInitialized())
    {
        NSVR_LOG("Player requires GStreamer to be initialized.");
        return;
    }
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

bool Player::isContextValid() const
{
    return (mPlayerContext != nullptr);
}

void Player::freeContext()
{
    if (!isContextValid())
        return;

    g_main_context_unref(mPlayerContext);
    mPlayerContext = nullptr;
}

bool Player::makeContext()
{
    return (mPlayerContext = g_main_context_new()) != nullptr;
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
    bool success = false;

    if (isContextValid())
        freeContext();

    if (!makeContext())
    {
        NSVR_LOG("Unable to construct player context.");
        success = false;
    }
    else if (!(mGstPlayer = gst_player_new(nullptr, gst_player_g_main_context_signal_dispatcher_new(mPlayerContext))))
    {
        NSVR_LOG("Unable to construct GstPlayer instance.");
        success = false;
    }
    else if (!(mGstPipeline = gst_player_get_pipeline(mGstPlayer)))
    {
        NSVR_LOG("Unable to obtain GstPlayer's pipeline.");
        success = false;
    }
    else
    {
        g_signal_connect(mGstPlayer, "buffering", G_CALLBACK(on_buffering), this);
        g_signal_connect(mGstPlayer, "duration-changed", G_CALLBACK(on_duration_changed), this);
        g_signal_connect(mGstPlayer, "end-of-stream", G_CALLBACK(on_end_of_stream), this);
        g_signal_connect(mGstPlayer, "error", G_CALLBACK(on_error), this);
        g_signal_connect(mGstPlayer, "mute-changed", G_CALLBACK(on_mute_changed), this);
        g_signal_connect(mGstPlayer, "position-updated", G_CALLBACK(on_position_updated), this);
        g_signal_connect(mGstPlayer, "seek-done", G_CALLBACK(on_seek_done), this);
        g_signal_connect(mGstPlayer, "state-changed", G_CALLBACK(on_state_changed), this);
        g_signal_connect(mGstPlayer, "video-dimensions-changed", G_CALLBACK(on_video_dimensions_changed), this);
        g_signal_connect(mGstPlayer, "volume-changed", G_CALLBACK(on_volume_changed), this);
        g_signal_connect(mGstPlayer, "warning", G_CALLBACK(on_warning), this);

        success = true;
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
            callbacks.new_preroll   = reinterpret_cast<decltype(callbacks.new_preroll)>(on_videosink_preroll);
            callbacks.new_sample    = reinterpret_cast<decltype(callbacks.new_sample)>(on_videosink_sample);

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

    if (path.empty())
    {
        NSVR_LOG("Path given to Player is empty.");
        return false;
    }

    close();

    if (!isGstPlayerValid() && !makeGstPlayer())
    {
        NSVR_LOG("Unable to construct GstPlayer instance.");
        return false;
    }

    if (isVideoSinkValid())
        freeVideoSink();

    if (!makeVideoSink(width, height, fmt))
    {
        NSVR_LOG("Unable to construct video's AppSink instance.");
        return false;
    }

    if (mDiscoverer.open(path))
    {
        gst_player_set_uri(mGstPlayer, mDiscoverer.getMediaUri().c_str());
        pause(); // pause to obtain preview sample (same as callbacks)
        onClockSetup();

        mWidth = mDiscoverer.getWidth();
        mHeight = mDiscoverer.getHeight();
        mDuration = mDiscoverer.getDuration();

        return true;
    }
    else
    {
        NSVR_LOG("Unable to discover media: " << mDiscoverer.getMediaUri());
        return false;
    }
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
    onClose();
    onClockClear();

    stop();

    if (isVideoSinkValid())
        freeVideoSink();

    if (isGstPlayerValid())
        freeGstPlayer();

    if (mCurrentBuffer != nullptr) gst_buffer_unmap(mCurrentBuffer, &mCurrentMapInfo);
    if (mCurrentSample != nullptr) gst_sample_unref(mCurrentSample);

    reset_state();
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
    return mState;
}

void Player::stop()
{
    g_return_if_fail(isGstPlayerValid());
    gst_player_stop(mGstPlayer);
}

void Player::play()
{
    g_return_if_fail(isGstPlayerValid());
    gst_player_play(mGstPlayer);
}

void Player::replay()
{
    stop();
    play();
}

void Player::pause()
{
    g_return_if_fail(isGstPlayerValid()); 
    gst_player_pause(mGstPlayer);
}

void Player::update()
{
    onUpdate();

    if (isContextValid())
    {
        while (g_main_context_pending(mPlayerContext) != FALSE)
            g_main_context_iteration(mPlayerContext, FALSE);
    }

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
    return mDuration;
}

void Player::setLoop(bool on)
{
    mLoop = on;
}

bool Player::getLoop() const
{
    return mLoop;
}

void Player::setTime(gdouble time)
{
    g_return_if_fail(isGstPlayerValid());

    mSeeking = true;
    gst_player_seek(mGstPlayer, static_cast<GstClockTime>(CLAMP(time, 0, getDuration()) * GST_SECOND));
}

gdouble Player::getTime() const
{
    return mPosition;
}

void Player::setVolume(gdouble vol)
{
    g_return_if_fail(isGstPlayerValid());
    gst_player_set_volume(mGstPlayer, CLAMP(vol, 0., 1.));
}

gdouble Player::getVolume() const
{
    return mVolume;
}

void Player::setMute(bool on)
{
    g_return_if_fail(isGstPlayerValid());
    gst_player_set_mute(mGstPlayer, (on ? TRUE : FALSE));
}

bool Player::getMute() const
{
    return mMute;
}

gint Player::getWidth() const
{
    return mWidth;
}

gint Player::getHeight() const
{
    return mHeight;
}

void Player::reset_state()
{
    mDiscoverer     .reset();
    mBufferDirty    = false;
    mCurrentMapInfo = GstMapInfo();
    mCurrentSample  = nullptr;
    mCurrentBuffer  = nullptr;
    mGstPlayer      = nullptr;
    mGstPipeline    = nullptr;
    mPlayerContext  = nullptr;
    mState          = GST_PLAYER_STATE_STOPPED;
    mDuration       = 0.;
    mPosition       = 0.;
    mVolume         = 1.;
    mWidth          = 0;
    mHeight         = 0;
    mSeeking        = false;
    mMute           = false;
    mLoop           = false;
}

GstFlowReturn Player::on_videosink_preroll(GstElement* appsink, Player* player)
{
    if (GstSample* sample = gst_app_sink_pull_preroll(GST_APP_SINK(appsink)))
        player->extract_sample(sample);

    return GST_FLOW_OK;
}

GstFlowReturn Player::on_videosink_sample(GstElement* appsink, Player* player)
{
    if (GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink)))
        player->extract_sample(sample);

    return GST_FLOW_OK;
}

void Player::extract_sample(GstSample* sample)
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

void Player::on_buffering(GstPlayer* /* player */, gint percent, Player* self)
{
    self->onBuffering(percent);
}

void Player::on_duration_changed(GstPlayer* /* player */, GstClockTime duration, Player* self)
{
    auto new_duration = static_cast<double>(GST_TIME_AS_SECONDS(duration));

    if (new_duration != self->mDuration)
    {
        self->mDuration = new_duration;
        self->onDurationChanged();
    }
}

void Player::on_end_of_stream(GstPlayer* /* player */, Player* self)
{
    self->onEndOfStream();

    if (self->getLoop())
        self->play();
}

void Player::on_error(GstPlayer* /* player */, GError* error, Player* self)
{
    self->onError(error->message);
}

void Player::on_mute_changed(GstPlayer* player, Player* self)
{
    auto new_mute = gst_player_get_mute(player) != FALSE;

    if (new_mute != self->mMute)
    {
        self->mMute = new_mute;
        self->onMuteChanged();
    }
}

void Player::on_position_updated(GstPlayer* /* player */, GstClockTime position, Player* self)
{
    auto new_position = static_cast<double>(GST_TIME_AS_SECONDS(position));

    if (new_position != self->mPosition)
    {
        self->mPosition = new_position;
        self->onPositionChanged();
    }
}

void Player::on_state_changed(GstPlayer* /* player */, GstPlayerState state, Player* self)
{
    self->mState = state;
    self->onStateChanged();
}

void Player::on_seek_done(GstPlayer* /* player */, guint64 position, Player* self)
{
    self->mSeeking = false;
    self->onSeekDone();
}

void Player::on_video_dimensions_changed(GstPlayer* /* player */, gint width, gint height, Player* self)
{
    if (self->mWidth != width || self->mHeight != height)
    {
        self->mWidth = width;
        self->mHeight = height;
        self->onVideoDimensionChanged();
    }
}

void Player::on_volume_changed(GstPlayer* player, Player* self)
{
    auto new_volume = gst_player_get_volume(player);

    if (new_volume != self->mVolume)
    {
        self->mVolume = new_volume;
        self->onVolumeChanged();
    }
}

void Player::on_warning(GstPlayer* /* player */, GError* warning, Player* self)
{
    self->onWarning(warning->message);
}

}
