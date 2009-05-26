// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NATIVE_WINDOW_NOTIFICATION_SOURCE_H_
#define CHROME_COMMON_NATIVE_WINDOW_NOTIFICATION_SOURCE_H_

#include "base/gfx/native_widget_types.h"
#include "chrome/common/notification_source.h"

// Specialization of the Source class for HWND.  This is needed as the Source
// class expects a pointer type.
template<>
class Source<gfx::NativeWindow> : public NotificationSource {
 public:
  explicit Source(gfx::NativeWindow wnd) : NotificationSource(wnd) {}

  explicit Source(const NotificationSource& other)
      : NotificationSource(other) {}

  gfx::NativeWindow operator->() const { return ptr(); }
  gfx::NativeWindow ptr() const { return static_cast<gfx::NativeWindow>(ptr_); }
};

#endif  // CHROME_COMMON_NATIVE_WINDOW_NOTIFICATION_SOURCE_H_
