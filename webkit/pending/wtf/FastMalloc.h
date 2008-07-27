// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_FastMalloc_h
#define WTF_FastMalloc_h

#include "Platform.h"
#include <stdlib.h>
#include <string.h>
#include <new>

#ifndef CRASH
#define CRASH() *(int *)(uintptr_t)0xbbadbeef = 0
#endif

namespace WTF {

#if !PLATFORM(WIN_OS)
    void *fastMalloc(size_t n);
    void *fastZeroedMalloc(size_t n);
    void *fastCalloc(size_t n_elements, size_t element_size);
    void fastFree(void* p);
    void *fastRealloc(void* p, size_t n);

#ifndef NDEBUG    
    void fastMallocForbid();
    void fastMallocAllow();
#endif
#else  // !PLATFORM(WIN_OS)
    // Avoid using TCMalloc / FastMalloc in the Windows port of WebKit.
    //
    // We need to avoid using FastMalloc as it requires pthreads to be
    // initialized when allocations are made. This is a problem with
    // static objects that make memory allocations before main() is run.
    //
    // Instead, we'll use the regular Windows allocator, and pass any calls to
    // FastMalloc through to it.
    inline void *fastMalloc(size_t n) {
      void* rv = malloc(n);
      if (!rv) CRASH();
      return rv;
    }

    inline void *fastZeroedMalloc(size_t n) {
      void *result = fastMalloc(n);
      if (!result)
        return 0;
      memset(result, 0, n);
      return result;
    }
    inline void *fastCalloc(size_t n_elements, size_t element_size) {
      void* rv = calloc(n_elements, element_size);
      if (!rv) CRASH();
      return rv;
    }
    inline void fastFree(void* p) { free(p); }
    inline void *fastRealloc(void* p, size_t n) {
      void* rv = realloc(p, n);
      if (!rv) CRASH();
      return rv;
    }

#ifndef NDEBUG    
    inline void fastMallocForbid() {}
    inline void fastMallocAllow() {}
#endif
#endif  // !PLATFORM(WIN_OS)

} // namespace WTF

using WTF::fastMalloc;
using WTF::fastZeroedMalloc;
using WTF::fastCalloc;
using WTF::fastRealloc;
using WTF::fastFree;

#ifndef NDEBUG    
using WTF::fastMallocForbid;
using WTF::fastMallocAllow;
#endif

#if COMPILER(GCC) && PLATFORM(DARWIN)
#define WTF_PRIVATE_INLINE __private_extern__ inline __attribute__((always_inline))
#elif COMPILER(GCC)
#define WTF_PRIVATE_INLINE inline __attribute__((always_inline))
#else
#define WTF_PRIVATE_INLINE inline
#endif

#ifndef _CRTDBG_MAP_ALLOC

#if !defined(USE_SYSTEM_MALLOC) || !(USE_SYSTEM_MALLOC)
WTF_PRIVATE_INLINE void* operator new(size_t s) { return fastMalloc(s); }
WTF_PRIVATE_INLINE void operator delete(void* p) { fastFree(p); }
WTF_PRIVATE_INLINE void* operator new[](size_t s) { return fastMalloc(s); }
WTF_PRIVATE_INLINE void operator delete[](void* p) { fastFree(p); }
#endif

#endif // _CRTDBG_MAP_ALLOC

#endif /* WTF_FastMalloc_h */
