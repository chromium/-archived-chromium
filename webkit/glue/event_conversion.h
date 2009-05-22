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
class FrameView;
class KeyboardEvent;
class MouseEvent;
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

// Converts a WebCore::MouseEvent to a corresponding WebMouseEvent. view is the
// FrameView corresponding to the event.
// Returns true if successful.
// NOTE: This is only implemented for mousemove, mouseover, mouseout, mousedown
// and mouseup.
bool ToWebMouseEvent(const WebCore::FrameView& view,
                     const WebCore::MouseEvent& event,
                     WebKit::WebMouseEvent* web_event);

// Converts a WebCore::KeyboardEvent to a corresponding WebKeyboardEvent.
// Returns true if successful.
// NOTE: This is only implemented for keydown and keyup.
bool ToWebKeyboardEvent(const WebCore::KeyboardEvent& event,
                        WebKit::WebKeyboardEvent* web_event);

#endif  // WEBKIT_GLUE_EVENT_CONVERSION_H_
