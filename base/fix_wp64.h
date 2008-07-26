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

// Various inline functions and macros to fix compilation of 32 bit target
// on MSVC with /Wp64 flag enabled.

#ifndef BASE_FIX_WP64_H__
#define BASE_FIX_WP64_H__

#include <windows.h>

// Platform SDK fixes when building with /Wp64 for a 32 bits target.
#if !defined(_WIN64) && defined(_Wp64)

#ifdef InterlockedExchangePointer
#undef InterlockedExchangePointer
// The problem is that the macro provided for InterlockedExchangePointer() is
// doing a (LONG) C-style cast that triggers invariably the warning C4312 when
// building on 32 bits.
inline void* InterlockedExchangePointer(void* volatile* target, void* value) {
  return reinterpret_cast<void*>(static_cast<LONG_PTR>(InterlockedExchange(
      reinterpret_cast<volatile LONG*>(target),
      static_cast<LONG>(reinterpret_cast<LONG_PTR>(value)))));
}
#endif  // #ifdef InterlockedExchangePointer

#ifdef SetWindowLongPtrA
#undef SetWindowLongPtrA
// When build on 32 bits, SetWindowLongPtrX() is a macro that redirects to
// SetWindowLongX(). The problem is that this function takes a LONG argument
// instead of a LONG_PTR.
inline LONG_PTR SetWindowLongPtrA(HWND window, int index, LONG_PTR new_long) {
  return ::SetWindowLongA(window, index, static_cast<LONG>(new_long));
}
#endif  // #ifdef SetWindowLongPtrA

#ifdef SetWindowLongPtrW
#undef SetWindowLongPtrW
inline LONG_PTR SetWindowLongPtrW(HWND window, int index, LONG_PTR new_long) {
  return ::SetWindowLongW(window, index, static_cast<LONG>(new_long));
}
#endif  // #ifdef SetWindowLongPtrW

#ifdef GetWindowLongPtrA
#undef GetWindowLongPtrA
inline LONG_PTR GetWindowLongPtrA(HWND window, int index) {
  return ::GetWindowLongA(window, index);
}
#endif  // #ifdef GetWindowLongPtrA

#ifdef GetWindowLongPtrW
#undef GetWindowLongPtrW
inline LONG_PTR GetWindowLongPtrW(HWND window, int index) {
  return ::GetWindowLongW(window, index);
}
#endif  // #ifdef GetWindowLongPtrW

#ifdef SetClassLongPtrA
#undef SetClassLongPtrA
inline LONG_PTR SetClassLongPtrA(HWND window, int index, LONG_PTR new_long) {
  return ::SetClassLongA(window, index, static_cast<LONG>(new_long));
}
#endif  // #ifdef SetClassLongPtrA

#ifdef SetClassLongPtrW
#undef SetClassLongPtrW
inline LONG_PTR SetClassLongPtrW(HWND window, int index, LONG_PTR new_long) {
  return ::SetClassLongW(window, index, static_cast<LONG>(new_long));
}
#endif  // #ifdef SetClassLongPtrW

#endif  // #if !defined(_WIN64) && defined(_Wp64)

#endif  // BASE_FIX_WP64_H__
