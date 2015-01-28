/* Copyright (c) 2012, Antti Hakkinen
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

#ifndef ENDIAN_H_
#define ENDIAN_H_

#include "SDL.h"

namespace {

  // Swap BE <-> LE
    // You don't want to use these.

template<class T>
inline static
T
bswap(T); // Don't remove this, prevents dangerous implicit casts

inline static
uint16_t
bswap(uint16_t x)
{ return SDL_Swap16(x); }

inline static
int16_t
bswap(int16_t x)
{ return int16_t(bswap(uint16_t(x))); }

inline static
uint32_t
bswap(uint32_t x)
{ return SDL_Swap32(x); }

inline static
int32_t
bswap(int32_t x)
{ return int32_t(bswap(uint32_t(x))); }

inline static
uint64_t
bswap(uint64_t x)
{ return SDL_Swap64(x); }

inline static
int64_t
bswap(int64_t x)
{ return int64_t(bswap(uint64_t(x))); }

  // Swap from/to Big Endian

template<class T>
inline static
T 
bswap_from_big(T x)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  return x;
#elif SDL_BYTEORDER == SDL_LIL_ENDIAN
  return bswap(x);
#else
# error: SDL endian fail.
#endif
}

template<class T>
inline static
T
bswap_to_big(T x)
{ return bswap_from_big(x); }

} // <anonymous>

#endif // ENDIAN_H_
