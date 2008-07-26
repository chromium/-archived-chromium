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

#include "base/singleton_dll_unittest.h"
#include "base/logging.h"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason_for_call, LPVOID reserved) {
  switch (reason_for_call) {
    case DLL_PROCESS_ATTACH:
      DisableThreadLibraryCalls(module);
      break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
      break;
  }
  return TRUE;
}

COMPILE_ASSERT(DefaultSingletonTraits<int>::kRegisterAtExit == true, a);
COMPILE_ASSERT(
    DefaultSingletonTraits<int>::kMustCallNewExactlyOnce == false,
    b);

template<typename Type>
struct LockTrait : public DefaultSingletonTraits<Type> {
  static const bool kMustCallNewExactlyOnce = true;
};

struct Init5Trait : public DefaultSingletonTraits<int> {
  static int* New() {
    return new int(5);
  }
};

struct CallbackTrait : public CustomAllocTrait<CallBackFunc> {
  static void Delete(CallBackFunc* p) {
    if (*p)
      (*p)();
    CHECK(CustomAllocTrait<CallBackFunc>::Delete(p));
  }
};

struct NoLeakTrait : public CallbackTrait {
};

struct LeakTrait : public CallbackTrait {
  static const bool kRegisterAtExit = false;
};

SINGLETON_UNITTEST_API int* WINAPI SingletonInt1() {
  return Singleton<int>::get();
}

SINGLETON_UNITTEST_API int* WINAPI SingletonInt2() {
  // Force to use a different singleton than SingletonInt1.
  return Singleton<int, DefaultSingletonTraits<int> >::get();
}

class DummyDifferentiatingClass {
};

SINGLETON_UNITTEST_API int* WINAPI SingletonInt3() {
  // Force to use a different singleton than SingletonInt1 and SingletonInt2.
  // Note that any type can be used; int, float, std::wstring...
  return Singleton<int, DefaultSingletonTraits<int>,
                   DummyDifferentiatingClass>::get();
}

SINGLETON_UNITTEST_API int* WINAPI SingletonInt4() {
  return Singleton<int, LockTrait<int> >::get();
}

SINGLETON_UNITTEST_API int* WINAPI SingletonInt5() {
  return Singleton<int, Init5Trait>::get();
}

SINGLETON_UNITTEST_API void WINAPI SingletonNoLeak(CallBackFunc CallOnQuit) {
  *Singleton<CallBackFunc, NoLeakTrait>::get() = CallOnQuit;
}

SINGLETON_UNITTEST_API void WINAPI SingletonLeak(CallBackFunc CallOnQuit) {
  *Singleton<CallBackFunc, LeakTrait>::get() = CallOnQuit;
}

SINGLETON_UNITTEST_API CallBackFunc* WINAPI GetLeakySingleton() {
  return Singleton<CallBackFunc, LeakTrait>::get();
}
