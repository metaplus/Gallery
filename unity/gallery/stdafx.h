// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files:
#include <windows.h>



// TODO: reference additional headers your program requires here
#define CORE_USE_BOOST_ASIO
#include "core/pch.h"
#include "multimedia/pch.h"
#include "network/pch.h"

#include <folly/executors/task_queue/LifoSemMPMCQueue.h>
#include <folly/executors/thread_factory/NamedThreadFactory.h>
#include <folly/synchronization/Baton.h>
#include "unity/gallery/pch.h"

#pragma comment(lib, "core")
#pragma comment(lib, "multimedia")
#pragma comment(lib, "network")