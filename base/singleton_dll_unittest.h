// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_SINGLETON_DLL_UNITTEST_H__
#define BASE_SINGLETON_DLL_UNITTEST_H__

#include "base/singleton.h"

#ifdef SINGLETON_UNITTEST_EXPORTS
#define SINGLETON_UNITTEST_API __declspec(dllexport)
#else
#define SINGLETON_UNITTEST_API __declspec(dllimport)
#endif

// Function pointer for singleton getters.
typedef int* (WINAPI* SingletonIntFunc)();

// Callback function to be called on library unload.
typedef void (WINAPI* CallBackFunc)();

// Leaky/nonleak singleton initialization.
typedef void (WINAPI* LeakySingletonFunc)(CallBackFunc);

// Retrieve the leaky singleton for later disposal.
typedef CallBackFunc* (WINAPI* GetLeakySingletonFunc)();

// When using new/delete, the heap is destroyed on library unload. So use
// VirtualAlloc/VirtualFree to bypass this behavior.
template<typename Type>
struct CustomAllocTrait : public DefaultSingletonTraits<Type> {
  static Type* New() {
    return static_cast<Type*>(VirtualAlloc(NULL, sizeof(Type), MEM_COMMIT,
                                           PAGE_READWRITE));
  }

  static bool Delete(Type* p) {
    return 0!=VirtualFree(p, 0, MEM_RELEASE);
  }
};

// 1 and 2 share the same instance.
// 3 simply use a different key.
// 4 sets kMustCallNewExactlyOnce to true.
// 5 default initialize to 5.
extern "C" SINGLETON_UNITTEST_API int* WINAPI SingletonInt1();
extern "C" SINGLETON_UNITTEST_API int* WINAPI SingletonInt2();
extern "C" SINGLETON_UNITTEST_API int* WINAPI SingletonInt3();
extern "C" SINGLETON_UNITTEST_API int* WINAPI SingletonInt4();
extern "C" SINGLETON_UNITTEST_API int* WINAPI SingletonInt5();

extern "C" SINGLETON_UNITTEST_API void WINAPI SingletonNoLeak(
    CallBackFunc CallOnQuit);
extern "C" SINGLETON_UNITTEST_API void WINAPI SingletonLeak(
    CallBackFunc CallOnQuit);
extern "C" SINGLETON_UNITTEST_API CallBackFunc* WINAPI GetLeakySingleton();

#endif  // BASE_SINGLETON_DLL_UNITTEST_H__
