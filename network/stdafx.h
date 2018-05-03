// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include "targetver.h"

// TODO: reference additional headers your program requires here
#define CORE_USE_FMTLIB
#include "core/pch.h"
#include "network/pch.h"

#pragma comment(lib, "core")