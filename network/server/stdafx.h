// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: reference additional headers your program requires here


#include "core/pch.h"
#include "multimedia/pch.h"
#include "network/pch.h"

#include "multimedia/ffmpeg/ffmpeg.h"
#include "multimedia/ffmpeg/context.h"

#pragma comment(lib, "core")
#pragma comment(lib, "network")
#pragma comment(lib, "multimedia")