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

#ifndef BASE_SCOPED_HANDLE_H__
#define BASE_SCOPED_HANDLE_H__

#include <windows.h>

#include "base/basictypes.h"

// Used so we always remember to close the handle. Example:
//   ScopedHandle hfile(CreateFile(...));
//   if (!hfile.Get())
//     ...process error
//   ReadFile(hfile.Get(), ...);
//
// To sqirrel the handle away somewhere else:
//   secret_handle_ = hfile.Take();
//
// To explicitly close the handle:
//   CloseHandle(hfile.Take());
class ScopedHandle {
 public:
  ScopedHandle() : handle_(NULL) {
  }

  explicit ScopedHandle(HANDLE h) : handle_(NULL) {
    Set(h);
  }

  ~ScopedHandle() {
    Close();
  }

  // Use this instead of comparing to INVALID_HANDLE_VALUE to pick up our NULL
  // usage for errors.
  bool IsValid() const {
    return handle_ != NULL;
  }

  void Set(HANDLE new_handle) {
    Close();

    // Windows is inconsistent about invalid handles, so we always use NULL
    if (handle_ != INVALID_HANDLE_VALUE)
      handle_ = new_handle;
  }

  HANDLE Get() {
    return handle_;
  }

  operator HANDLE() { return handle_; }

  HANDLE Take() {
    // transfers ownership away from this object
    HANDLE h = handle_;
    handle_ = NULL;
    return h;
  }

 private:
  void Close() {
    if (handle_) {
      CloseHandle(handle_);
      handle_ = NULL;
    }
  }

  HANDLE handle_;
  DISALLOW_EVIL_CONSTRUCTORS(ScopedHandle);
};

// Like ScopedHandle, but for HANDLEs returned from FindFile().
class ScopedFindFileHandle {
 public:
  explicit ScopedFindFileHandle(HANDLE handle) : handle_(handle) {
    // Windows is inconsistent about invalid handles, so we always use NULL
    if (handle_ == INVALID_HANDLE_VALUE)
      handle_ = NULL;
  }

  ~ScopedFindFileHandle() {
    if (handle_)
      FindClose(handle_);
  }

  // Use this instead of comparing to INVALID_HANDLE_VALUE to pick up our NULL
  // usage for errors.
  bool IsValid() const { return handle_ != NULL; }

  operator HANDLE() { return handle_; }

 private:
  HANDLE handle_;

  DISALLOW_EVIL_CONSTRUCTORS(ScopedFindFileHandle);
};

// Like ScopedHandle but for HDC.  Only use this on HDCs returned from
// CreateCompatibleDC.  For an HDC returned by GetDC, use ReleaseDC instead.
class ScopedHDC {
 public:
  explicit ScopedHDC(HDC h) : hdc_(h) { }

  ~ScopedHDC() {
    if (hdc_)
      DeleteDC(hdc_);
  }

  operator HDC() { return hdc_; }

 private:
  HDC hdc_;
  DISALLOW_EVIL_CONSTRUCTORS(ScopedHDC);
};

// Like ScopedHandle but for HBITMAP.
class ScopedBitmap {
 public:
  explicit ScopedBitmap(HBITMAP h) : hbitmap_(h) { }

  ~ScopedBitmap() {
    if (hbitmap_)
      DeleteObject(hbitmap_);
  }

  operator HBITMAP() { return hbitmap_; }

 private:
  HBITMAP hbitmap_;
  DISALLOW_EVIL_CONSTRUCTORS(ScopedBitmap);
};

// Like ScopedHandle but for HRGN.
class ScopedHRGN {
 public:
  explicit ScopedHRGN(HRGN h) : hrgn_(h) { }

  ~ScopedHRGN() {
    if (hrgn_)
      DeleteObject(hrgn_);
  }

  operator HRGN() { return hrgn_; }

  ScopedHRGN& operator=(HRGN hrgn) {
    if (hrgn_ && hrgn != hrgn_)
      DeleteObject(hrgn_);
    hrgn_ = hrgn;
    return *this;
  }

 private:
  HRGN hrgn_;
  DISALLOW_EVIL_CONSTRUCTORS(ScopedHRGN);
};

// Like ScopedHandle except for HGLOBAL.
template<class T>
class ScopedHGlobal {
 public:
  explicit ScopedHGlobal(HGLOBAL glob) : glob_(glob) {
    data_ = static_cast<T*>(GlobalLock(glob_));
  }
  ~ScopedHGlobal() {
    GlobalUnlock(glob_);
  }

  T* get() { return data_; }

  size_t Size() const { return GlobalSize(glob_); }

  T* operator->() const  {
    assert(data_ != 0);
    return data_;
  }

 private:
  HGLOBAL glob_;

  T* data_;

  DISALLOW_EVIL_CONSTRUCTORS(ScopedHGlobal);
};

#endif // BASE_SCOPED_HANDLE_H__
