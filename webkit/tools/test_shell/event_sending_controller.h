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


/*
  EventSendingController class:
  Bound to a JavaScript window.eventSender object using
  CppBoundClass::BindToJavascript(), this allows layout tests that are run in
  the test_shell to fire DOM events.
  
  The OSX reference file is in
  WebKit/WebKitTools/DumpRenderTree/EventSendingController.m
*/

#ifndef WEBKIT_TOOLS_TEST_SHELL_EVENT_SENDING_CONTROLLER_H__
#define WEBKIT_TOOLS_TEST_SHELL_EVENT_SENDING_CONTROLLER_H__

#include "base/gfx/point.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/glue/webinputevent.h"

struct IDataObject;
struct IDropSource;
class TestShell;
class WebView;

class EventSendingController : public CppBoundClass {
 public:
  // Builds the property and method lists needed to bind this class to a JS
  // object.
  EventSendingController(TestShell* shell);

  // Resets some static variable state.
  void Reset();

  // Simulate Windows' drag&drop system call.
  static void DoDragDrop(IDataObject* drag_data);

  // JS callback methods.
  void mouseDown(const CppArgumentList& args, CppVariant* result);
  void mouseUp(const CppArgumentList& args, CppVariant* result);
  void mouseMoveTo(const CppArgumentList& args, CppVariant* result);
  void leapForward(const CppArgumentList& args, CppVariant* result);
  void keyDown(const CppArgumentList& args, CppVariant* result);
  void textZoomIn(const CppArgumentList& args, CppVariant* result);
  void textZoomOut(const CppArgumentList& args, CppVariant* result);

  // Unimplemented stubs
  void contextClick(const CppArgumentList& args, CppVariant* result);
  void enableDOMUIEventLogging(const CppArgumentList& args, CppVariant* result);
  void fireKeyboardEventsToElement(const CppArgumentList& args, CppVariant* result);
  void clearKillRing(const CppArgumentList& args, CppVariant* result);
  CppVariant dragMode;

 private:
  // Returns the test shell's webview.
  static WebView* webview();

  // Returns true if dragMode is true.
  bool drag_mode() { return dragMode.isBool() && dragMode.ToBoolean(); }

  // Sometimes we queue up mouse move and mouse up events for drag drop
  // handling purposes.  These methods dispatch the event.
  static void DoMouseMove(const WebMouseEvent& e);
  static void DoMouseUp(const WebMouseEvent& e);
  static void ReplaySavedEvents();

  // Returns true if the key_code passed in needs a shift key modifier to
  // be passed into the generated event.
  bool NeedsShiftModifer(wchar_t key_code);

  // Non-owning pointer.  The LayoutTestController is owned by the host.
  static TestShell* shell_;

  // Location of last mouseMoveTo event.
  static gfx::Point last_mouse_pos_;

  // Currently pressed mouse button (Left/Right/Middle or None)
  static WebMouseEvent::Button pressed_button_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_EVENT_SENDING_CONTROLLER_H__
