// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#define _SILENCE_PARALLEL_ALGORITHMS_EXPERIMENTAL_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS           
#include <WinSock2.h>
#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: reference additional headers your program requires here
#include <rang.hpp>
#include <fmt/container.h>    
#include <fmt/format.h>                  
#include <fmt/ostream.h>
#include <fmt/string.h>
#include <fmt/time.h>
#include "Core/pch.h"
#include "FFmpeg/pch.h"
#include "Gallery/pch.h"
#include "Monitor/pch.h"
#include "Core/base.h"
#include "Core/verify.hpp"
#include "Core/basic_ptr.hpp"
#include "FFmpeg/base.h"
#include "FFmpeg/context.h"
//#include "Gallery/pch.h"
#include "Gallery/openvr.h"
#include "Gallery/interprocess.h"
#include "Gallery/interface.h"
#include "Monitor/base.hpp"
#pragma comment(lib,"Core")
#pragma comment(lib,"FFmpeg")
#pragma comment(lib,"Gallery")
#ifdef _WIN32
#ifdef _DEBUG
//#pragma comment(lib,"tbb_debug")
#pragma comment(lib,"Debug/fmt")
#else
//#pragma comment(lib,"tbb")
#pragma comment(lib,"Release/fmt")
#endif  // NDEBUG
#endif  // _WIN32