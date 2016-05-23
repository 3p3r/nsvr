#pragma once

#include <gst/gst.h>
#include <gst/player/player.h>

#include <atomic>
#include <string>

namespace nsvr
{

/*!
 * @class   Player
 * @brief   Media player class. Designed to play audio through system's
 *          default audio output (speakers) and hand of video frames to
 *          the user of the library.
 * @note    API of this class is not MT safe. Designed to be exclusively
 *          used in one thread and embedded in other game engines.
 * @details To obtain video frames, you need to subclass and override
 *          onVideoFrame(...) method. Same goes for receiving events. To get
 *          event callbacks, on[name of function] should be overridden.
 */
class Player
{
public:
    Player();
    virtual         ~Player();
    //! opens a media file, can resize and reformat the video (if any). Returns true on success
    bool            open(const std::string& path, gint width, gint height, const std::string& fmt);

    //! opens a media file, can resize the video (if any). Returns true on success
    bool            open(const std::string& path, gint width, gint height);

    //! opens a media file, can reformat the video (if any). Returns true on success
    bool            open(const std::string& path, const std::string& fmt);

    //! opens a media file and auto detects its meta data and outputs 32bit BGRA. Returns true on success
    bool            open(const std::string& path);

    //! closes the current media file and its associated resources (no op if no media)
    void            close();

    //! stops playback (setting time to 0)
    void            stop();

    //! resumes playback from its current time.
    void            play();

    //! replays the media from the beginning
    void            replay();

    //! pauses playback (leaving time at its current position)
    void            pause();

    //! update loop logic, MUST be called often in your engine's update loop
    void            update();

    //! answers duration of the media file. Valid after call to open()
    gdouble         getDuration() const;

    //! sets state of the player
    void            setState(GstPlayerState state);

    //! answers the current cached state of the player
    GstPlayerState  getState() const;

    //! sets if the player should loop playback in the end (true) or not (false)
    void            setLoop(bool on);

    //! answers true if the player is currently looping playback
    bool            getLoop() const;

    //! seeks the media to a given time. this is an @b async call
    virtual void    setTime(gdouble time);

    //! answers the current position of the player between [ 0. , getDuration() ]
    gdouble         getTime() const;

    //! sets the current volume of the player between [ 0. , 1. ]
    void            setVolume(gdouble vol);

    //! gets the current volume of the player between [ 0. , 1. ]
    gdouble         getVolume() const;

    //! sets if the player should mute playback (true) or not (false)
    void            setMute(bool on);

    //! answers true if the player is muted
    bool            getMute() const;

    //! answers width of the video, 0 if audio is being played. Valid after open()
    gint            getWidth() const;

    //! answers height of the video, 0 if audio is being played. Valid after open()
    gint            getHeight() const;

protected:
    //! Video frame callback, video buffer data and its size are passed in
    virtual void    onVideoFrame(guchar* buf, gsize size) const {}

    //! State change event, propagated by the pipeline. Old state passed in, obtain new state with getState()
    virtual void    onStateChanged(GstState old) {}

    //! Called on end of the stream. Playback is finished at this point
    virtual void    onStreamEnd() {}

    //! Called by whenever pipeline clock needs to be (re)constructed
    virtual void    setupClock() {}

    //! Called when seeking operation is finished
    virtual void    onSeekFinished() {}

    //! Called before open() is called
    virtual void    onBeforeOpen() {}

    //! Called before close() is called
    virtual void    onBeforeClose() {}

    //! Called before update() is called
    virtual void    onBeforeUpdate() {}

    //! Called before setState() is called. target state is passed in.
    virtual void    onBeforeSetState(GstState state) {}

private:
    bool isReady() const;
    bool isGstPlayerValid() const;
    bool isVideoSinkValid() const;
    bool isContextValid() const;
    void freeContext();
    bool makeContext();
    void freeGstPlayer();
    bool makeGstPlayer();
    void freeVideoSink();
    bool makeVideoSink(int width, int height, const std::string& fmt);

private:
    static void on_buffering(GstPlayer* player, gint percent, Player* self);
    static void on_duration_changed(GstPlayer* player, GstClockTime duration, Player* self);
    static void on_end_of_stream(GstPlayer* player, Player* self);
    static void on_error(GstPlayer* player, GError* error, Player* self);
    static void on_mute_changed(GstPlayer* player, Player* self);
    static void on_position_updated(GstPlayer* player, GstClockTime position, Player* self);
    static void on_seek_done(GstPlayer* player, guint64 position, Player* self);
    static void on_state_changed(GstPlayer* player, GstPlayerState state, Player* self);
    static void on_video_dimensions_changed(GstPlayer* player, gint width, gint height, Player* self);
    static void on_volume_changed(GstPlayer* player, Player* self);
    static void on_warning(GstPlayer* player, GError* warning, Player* self);
    static GstFlowReturn on_videosink_preroll(GstElement* appsink, Player* player);
    static GstFlowReturn on_videosink_sample(GstElement* appsink, Player* player);

private:
    void process_sample(GstSample* sample);
    void reset_state();

protected:
    virtual void onBuffering(gint percent)                      { /* no-op */ }
    virtual void onDurationChanged()                            { /* no-op */ }
    virtual void onEndOfStream()                                { /* no-op */ }
    virtual void onError(const std::string& /* message */)      { /* no-op */ }
    virtual void onMuteChanged()                                { /* no-op */ }
    virtual void onPositionChanged()                            { /* no-op */ }
    virtual void onSeekStart()                                  { /* no-op */ }
    virtual void onSeekDone()                                   { /* no-op */ }
    virtual void onStateChanged()                               { /* no-op */ }
    virtual void onVideoDimensionChanged()                      { /* no-op */ }
    virtual void onVolumeChanged()                              { /* no-op */ }
    virtual void onWarning(const std::string& /* message */)    { /* no-op */ }

protected:
    std::atomic<bool>   mBufferDirty;
    GstMapInfo          mCurrentMapInfo;
    GstSample*          mCurrentSample;
    GstBuffer*          mCurrentBuffer;
    GstPlayer*          mGstPlayer;
    GstElement*         mGstPipeline;
    GMainContext*       mPlayerContext;
    GstPlayerState      mState;
    gdouble             mDuration, mPosition, mVolume;
    gint                mWidth, mHeight;
    bool                mSeeking, mMute, mReady, mLoop;
};

}
