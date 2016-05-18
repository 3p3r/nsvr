#include "nsvr_internal.hpp"

#include "nsvr/nsvr_player.hpp"
#include "nsvr/nsvr_discoverer.hpp"

#include <gst/app/gstappsink.h>

namespace nsvr
{

Player::Player()
    : mLoop(false)
    , mMute(false)
{
    reset();

    if (!internal::gstreamerInitialized())
    {
        NSVR_LOG("Player requires GStreamer to be initialized.");
        return;
    }
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

    close();
    onBeforeOpen();

    if (path.empty())
    {
        NSVR_LOG("Path given to Player is empty.");
        return false;
    }

    GError*     errors = nullptr;
    Discoverer  discoverer;
    BIND_TO_SCOPE(errors);

    if (discoverer.open(path))
    {
        std::stringstream pipeline_cmd;

        if (discoverer.getHasVideo())
        {
            pipeline_cmd
                << "playbin uri=\""
                << discoverer.getMediaUri()
                << "\" video-sink=\"appsink drop=yes async=no qos=yes sync=yes max-lateness=" << GST_SECOND
                << " caps=video/x-raw"
                << ",width=" << width
                << ",height=" << height
                << ",format=" << fmt
                << "\"";
        }
        else if (discoverer.getHasAudio())
        {
            pipeline_cmd
                << "playbin uri=\""
                << discoverer.getMediaUri()
                << "\"";
        }
        else
        {
            NSVR_LOG("Media provided does not contain neither audio nor video.");
            return false;
        }

        mPipeline = gst_parse_launch(pipeline_cmd.str().c_str(), &errors);

        if (mPipeline == nullptr)
        {
            close();
            NSVR_LOG("Unable to launch the pipeline [" << errors->message << "].");
            return false;
        }

        mGstBus = gst_pipeline_get_bus(GST_PIPELINE(mPipeline));

        if (mGstBus == nullptr)
        {
            close();
            NSVR_LOG("Unable to obtain pipeline's event bus [" << errors->message << "].");
            return false;
        }

        if (discoverer.getHasVideo())
        {
            GstAppSink *app_sink = nullptr;
            BIND_TO_SCOPE(app_sink);

            g_object_get(mPipeline, "video-sink", &app_sink, nullptr);

            if (app_sink == nullptr)
            {
                close();
                NSVR_LOG("Unable to obtain pipeline's video sink.");
                return false;
            }

            GstAppSinkCallbacks     callbacks;
            callbacks.eos           = nullptr;
            callbacks.new_preroll   = reinterpret_cast<decltype(callbacks.new_preroll)>(onPreroll);
            callbacks.new_sample    = reinterpret_cast<decltype(callbacks.new_sample)>(onSample);

            gst_app_sink_set_callbacks(scoped_app_sink.pointer, &callbacks, this, nullptr);
        }

        setupClock();

        // Going from NULL => READY => PAUSE forces the
        // pipeline to pre-roll so we can get video dim
        GstState state;
        unsigned timeout = 10;

        setState(GST_STATE_READY);

        if (gst_element_get_state(mPipeline, &state, nullptr, timeout * GST_SECOND) == GST_STATE_CHANGE_FAILURE ||
            state != GST_STATE_READY)
        {
            close();
            NSVR_LOG("Failed to put pipeline in READY state.");
            return false;
        }

        setState(GST_STATE_PAUSED);

        if (gst_element_get_state(mPipeline, &state, nullptr, timeout * GST_SECOND) == GST_STATE_CHANGE_FAILURE ||
            state != GST_STATE_PAUSED)
        {
            close();
            NSVR_LOG("Failed to put pipeline in PAUSE state.");
            return false;
        }

        mDuration = discoverer.getDuration();
    }

    return true;
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

    if (mPipeline != nullptr)      gst_object_unref(mPipeline);
    if (mGstBus != nullptr)        gst_object_unref(mGstBus);
    if (mCurrentBuffer != nullptr) gst_buffer_unmap(mCurrentBuffer, &mCurrentMapInfo);
    if (mCurrentSample != nullptr) gst_sample_unref(mCurrentSample);

    reset();
}

void Player::setState(GstState state)
{
    g_return_if_fail(mPipeline != nullptr);

    onBeforeSetState(state);

    GstState old_state = getState();
    if (gst_element_set_state(mPipeline, state) == GST_STATE_CHANGE_SUCCESS)
        onStateChanged(old_state);
}

GstState Player::getState() const
{
    return mState;
}

GstState Player::queryState() const
{
    GstState state = mState;
    
    if (gst_element_get_state(mPipeline, &state, nullptr, GST_SECOND) == GST_STATE_CHANGE_FAILURE)
        NSVR_LOG("Failed to obtain state in specified timeout.");

    return state;
}

void Player::stop()
{
    setState(GST_STATE_NULL);
    setState(GST_STATE_READY);
}

void Player::play()
{
    setState(GST_STATE_PLAYING);
    // This can happen if current instance is used to open a second URI
    if (getMute() && getVolume() != 0.) setMute(true);
}

void Player::replay()
{
    stop();
    play();
}

void Player::pause()
{
    setState(GST_STATE_PAUSED);
}

void Player::update()
{
    onBeforeUpdate();

    if (mGstBus != nullptr)
    {
        while (gst_bus_have_pending(mGstBus) != FALSE)
        {
            if (GstMessage* msg = gst_bus_pop(mGstBus))
            {
                BIND_TO_SCOPE(msg);

                switch (GST_MESSAGE_TYPE(msg))
                {

                case GST_MESSAGE_ERROR:
                {
                    GError* err = nullptr;
                    gchar*  dbg = nullptr;
                    
                    BIND_TO_SCOPE(err);
                    BIND_TO_SCOPE(dbg);

                    gst_message_parse_error(msg, &err, &dbg);

                    NSVR_LOG("Pipeline encountered an error: [" << err->message << "] debug: [" << dbg << "].");
                }
                break;

                case GST_MESSAGE_STATE_CHANGED:
                {
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(mPipeline))
                    {
                        GstState old_state = GST_STATE_NULL;
                        gst_message_parse_state_changed(msg, &old_state, &mState, nullptr);

                        if (old_state != mState)
                        {
                            onStateChanged(old_state);
                        }
                    }
                }
                break;

                case GST_MESSAGE_ASYNC_DONE:
                {
                    if (mSeekingLock)
                    {
                        mSeekingLock = false;
                    }

                    if (mPendingSeek >= 0.)
                    {
                        setTime(mPendingSeek);
                    }

                    onSeekFinished();
                }
                break;

                case GST_MESSAGE_DURATION_CHANGED:
                {
                    queryDuration();
                }
                break;

                case GST_MESSAGE_EOS:
                {
                    onStreamEnd();

                    if (getLoop())
                    {
                        replay();
                    }
                    else
                    {
                        pause();
                    }
                }
                    break;

                case GST_MESSAGE_WARNING:
                {
                    GError* err = nullptr;
                    gchar*  dbg = nullptr;

                    BIND_TO_SCOPE(err);
                    BIND_TO_SCOPE(dbg);

                    gst_message_parse_warning(msg, &err, &dbg);

                    NSVR_LOG("Pipeline emitted warning: [" << err->message << "] debug: [" << dbg << "].");
                }
                    break;

                case GST_MESSAGE_INFO:
                {
                    GError* err = nullptr;
                    gchar*  dbg = nullptr;

                    BIND_TO_SCOPE(err);
                    BIND_TO_SCOPE(dbg);

                    gst_message_parse_info(msg, &err, &dbg);

                    NSVR_LOG("Pipeline emitted info: [" << err->message << "] debug: [" << dbg << "].");
                }
                break;

                default:
                    break;
                }
            }
        }
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
    g_return_if_fail(mLoop != on);
    mLoop = on;
}

bool Player::getLoop() const
{
    return mLoop;
}

void Player::setTime(gdouble time)
{
    g_return_if_fail(mPipeline != nullptr);

    if (mSeekingLock || mDuration == 0)
    {
        mPendingSeek = time;
        return;
    }
    else if (gst_element_seek_simple(
        mPipeline,
        GST_FORMAT_TIME,
        GstSeekFlags(
            GST_SEEK_FLAG_FLUSH |
            GST_SEEK_FLAG_ACCURATE),
        gint64(CLAMP(time, 0, mDuration) * GST_SECOND)) != FALSE)
    {
        mSeekingLock = true;
        mPendingSeek = -1.;
    }
}

gdouble Player::getTime() const
{
    g_return_val_if_fail(mPipeline != nullptr, 0.);

    gint64 time_ns;
    if (gst_element_query_position(mPipeline, GST_FORMAT_TIME, &time_ns) != FALSE)
    {
        mTime = time_ns / gdouble(GST_SECOND);
    }

    return mTime;
}

void Player::setVolume(gdouble vol)
{
    g_return_if_fail(mPipeline != nullptr || mVolume != vol || !getMute());

    mVolume = CLAMP(vol, 0., 1.);
    mMute = false;

    if (mPipeline)
    {
        g_object_set(mPipeline, "volume", mVolume, nullptr);
    }
}

gdouble Player::getVolume() const
{
    g_return_val_if_fail(mPipeline != nullptr, 0.);

    if (mPipeline)
    {
        g_object_get(mPipeline, "volume", &mVolume, nullptr);
    }

    return mVolume;
}

void Player::setMute(bool on)
{
    static gdouble saved_volume = 1.;
    g_return_if_fail(mPipeline != nullptr || (on && saved_volume == 0.));

    if (on)
    {
        saved_volume = getVolume();
        setVolume(0.);
        mMute = true;
    }
    else
    {
        mMute = false;
        setVolume(saved_volume);
        saved_volume = 1.;
    }
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

void Player::reset()
{
    mState          = GST_STATE_NULL;
    mPipeline       = nullptr;
    mGstBus         = nullptr;
    mCurrentBuffer  = nullptr;
    mCurrentSample  = nullptr;
    mWidth          = 0;
    mHeight         = 0;
    mDuration       = 0;
    mTime           = 0.;
    mVolume         = 1.;
    mRate           = 1.;
    mPendingSeek    = -1.;
    mSeekingLock    = false;
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

void Player::queryDuration()
{
    g_return_if_fail(mPipeline != nullptr);

    // Nanoseconds
    gint64 duration_ns = 0;
    if (gst_element_query_duration(mPipeline, GST_FORMAT_TIME, &duration_ns) != FALSE) {
        // Seconds
        mDuration = duration_ns / gdouble(GST_SECOND);
    }
}

}
