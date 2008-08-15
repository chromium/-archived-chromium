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

// This file defines the type used to provide details for NotificationService
// notifications.

#ifndef CHROME_COMMON_NOTIFICATION_DETAILS_H__
#define CHROME_COMMON_NOTIFICATION_DETAILS_H__

#include "base/basictypes.h"

// Do not declare a NotificationDetails directly--use either
// "Details<detailsclassname>(detailsclasspointer)" or
// NotificationService::NoDetails().
class NotificationDetails {
 public:
  NotificationDetails() : ptr_(NULL) {}
  NotificationDetails(const NotificationDetails& other) : ptr_(other.ptr_) {}
  ~NotificationDetails() {}

  // NotificationDetails can be used as the index for a map; this method
  // returns the pointer to the current details as an identifier, for use as a
  // map index.
  uintptr_t map_key() const { return reinterpret_cast<uintptr_t>(ptr_); }

  bool operator!=(const NotificationDetails& other) const {
    return ptr_ != other.ptr_;
  }

  bool operator==(const NotificationDetails& other) const {
    return ptr_ == other.ptr_;
  }

 protected:
  NotificationDetails(void* ptr) : ptr_(ptr) {}

  void* ptr_;
};

template <class T>
class Details : public NotificationDetails {
 public:
  Details(T* ptr) : NotificationDetails(ptr) {}
  Details(const NotificationDetails& other)
    : NotificationDetails(other) {}

  T* operator->() const { return ptr(); }
  T* ptr() const { return static_cast<T*>(ptr_); }
};

#endif  // CHROME_COMMON_NOTIFICATION_DETAILS_H__
