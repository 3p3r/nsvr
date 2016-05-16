#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <iostream>

namespace nsvr {
namespace internal {

/*!
 * @struct BindToScope
 * @brief  RAII utility that releases a typed C-style pointer.
 */
template<typename T>
struct BindToScope
{
    BindToScope(T*& ptr) : pointer(ptr) {}
    ~BindToScope();
    T*& pointer;
};

template< class T > struct no_ptr        { typedef T type; };
template< class T > struct no_ptr<T*>    { typedef T type; };

//! convenience macro to bound a pointer to a scope
#define BIND_TO_SCOPE(var) nsvr::internal::BindToScope<\
    nsvr::internal::no_ptr<decltype(var)>::type> scoped_##var(var);

//! Initializes GStreamer if it's not already. Returns true on success
bool gstreamerInitialized();

//! converts path to URI. no-op if a URI is passed
std::string pathToUri(const std::string& path);

//! Text-implode. Glues multiple strings together
std::string implode(const std::vector<std::string>& elements, const std::string& glue);

//! Text-explode. Separates a string with a separator
std::vector<std::string> explode(const std::string &input, char separator);

/*!
 * @fn    log
 * @brief A very basic thread-safe logger. It also outputs to
 * DebugView if library is being compiled on Windows.
 * @note  Users should prefer using convenience NSVR_LOG macro.
 */
void log(const std::string& msg);

}}

/*! A convenience macro for nsvr::Logger. Input can be either string or stream
constructed with << operator. NOTE: an end line will be automatically appended. */
#ifndef NSVR_LOG
#   define NSVR_LOG(buf) { std::stringstream ss; ss << "[NSVR]" << "[" << std::this_thread::get_id() << "] " << buf << std::endl; nsvr::internal::log(ss.str()); }
#endif
