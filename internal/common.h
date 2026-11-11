#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#ifdef DeleteFile
#undef DeleteFile
#endif

#ifdef CopyFile
#undef CopyFile
#endif

#ifdef MoveFile
#undef MoveFile
#endif
