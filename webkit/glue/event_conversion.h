// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_EVENT_CONVERSION_H_
#define WEBKIT_GLUE_EVENT_CONVERSION_H_

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

namespace WebKit {
class WebMouseEvent;
class WebMouseWheelEvent;
class WebKeyboardEvent;
}

// These classes are used to convert from WebInputEvent subclasses to
// corresponding WebCore events.

class MakePlatformMouseEvent : public WebCore::PlatformMouseEvent {
 public:
  MakePlatformMouseEvent(
      WebCore::Widget* widget, const WebKit::WebMouseEvent& e);
};

class MakePlatformWheelEvent : public WebCore::PlatformWheelEvent {
 public:
  MakePlatformWheelEvent(
      WebCore::Widget* widget, const WebKit::WebMouseWheelEvent& e);
};

class MakePlatformKeyboardEvent : public WebCore::PlatformKeyboardEvent {
 public:
  MakePlatformKeyboardEvent(const WebKit::WebKeyboardEvent& e);
  void SetKeyType(Type type);
  bool IsCharacterKey() const;
};

#endif  // WEBKIT_GLUE_EVENT_CONVERSION_H_
