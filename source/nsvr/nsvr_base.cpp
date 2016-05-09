#include "nsvr_internal.hpp"
#include "nsvr/nsvr_base.hpp"
#include "nsvr/nsvr_discoverer.hpp"

namespace nsvr
{

Player::Player()
    : mLoop(false)
    , mMute(false)
{
    Internal::reset(*this);
    if (!Internal::gstreamerInitialized())
    {
        onError("GStreamer could not be initialized.");
    }
}

Player::~Player()
{
    close();
}

const gchar* getVersion()
{
#   define STRINGIFO(v) #v
#   define STRINGIFY(v) STRINGIFO v

    return "1.0.0" "_"
        STRINGIFY(GST_VERSION_MAJOR) "."
        STRINGIFY(GST_VERSION_MINOR) "."
        STRINGIFY(GST_VERSION_MICRO) "."
        STRINGIFY(GST_VERSION_NANO);
}

void addPluginPath(const gchar* path)
{
    if (Internal::isNullOrEmpty(path))
    {
        g_debug("Plug-in path supplied is empty.");
        return;
    }

    if (!Internal::gstreamerInitialized())
    {
        g_debug("You are not able to add plug-in path. %s",
                "GStreamer could not be initialized.");
        return;
    }

    if (GstRegistry *registry = gst_registry_get())
    {
        gst_registry_scan_path(registry, path);
    }
}

void addBinaryPath(const gchar* path)
{
    if (Internal::isNullOrEmpty(path))
    {
        g_debug("Binary path supplied is empty.");
        return;
    }

    gchar* path_var = nullptr;
    BIND_TO_SCOPE(path_var);

    if (Internal::isNullOrEmpty(g_getenv("PATH")))
    {
        path_var = g_strdup(path);
    }
    else
    {
        path_var = g_strdup_printf("%s;%s", g_getenv("PATH"), path);
    }

    if (g_setenv("PATH", scoped_path_var.pointer, TRUE) == FALSE)
    {
        g_debug("Unable to append %s to PATH.", path);
    }
}

bool Player::open(const gchar *path, gint width, gint height, const gchar* fmt)
{
    bool success = false;
    if (!Internal::gstreamerInitialized())
    {
        onError("You cannot open a media with ngw.");
        return success;
    }

    // First close any current streams.
    close();

    if (Internal::isNullOrEmpty(path))
    {
        onError("Supplied media path is empty.");
        return success;
    }

    Discoverer discoverer;
    if (discoverer.open(path))
    {
        gchar* pipeline_cmd = nullptr;
        BIND_TO_SCOPE(pipeline_cmd);

        if (discoverer.getHasVideo())
        {
            // Create the pipeline expression
            pipeline_cmd = g_strdup_printf(
                "playbin uri=\"%s\" video-sink=\""
                "appsink drop=yes async=no qos=yes sync=yes max-lateness=%lld "
                "caps=video/x-raw,width=%d,height=%d,format=%s\"",
                discoverer.getUri(),
                static_cast<long long>(GST_SECOND),
                width,
                height,
                Internal::isNullOrEmpty(fmt) ? "BGRA" : fmt);
        }
        else if (discoverer.getHasAudio())
        {
            // Create the pipeline expression
            pipeline_cmd = g_strdup_printf(
                "playbin uri=\"%s\"",
                discoverer.getUri());
        }
        else
        {
            onError("Media provided does not contain neither audio nor video.");
            return success;
        }

        if (!Internal::isNullOrEmpty(scoped_pipeline_cmd.pointer))
        {
            mPipeline = gst_parse_launch(scoped_pipeline_cmd.pointer, nullptr);
            if (mPipeline == nullptr)
            {
                close();
                onError("Unable to launch the pipeline.");
                return success;
            }

            mGstBus = gst_pipeline_get_bus(GST_PIPELINE(mPipeline));
            if (mGstBus == nullptr)
            {
                close();
                onError("Unable to obtain pipeline's bus.");
                return success;
            }

            if (discoverer.getHasVideo())
            {
                GstAppSink *app_sink = nullptr;
                BIND_TO_SCOPE(app_sink);

                g_object_get(mPipeline, "video-sink", &app_sink, nullptr);
                if (app_sink == nullptr)
                {
                    close();
                    onError("Unable to obtain pipeline's video sink.");
                    return success;
                }

                // Configure VideoSink's appsink:
                typedef GstFlowReturn(*APP_SINK_CB) (GstAppSink*, gpointer);
                GstAppSinkCallbacks callbacks;

                callbacks.eos           = nullptr;
                callbacks.new_preroll   = APP_SINK_CB(&Internal::onPreroll);
                callbacks.new_sample    = APP_SINK_CB(&Internal::onSampled);

                gst_app_sink_set_callbacks(scoped_app_sink.pointer, &callbacks, this, nullptr);
            }

            // Going from NULL => READY => PAUSE forces the
            // pipeline to pre-roll so we can get video dim
            GstState state;

            gst_element_set_state(mPipeline, GST_STATE_READY);
            if (gst_element_get_state(mPipeline, &state, nullptr, 10 * GST_SECOND) == GST_STATE_CHANGE_FAILURE ||
                state != GST_STATE_READY)
            {
                onError("Failed to put pipeline in READY state.");
                return success;
            }

            gst_element_set_state(mPipeline, GST_STATE_PAUSED);
            if (gst_element_get_state(mPipeline, &state, nullptr, 10 * GST_SECOND) == GST_STATE_CHANGE_FAILURE ||
                state != GST_STATE_PAUSED)
            {
                onError("Failed to put pipeline in PAUSE state.");
                return success;
            }

            mDuration = discoverer.getDuration();
            success = true;
        }
        else
        {
            onError("Pipeline string is empty.");
        }
    }

    return success;
}

bool Player::open(const gchar *path, gint width, gint height)
{
    return open(path, width, height, "BGRA");
}

bool Player::open(const gchar *path, const gchar* fmt)
{
    Discoverer discoverer;
    return discoverer.open(path) && open(path, discoverer.getWidth(), discoverer.getHeight(), fmt);

}

bool Player::open(const gchar *path)
{
    Discoverer discoverer;
    return discoverer.open(path) && open(path, discoverer.getWidth(), discoverer.getHeight());
}

void Player::close()
{
    stop();

    if (mPipeline != nullptr)      gst_object_unref(mPipeline);
    if (mGstBus != nullptr)        gst_object_unref(mGstBus);
    if (mCurrentBuffer != nullptr) gst_buffer_unmap(mCurrentBuffer, &mCurrentMapInfo);
    if (mCurrentSample != nullptr) gst_sample_unref(mCurrentSample);

    Internal::reset(*this);
}

void Player::setState(GstState state)
{
    g_return_if_fail(mPipeline != nullptr);
    gst_element_set_state(mPipeline, state);
}

GstState Player::getState() const
{
    return mState;
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
    if (mGstBus != nullptr)
    {
        while (gst_bus_have_pending(mGstBus) != FALSE)
        {
            if (GstMessage* msg = gst_bus_pop(mGstBus))
            {
                BIND_TO_SCOPE(msg);

                switch (GST_MESSAGE_TYPE(scoped_msg.pointer))
                {
                case GST_MESSAGE_ERROR:
                {
                    GError *err = nullptr;
                    BIND_TO_SCOPE(err);
                    gst_message_parse_error(msg, &err, nullptr);
                    onError(scoped_err.pointer->message);
                    close();
                }
                break;

                case GST_MESSAGE_STATE_CHANGED:
                {
                    if (GST_MESSAGE_SRC(msg) != GST_OBJECT(mPipeline))
                        break;

                    GstState old_state = GST_STATE_NULL;
                    gst_message_parse_state_changed(msg, &old_state, &mState, nullptr);

                    if (old_state != mState)
                    {
                        onState(old_state);
                    }
                }
                break;

                case GST_MESSAGE_ASYNC_DONE:
                {
                    Internal::processDuration(*this);

                    if (mSeekingLock)
                    {
                        mSeekingLock = false;
                    }

                    if (mPendingSeek >= 0.)
                    {
                        setTime(mPendingSeek);
                    }
                }
                break;

                case GST_MESSAGE_DURATION_CHANGED:
                {
                    Internal::processDuration(*this);
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

                default:
                    break;
                }
            }
        }
    }

    if (g_atomic_int_get(&mBufferDirty) != FALSE)
    {
        onFrame(
            mCurrentMapInfo.data,
            mCurrentMapInfo.size);

        // free current resources on previous frame
        if (mCurrentBuffer) gst_buffer_unmap(mCurrentBuffer, &mCurrentMapInfo);
        if (mCurrentSample) gst_sample_unref(mCurrentSample);

        mCurrentBuffer = nullptr;
        mCurrentSample = nullptr;

        // Signal Streaming thread it can produce
        g_atomic_int_set(&mBufferDirty, FALSE);
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

    if (mSeekingLock)
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
        gint64(CLAMP(time, 0, mDuration) * GST_SECOND)))
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

void Player::setRate(gdouble rate)
{
    gint64 position      = 0;
    GstEvent *seek_event = nullptr;

    if (!gst_element_query_position(mPipeline, GST_FORMAT_TIME, &position)) {
        onError("Unable to retrieve current position.\n");
        return;
    }

    if (rate > 0) {
        seek_event = gst_event_new_seek(rate, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
            GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_NONE, 0);
    }
    else {
        seek_event = gst_event_new_seek(rate, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
            GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, position);
    }

    if (gst_element_send_event(mPipeline, seek_event) != FALSE) {
        mRate = rate;
    }
    else {
        onError("Pipeline did not handle the set rate event. Probably media does not support it.");
        gst_object_unref(seek_event);
    }
}

gdouble Player::getRate() const
{
    return mRate;
}

GstMapInfo Player::getMapInfo() const
{
    return mCurrentMapInfo;
}

GstSample* Player::getSample() const
{
    return mCurrentSample;
}

GstBuffer* Player::getBuffer() const
{
    return mCurrentBuffer;
}

}
