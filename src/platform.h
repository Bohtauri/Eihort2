/* Copyright (c) 2012, Jason Lloyd-Price and Antti Hakkinen
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met: 

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <cstdlib>

// Put global platform-dependent stuff here

// Implementation-defined shift for signed integers
// Should be equivalent to floor( what / pow( 2, how_far ) );
template< typename T >
T shift_right( T what, unsigned how_far ) {
	return what >> how_far;
}

#define MAX_WORKERS 64

#ifdef _WINDOWS
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max
#undef OPAQUE
#undef TRANSPARENT

#define snprintf _snprintf
#endif

#if defined(__unix__) || defined(__unix) || defined(unix) || \
  defined(__APPLE__) && defined(__MACH__)
  // On unices, try to get _POSIX_VERSION 
# include <unistd.h>
#endif

#ifdef _POSIX_VERSION
  // They just had to put it the other way..
# include <limits.h>
# define MAX_PATH (PATH_MAX)
#endif

#if defined(__APPLE__) && defined(__MACH__)
// On Mac, use the resources folder within the application.
#define RESOURCE_FOLDER_FORMAT "%s/../Resources/"
// On Mac use three levels above the binary path, since
  // the binary is located in Eihort.app/Contents/MacOS.
#define ROOT_FOLDER_FORMAT "%s/../../../"
# define MINECRAFT_PATH_FORMAT "%s/Library/Application Support/minecraft/"
#else
#define RESOURCE_FOLDER_FORMAT "%s/"
#define ROOT_FOLDER_FORMAT "%s/"
# define MINECRAFT_PATH_FORMAT "%s/.minecraft/"
#endif

inline static
const char *
gethomedir()
#ifdef _WINDOWS
{ return getenv( "AppData" ); }
#else
{ return getenv("HOME"); }
#endif

#endif // PLATFORM_H
