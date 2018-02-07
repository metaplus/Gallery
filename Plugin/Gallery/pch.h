#pragma once
#include "Gallery/openvr.h"
//#include "Gallery/interface.h"
#include <boost/asio.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#pragma warning(push)
#pragma warning(disable:4267)
#include <cereal/cereal.hpp>
#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp> 
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/chrono.hpp>
#include <cereal/types/common.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>
#pragma warning(pop)
//#include <boost/process.hpp>
//namespace process = boost::process;
namespace interprocess = boost::interprocess;
namespace asio = boost::asio;
#if defined UNITYAPI || defined EXTERN
#error naming collide
#endif
#ifdef GALLERY_EXPORTS
#define DLLAPI __declspec(dllexport)
#define UNITYAPI  __declspec(dllexport) __stdcall
#else
#define DLLAPI __declspec(dllimport)
#define UNITYAPI  __declspec(dllimport) __stdcall
#endif
#ifdef __cplusplus
#define EXTERN extern "C"
#else 
#define EXTERN extern 
#endif