#pragma once

#include <gst/gst.h>

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

    //! sets state of the player (GST_STATE_PAUSED, etc.)
    void            setState(GstState state);

    //! answers the current cached state of the player (GST_STATE_PAUSED, etc.)
    GstState        getState() const;

    //! answers the current actual state of the player (GST_STATE_PAUSED, etc.)
    GstState        queryState() const;

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
    //! Resets internal state of the Player (does not free any memories!)
    void            reset();

    //! Called by GStreamer on streaming thread when a rolled sample is ready
    static GstFlowReturn onPreroll(GstElement* appsink, Player* player);

    //! Called by GStreamer on streaming thread when a new sample is ready
    static GstFlowReturn onSample(GstElement* appsink, Player* player);

    //! Called inside onPreroll() or onSample() to consume the new video frame
    void processSample(GstSample* const sample);

    //! Called within update() to query media duration when it is possible
    void queryDuration();

protected:
    GstState        mState;                 //!< Current state of the player (playing, paused, etc.)
    GstMapInfo      mCurrentMapInfo;        //!< Mapped Buffer info, ONLY valid inside onVideoFrame(...)
    GstSample       *mCurrentSample;        //!< Mapped Sample, ONLY valid inside onVideoFrame(...)
    GstBuffer       *mCurrentBuffer;        //!< Mapped Buffer, ONLY valid inside onVideoFrame(...)
    GstElement      *mPipeline;             //!< GStreamer pipeline (play-bin) object
    GstBus          *mGstBus;               //!< Bus associated with mPipeline
    mutable gdouble mPendingSeek;           //!< Value of the seek operation pending to be executed
    mutable bool    mSeekingLock;           //!< Boolean flag, indicating a seek operation is pending to be executed

private:
    mutable gint    mWidth      = 0;        //!< Width of the video being played. Valid after a call to open(...)
    mutable gint    mHeight     = 0;        //!< Height of the video being played. Valid after a call to open(...)
    mutable gdouble mDuration   = 0.;       //!< Duration of the media being played
    mutable gdouble mTime       = 0.;       //!< Current time of the media being played (current position)
    mutable gdouble mVolume     = 1.;       //!< Volume of the media being played
    mutable gdouble mRate       = 1.;       //!< Rate of playback, negative number for reverse playback

    std::atomic<bool>   mBufferDirty;       //!< Atomic boolean, representing a new frame is ready by GStreamer
    bool                mLoop   = false;    //!< Flag, indicating whether the player is looping or not
    bool                mMute   = false;    //!< Flag, indicating whether the player is muted or not
};

}
