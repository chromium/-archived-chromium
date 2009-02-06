// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/*
  EventSendingController class:
  Bound to a JavaScript window.eventSender object using
  CppBoundClass::BindToJavascript(), this allows layout tests that are run in
  the test_shell to fire DOM events.
  
  The OSX reference file is in
  WebKit/WebKitTools/DumpRenderTree/EventSendingController.m
*/

#ifndef WEBKIT_TOOLS_TEST_SHELL_EVENT_SENDING_CONTROLLER_H_
#define WEBKIT_TOOLS_TEST_SHELL_EVENT_SENDING_CONTROLLER_H_

#include "build/build_config.h"
#include "base/gfx/point.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webinputevent.h"

class TestShell;
class WebView;

class EventSendingController : public CppBoundClass {
 public:
  // Builds the property and method lists needed to bind this class to a JS
  // object.
  EventSendingController(TestShell* shell);

  // Resets some static variable state.
  void Reset();

  // Simulate drag&drop system call.
  static void DoDragDrop(const WebDropData& drag_data);

  // JS callback methods.
  void mouseDown(const CppArgumentList& args, CppVariant* result);
  void mouseUp(const CppArgumentList& args, CppVariant* result);
  void mouseMoveTo(const CppArgumentList& args, CppVariant* result);
  void leapForward(const CppArgumentList& args, CppVariant* result);
  void keyDown(const CppArgumentList& args, CppVariant* result);
  void dispatchMessage(const CppArgumentList& args, CppVariant* result);
  void textZoomIn(const CppArgumentList& args, CppVariant* result);
  void textZoomOut(const CppArgumentList& args, CppVariant* result);

  // Unimplemented stubs
  void contextClick(const CppArgumentList& args, CppVariant* result);
  void enableDOMUIEventLogging(const CppArgumentList& args, CppVariant* result);
  void fireKeyboardEventsToElement(const CppArgumentList& args, CppVariant* result);
  void clearKillRing(const CppArgumentList& args, CppVariant* result);
  CppVariant dragMode;

  // Properties used in layout tests.
#if defined(OS_WIN)
  CppVariant wmKeyDown;
  CppVariant wmKeyUp;
  CppVariant wmChar;
  CppVariant wmDeadChar;
  CppVariant wmSysKeyDown;
  CppVariant wmSysKeyUp;
  CppVariant wmSysChar;
  CppVariant wmSysDeadChar;
#endif

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

  // Helper to return the button type given a button code
  static WebMouseEvent::Button GetButtonTypeFromButtonNumber(int button_code);

  // Helper to extract the button number from the optional argument in
  // mouseDown and mouseUp
  static int GetButtonNumberFromSingleArg(const CppArgumentList& args);

  // Returns true if the key_code passed in needs a shift key modifier to
  // be passed into the generated event.
  bool NeedsShiftModifer(int key_code);

  // Non-owning pointer.  The LayoutTestController is owned by the host.
  static TestShell* shell_;

  // Location of last mouseMoveTo event.
  static gfx::Point last_mouse_pos_;

  // Currently pressed mouse button (Left/Right/Middle or None)
  static WebMouseEvent::Button pressed_button_;

  // The last button number passed to mouseDown and mouseUp.
  // Used to determine whether the click count continues to
  // increment or not.
  static int last_button_number_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_EVENT_SENDING_CONTROLLER_H_

