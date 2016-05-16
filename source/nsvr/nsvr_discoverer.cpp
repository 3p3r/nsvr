#include "nsvr_internal.hpp"
#include "nsvr/nsvr_discoverer.hpp"

namespace nsvr
{

bool Discoverer::open(const std::string& path)
{
    reset();

    if (path.empty())
    {
        NSVR_LOG("Path given to Discoverer is empty.");
        return false;
    }

    if (!Internal::gstreamerInitialized())
    {
        NSVR_LOG("Discoverer requires GStreamer to be initialized.");
        return false;
    }

    auto    success = false;
    auto    timeout = 10;
    GError* errors  = nullptr;

    try
    {
        mMediaUri = Internal::processPath(path);
        
        if (mMediaUri.empty())
        {
            NSVR_LOG("Unable to convert " << path << " into a valid URI.");
            return false;
        }

        NSVR_LOG("About to discover media: " << getMediaUri() << " with timeout " << timeout << " seconds.");
        BIND_TO_SCOPE(errors);

        if (GstDiscoverer *discoverer = gst_discoverer_new(timeout * GST_SECOND, &errors))
        {
            BIND_TO_SCOPE(discoverer);
            if (GstDiscovererInfo *info = gst_discoverer_discover_uri(discoverer, mMediaUri.c_str(), &errors))
            {
                BIND_TO_SCOPE(info);
                if (gst_discoverer_info_get_result(info) == GST_DISCOVERER_OK)
                {
                    if (GList *video_streams = gst_discoverer_info_get_video_streams(info))
                    {
                        mHasVideo = true;
                        BIND_TO_SCOPE(video_streams);

                        for (GList *curr = video_streams; curr; curr = curr->next)
                        {
                            GstDiscovererStreamInfo *curr_sinfo = (GstDiscovererStreamInfo *)curr->data;

                            if (GST_IS_DISCOVERER_VIDEO_INFO(curr_sinfo))
                            {
                                mWidth      = gst_discoverer_video_info_get_width(GST_DISCOVERER_VIDEO_INFO(curr_sinfo));
                                mHeight     = gst_discoverer_video_info_get_height(GST_DISCOVERER_VIDEO_INFO(curr_sinfo));
                                mFrameRate  = gst_discoverer_video_info_get_framerate_num(GST_DISCOVERER_VIDEO_INFO(curr_sinfo))
                                    / float(gst_discoverer_video_info_get_framerate_denom(GST_DISCOVERER_VIDEO_INFO(curr_sinfo)));
                            }
                        }
                    }
                    else
                        NSVR_LOG("No video streams found in " << getMediaUri() << ".");

                    if (GList *audio_streams = gst_discoverer_info_get_audio_streams(info))
                    {
                        mHasAudio = true;
                        BIND_TO_SCOPE(audio_streams);

                        for (GList *curr = audio_streams; curr; curr = curr->next)
                        {
                            GstDiscovererStreamInfo *curr_sinfo = (GstDiscovererStreamInfo *)curr->data;

                            if (GST_IS_DISCOVERER_AUDIO_INFO(curr_sinfo))
                            {
                                mSampleRate = gst_discoverer_audio_info_get_sample_rate(GST_DISCOVERER_AUDIO_INFO(curr_sinfo));
                                mBitRate    = gst_discoverer_audio_info_get_bitrate(GST_DISCOVERER_AUDIO_INFO(curr_sinfo));
                            }
                        }
                    }
                    else
                        NSVR_LOG("No audio streams found in " << getMediaUri() << ".");

                    mSeekable = gst_discoverer_info_get_seekable(info) != FALSE;
                    mDuration = gst_discoverer_info_get_duration(info) / gdouble(GST_SECOND);
                    success = true;
                }
                else
                    NSVR_LOG("GstDiscovererResult is not GST_DISCOVERER_OK.");
            }
            else
                NSVR_LOG("Unable to constrcut GstDiscovererInfo [" << errors->message << "].");
        }
        else
            NSVR_LOG("Unable to constrcut GstDiscoverer [" << errors->message << "].");
    }
    catch (...)
    {
        NSVR_LOG("Discoverer was unable to proceed due to an unknwon exception.");
        success = false;
    }

    return success;
}

gint Discoverer::getWidth() const
{
    return mWidth;
}

gint Discoverer::getHeight() const
{
    return mHeight;
}

gfloat Discoverer::getFrameRate() const
{
    return mFrameRate;
}

bool Discoverer::getHasVideo() const
{
    return mHasVideo;
}

bool Discoverer::getHasAudio() const
{
    return mHasAudio;
}

bool Discoverer::getSeekable() const
{
    return mSeekable;
}

gdouble Discoverer::getDuration() const
{
    return mDuration;
}

guint Discoverer::getSampleRate() const
{
    return mSampleRate;
}

guint Discoverer::getBitRate() const
{
    return mBitRate;
}

const std::string& Discoverer::getMediaUri() const
{
    return mMediaUri;
}

void Discoverer::reset()
{
    if (!mMediaUri.empty())
        mMediaUri.clear();

    mWidth      = 0;
    mHeight     = 0;
    mFrameRate  = 0;
    mHasAudio   = false;
    mHasVideo   = false;
    mSeekable   = false;
    mDuration   = 0;
    mSampleRate = 0;
    mBitRate    = 0;
}

}
