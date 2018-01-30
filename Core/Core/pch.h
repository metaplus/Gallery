﻿#pragma once
#ifndef NDEBUG
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif  //NDEBUG
#define BOOST_THREAD_VERSION 4
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#define BOOST_USE_WINDOWS_H   
#define _SCL_SECURE_NO_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS //tbb usage

#include <limits>
#include <numeric>
#include <type_traits>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <string_view>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <algorithm>
#include <future>
#include <shared_mutex>
#include <variant>
#include <optional>
#include <any>
#include <filesystem>
#include <exception>
//#include <boost/thread.hpp>
//#include <boost/thread/scoped_thread.hpp>
//#include <boost/thread/future.hpp>
//#include <boost/assert.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/cstdlib.hpp>
//#include <boost/convert.hpp>
//#include <boost/convert/spirit.hpp>
//#include <boost/convert/stream.hpp>
//#include <boost/convert/lexical_cast.hpp>
//#include <boost/convert/strtol.hpp>
#include <boost/lexical_cast.hpp>       //prefer from_chars/to_chars
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/binary_object.hpp>    //make_binary_object
#include <boost/serialization/utility.hpp>
#include <boost/serialization/assume_abstract.hpp>
//#include <boost/asio.hpp>
//#include <boost/asio/buffer.hpp>
//#include <boost/asio/yield.hpp>
//#include <boost/iostreams/device/mapped_file.hpp>
#include <tbb/tbb.h>
#include <tbb/tbb_allocator.h>
#include <tbb/scalable_allocator.h>
#include <tbb/cache_aligned_allocator.h>
#include <fmt/format.h>                  
#include <fmt/container.h>    
#include <fmt/ostream.h>
#include <fmt/time.h>
#include <fmt/string.h>
//#include <spdlog/spdlog.h>
//#include <spdlog/fmt/ostr.h>
#include <termcolor/termcolor.hpp>

using namespace std::literals;
using namespace fmt::literals;
#include "Core/base.hpp"
#include "Core/revokable.hpp"
#include "Core/guard.h"

#define STRING2(x) #x  
#define STRING(x) STRING2(x)  
#define LINE_STR "[line]" STRING(__LINE__)
#define FILE_STR LINE_STR " [file]" __FILE__
#define FUNCTION_STR "[function]" __func__
#define ERROR_STR "[function]" __func__ " " LINE_STR " [file]" __FILE__ 

#ifdef _WIN32
#ifndef NDEBUG
#pragma comment(lib,"tbb_debug")
#pragma comment(lib,"Debug/fmt")
#else
#pragma comment(lib,"tbb")
#pragma comment(lib,"Release/fmt")
#endif  //NDEBUG
#endif  //_WIN32

