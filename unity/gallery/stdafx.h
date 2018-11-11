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
#pragma warning(push)
#pragma warning(disable:4267)
#include "core/pch.h"
//#include "multimedia/pch.h"
//#include "network/pch.h"
#pragma warning(pop)

#include <d3d11.h>
#include "gallery/openvr.h"
#include "unity/detail/PlatformBase.h"
#include "unity/detail/IUnityInterface.h"
#include "unity/detail/IUnityGraphics.h"
#include "unity/detail/IUnityGraphicsD3D11.h"

#include "unity/gallery/pch.h"

#pragma comment(lib,"Ws2_32")
#pragma comment(lib,"Shlwapi")