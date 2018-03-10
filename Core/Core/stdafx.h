// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX    //abolish evil macros from <windows.h>
#include "targetver.h"

#include <stdio.h>
#include <tchar.h>


// TODO: reference additional headers your program requires here
#undef min          //abolish vicious macros from <windows.h>, otherwise causing naming collision against STL
#undef max          //another tolerable solution appears like #define max_RESUME max #undef max ... #define max max_RESUME
#include "Core/pch.h"
//#include "Core/abbr.hpp"