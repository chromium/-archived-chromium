// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WIDGET_HWND_NOTIFICATION_SOURCE_H__
#define CHROME_VIEWS_WIDGET_HWND_NOTIFICATION_SOURCE_H__

#include "chrome/common/notification_source.h"

// Specialization of the Source class for HWND.  This is needed as the Source
// class expects a pointer type.
template<>
class Source<HWND> : public NotificationSource {
 public:
  explicit Source(HWND hwnd) : NotificationSource(hwnd) {}

  explicit Source(const NotificationSource& other)
      : NotificationSource(other) {}

  HWND operator->() const { return ptr(); }
  HWND ptr() const { return static_cast<HWND>(ptr_); }
};

#endif  // #define CHROME_VIEWS_WIDGET_HWND_NOTIFICATION_SOURCE_H__
