// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_EVENT_CONVERSION_H__
#define WEBKIT_GLUE_EVENT_CONVERSION_H__

#include "base/basictypes.h"
#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
MSVC_POP_WARNING();

namespace WebCore {
  class Widget;
}

class WebMouseEvent;
class WebMouseWheelEvent;
class WebKeyboardEvent;

// These classes are used to convert from WebInputEvent subclasses to
// corresponding WebCore events.

class MakePlatformMouseEvent : public WebCore::PlatformMouseEvent {
 public:
  MakePlatformMouseEvent(WebCore::Widget* widget, const WebMouseEvent& e);

  static void ResetLastClick() {
    last_click_time_ = last_click_count_ = 0;
  }

 private:
  static int last_click_count_;
  static uint32 last_click_time_;
};

class MakePlatformWheelEvent : public WebCore::PlatformWheelEvent {
 public:
  MakePlatformWheelEvent(WebCore::Widget* widget, const WebMouseWheelEvent& e);
};

class MakePlatformKeyboardEvent : public WebCore::PlatformKeyboardEvent {
 public:
  MakePlatformKeyboardEvent(const WebKeyboardEvent& e);
  void SetKeyType(Type type);
  bool IsCharacterKey() const;
};

#endif  // WEBKIT_GLUE_EVENT_CONVERSION_H__
