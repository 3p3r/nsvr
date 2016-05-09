#include "nsvr/nsvr_discoverer.hpp"
#include "nsvr_internal.hpp"

namespace nsvr
{

Discoverer::Discoverer()
{
    Internal::reset(*this);
}

Discoverer::~Discoverer()
{
    Internal::reset(*this);
}

bool Discoverer::open(const std::string& path)
{
    bool success = false;

    if (!Internal::gstreamerInitialized())
    {
        g_debug("You cannot open a media with nsvr. %s",
                "GStreamer could not be initialized.");
        return success;
    }

    try
    {
        Internal::reset(*this);
        if (path.empty()) return success;

        mMediaUri = Internal::processPath(path);
        if (mMediaUri.empty()) return success;

        auto discover_timeout = 10 * GST_SECOND;
        if (GstDiscoverer *discoverer = gst_discoverer_new(discover_timeout, nullptr))
        {
            BIND_TO_SCOPE(discoverer);
            if (GstDiscovererInfo *info = gst_discoverer_discover_uri(discoverer, mMediaUri.c_str(), nullptr))
            {
                BIND_TO_SCOPE(info);
                if (gst_discoverer_info_get_result(scoped_info.pointer) == GST_DISCOVERER_OK)
                {
                    if (GList *video_streams = gst_discoverer_info_get_video_streams(info))
                    {
                        mHasVideo = true;
                        BIND_TO_SCOPE(video_streams);

                        for (GList *curr = scoped_video_streams.pointer; curr; curr = curr->next)
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

                    if (GList *audio_streams = gst_discoverer_info_get_audio_streams(info))
                    {
                        mHasAudio = true;
                        BIND_TO_SCOPE(audio_streams);

                        for (GList *curr = scoped_audio_streams.pointer; curr; curr = curr->next)
                        {
                            GstDiscovererStreamInfo *curr_sinfo = (GstDiscovererStreamInfo *)curr->data;

                            if (GST_IS_DISCOVERER_AUDIO_INFO(curr_sinfo))
                            {
                                mSampleRate = gst_discoverer_audio_info_get_sample_rate(GST_DISCOVERER_AUDIO_INFO(curr_sinfo));
                                mBitRate    = gst_discoverer_audio_info_get_bitrate(GST_DISCOVERER_AUDIO_INFO(curr_sinfo));
                            }
                        }
                    }

                    mSeekable = gst_discoverer_info_get_seekable(info) != FALSE;
                    mDuration = gst_discoverer_info_get_duration(info) / gdouble(GST_SECOND);
                    success = true;
                }
            }
        }
    }
    catch (...)
    {
        g_debug("Discoverer was unable to proceed due to exception. Are you missing binaries?");
        success = false;
    }

    return success;
}

const std::string& Discoverer::getUri() const
{
    return mMediaUri;
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

}
