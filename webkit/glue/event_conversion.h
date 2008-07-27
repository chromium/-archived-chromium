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

#ifndef WEBKIT_GLUE_EVENT_CONVERSION_H__
#define WEBKIT_GLUE_EVENT_CONVERSION_H__

#pragma warning(push, 0)
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#pragma warning(pop)

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
  static long last_click_time_;
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
