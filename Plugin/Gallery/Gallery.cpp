// Gallery.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "interface.h"

BOOL GlobalCreate()
{
#ifdef NDEBUG                       
    try
    {
#endif
        dll::media_create();
        dll::ipc_create();
#ifdef NDEBUG
    }
    catch (...) { return false; }
#endif
    return true;
}
void GlobalRelease()
{
#ifdef NDEBUG
    try
    {
#endif
        dll::media_release();       //consider reverse releasing order
        dll::ipc_release();         //perhaps construct RAII guard inside current TU
#ifdef NDEBUG
    }
    catch (...)
    {
        dll::media_clear();
    }
#endif
}

