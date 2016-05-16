#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <iostream>

#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/net/net.h>
#include <gst/gstregistry.h>
#include <gst/app/gstappsink.h>
#include <gst/pbutils/gstdiscoverer.h>

#ifdef _WIN32
#   include <windows.h>
#endif

namespace nsvr
{

template<typename T>
struct BindToScope
{
    BindToScope(T*& ptr) : pointer(ptr) {}
    ~BindToScope();
    T*& pointer;
};

template< class T > struct no_ptr        { typedef T type; };
template< class T > struct no_ptr<T*>    { typedef T type; };
#define BIND_TO_SCOPE(var) BindToScope<\
    no_ptr<decltype(var)>::type> scoped_##var(var);

class Player;
class Discoverer;

class Internal
{
public:
    static bool             gstreamerInitialized();
    static std::string      processPath(const std::string& path);
    static void             reset(Player& player);
    static GstFlowReturn    onPreroll(GstElement* appsink, Player* player);
    static GstFlowReturn    onSampled(GstElement* appsink, Player* player);
    static void             processSample(Player *const player, GstSample* const sample);
    static void             processDuration(Player& player);
    static gboolean         onSocketDataAvailable(GSocket *socket, GIOCondition condition, gpointer user_data);
    static std::string              implode(const std::vector<std::string>& elements, const std::string& glue);
    static std::vector<std::string> explode(const std::string &input, char separator);
};

/*!
 * @class Logger
 * @brief A very basic thread-safe logger. It also outputs to
 * DebugView if library is being compiled on Windows.
 * @note  Users should prefer using convenience NSVR_LOG macro.
 */
class Logger
{
public:
    static void log(const std::string& msg)
    {
        static std::mutex guard;
        std::lock_guard<decltype(guard)> lock(guard);
#ifdef _WIN32
        ::OutputDebugStringA(msg.c_str());
#endif
        std::cout << msg;
    }
};

}

/*! A convenience macro for nsvr::Logger. Input can be either string or stream
constructed with << operator. NOTE: an end line will be automatically appended. */
#ifndef NSVR_LOG
#   define NSVR_LOG(buf) { std::stringstream ss; ss << "[NSVR]" << "[" << std::this_thread::get_id() << "] " << buf << std::endl; nsvr::Logger::log(ss.str()); }
#endif
