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

// This file defines the type used to provide sources for NotificationService
// notifications.

#ifndef CHROME_COMMON_NOTIFICATION_SOURCE_H__
#define CHROME_COMMON_NOTIFICATION_SOURCE_H__

#include "base/basictypes.h"

// Do not declare a NotificationSource directly--use either
// "Source<sourceclassname>(sourceclasspointer)" or
// NotificationService::AllSources().
class NotificationSource {
 public:
  ~NotificationSource() {}

  // NotificationSource can be used as the index for a map; this method
  // returns the pointer to the current source as an identifier, for use as a
  // map index.
  uintptr_t map_key() const { return reinterpret_cast<uintptr_t>(ptr_); }

  bool operator!=(const NotificationSource& other) const {
    return ptr_ != other.ptr_;
  }
  bool operator==(const NotificationSource& other) const {
    return ptr_ == other.ptr_;
  }

 protected:
  NotificationSource(void* ptr) : ptr_(ptr) {}
  NotificationSource(const NotificationSource& other) : ptr_(other.ptr_) { }

  void* ptr_;

 private:
  void operator=(const NotificationSource&);
};

template <class T>
class Source : public NotificationSource {
 public:
  Source(T* ptr) : NotificationSource(ptr) {}

  Source(const NotificationSource& other)
    : NotificationSource(other) {}

  T* operator->() const { return ptr(); }
  T* ptr() const { return static_cast<T*>(ptr_); }
};

#endif  // CHROME_COMMON_NOTIFICATION_SOURCE_H__
