#pragma once

#include <string>

namespace nsvr
{

class Discoverer
{
public:
    //! Attempts to open a media for discovery.
    bool open(const std::string& path);
    
    //! Returns path to discovered media or empty string ("") on failure
    const std::string& getUri() const;
    
    //! Returns width of the media if it contains video (0 otherwise)
    int getWidth() const;
    
    //! Returns height of the media if it contains video (0 otherwise)
    int getHeight() const;
    
    //! Returns frame rate of the media if it contains video (0 otherwise)
    float getFrameRate() const;
    
    //! Answers true if media contains video
    bool getHasVideo() const;
    
    //! Answers true if media contains audio
    bool getHasAudio() const;
    
    //! Answers true if media is seek-able (false for streams)
    bool getSeekable() const;
    
    //! Answers duration of the discovered media
    double getDuration() const;
    
    //! Returns sample rate of the associated audio stream (0 if missing)
    unsigned getSampleRate() const;

    //! Returns bit rate of the associated audio stream (0 if missing)
    unsigned getBitRate() const;

    //! Returns discovered media URI. Can be different from path passed to open()
    const std::string& getMediaUri() const;

    //! Resets the internal state
    void reset();

private:
    std::string mMediaUri;              //!< URI to the discovered media
    int         mWidth      = 0;        //!< Width of the discovered media
    int         mHeight     = 0;        //!< Height of the discovered media
    float       mFrameRate  = 0;        //!< Frame rate of the discovered media
    double      mDuration   = 0;        //!< Duration of the discovered media
    unsigned    mSampleRate = 0;        //!< Sample rate of the discovered media (audio)
    unsigned    mBitRate    = 0;        //!< Bit rate of the discovered media in bits/second (audio)
    bool        mHasVideo   = false;    //!< Indicates whether media has video or not
    bool        mHasAudio   = false;    //!< Indicates whether media has audio or not
    bool        mSeekable   = false;    //!< Indicates whether media is seek able or not
};

}
