// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the definition for EventSendingController.
//
// Some notes about drag and drop handling:
// Windows drag and drop goes through a system call to DoDragDrop.  At that
// point, program control is given to Windows which then periodically makes
// callbacks into the webview.  This won't work for layout tests, so instead,
// we queue up all the mouse move and mouse up events.  When the test tries to
// start a drag (by calling EvenSendingController::DoDragDrop), we take the
// events in the queue and replay them.
// The behavior of queueing events and replaying them can be disabled by a
// layout test by setting eventSender.dragMode to false.

#include "webkit/tools/test_shell/event_sending_controller.h"

#include <objidl.h>
#include <queue>

#include "base/ref_counted.h"
#include "base/string_util.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell.h"

// TODO(mpcomplete): layout before each event?
// TODO(mpcomplete): do we need modifiers for mouse events?

TestShell* EventSendingController::shell_ = NULL;
gfx::Point EventSendingController::last_mouse_pos_;
WebMouseEvent::Button EventSendingController::pressed_button_ = 
    WebMouseEvent::BUTTON_NONE;

namespace {

static scoped_refptr<IDataObject> drag_data_object;
static bool replaying_saved_events = false;
static std::queue<WebMouseEvent> mouse_event_queue;

// Time and place of the last mouse up event.
static double last_click_time_sec = 0;  
static gfx::Point last_click_pos;
static int click_count = 0;

// maximum distance (in space and time) for a mouse click
// to register as a double or triple click
static const double kMultiClickTimeSec = 1;
static const int kMultiClickRadiusPixels = 5;

inline bool outside_multiclick_radius(const gfx::Point &a, const gfx::Point &b) {
  return ((a.x() - b.x()) * (a.x() - b.x()) + (a.y() - b.y()) * (a.y() - b.y())) > 
    kMultiClickRadiusPixels * kMultiClickRadiusPixels;
}

// Used to offset the time the event hander things an event happened.  This is
// done so tests can run without a delay, but bypass checks that are time
// dependent (e.g., dragging has a timeout vs selection).
static uint32 time_offset_ms = 0;

double GetCurrentEventTimeSec() {
  return (GetTickCount() + time_offset_ms) / 1000.0;
}

void AdvanceEventTime(int32 delta_ms) {
  time_offset_ms += delta_ms;
}

void InitMouseEvent(WebInputEvent::Type t, WebMouseEvent::Button b,
                    const gfx::Point& pos, WebMouseEvent* e) {
  e->type = t;
  e->button = b;
  e->modifiers = 0;
  e->x = pos.x();
  e->y = pos.y();
  e->global_x = pos.x();
  e->global_y = pos.y();
  e->timestamp_sec = GetCurrentEventTimeSec();
  e->layout_test_click_count = click_count;
}

void ApplyKeyModifiers(const CppVariant* arg, WebKeyboardEvent* event) {
  std::vector<std::wstring> args = arg->ToStringVector();
  for (std::vector<std::wstring>::iterator i = args.begin();
       i != args.end(); ++i) {
    const wchar_t* arg_string = (*i).c_str();
    if (!wcscmp(arg_string, L"ctrlKey")) {
      event->modifiers |= WebInputEvent::CTRL_KEY;
    } else if (!wcscmp(arg_string, L"shiftKey")) {
      event->modifiers |= WebInputEvent::SHIFT_KEY;
    } else if (!wcscmp(arg_string, L"altKey")) {
      event->modifiers |= WebInputEvent::ALT_KEY;
      event->system_key = true;
    } else if (!wcscmp(arg_string, L"metaKey")) {
      event->modifiers |= WebInputEvent::META_KEY;
    }
  }
}

}  // anonymous namespace

EventSendingController::EventSendingController(TestShell* shell) {
  // Set static shell_ variable since we can't do it in an initializer list. 
  // We also need to be careful not to assign shell_ to new windows which are
  // temporary.
  if (NULL == shell_)
    shell_ = shell;

  // Initialize the map that associates methods of this class with the names
  // they will use when called by JavaScript.  The actual binding of those
  // names to their methods will be done by calling BindToJavaScript() (defined
  // by CppBoundClass, the parent to EventSendingController).
  BindMethod("mouseDown", &EventSendingController::mouseDown);
  BindMethod("mouseUp", &EventSendingController::mouseUp);
  BindMethod("contextClick", &EventSendingController::contextClick);
  BindMethod("mouseMoveTo", &EventSendingController::mouseMoveTo);
  BindMethod("leapForward", &EventSendingController::leapForward);
  BindMethod("keyDown", &EventSendingController::keyDown);
  BindMethod("enableDOMUIEventLogging", &EventSendingController::enableDOMUIEventLogging);
  BindMethod("fireKeyboardEventsToElement", &EventSendingController::fireKeyboardEventsToElement);
  BindMethod("clearKillRing", &EventSendingController::clearKillRing);
  BindMethod("textZoomIn", &EventSendingController::textZoomIn);
  BindMethod("textZoomOut", &EventSendingController::textZoomOut);

  // When set to true (the default value), we batch mouse move and mouse up
  // events so we can simulate drag & drop.
  BindProperty("dragMode", &dragMode);
}

void EventSendingController::Reset() {
  // The test should have finished a drag and the mouse button state.
  DCHECK(!drag_data_object);
  drag_data_object = NULL;
  pressed_button_ = WebMouseEvent::BUTTON_NONE;
  dragMode.Set(true);
  last_click_time_sec = 0;
  click_count = 0;
}

/* static */ WebView* EventSendingController::webview() {
  return shell_->webView();
}

/* static */ void EventSendingController::DoDragDrop(IDataObject* data_obj) {
  drag_data_object = data_obj;

  DWORD effect = 0;
  POINTL screen_ptl = {0, 0};
  TestWebViewDelegate* delegate = shell_->delegate();
  delegate->drop_delegate()->DragEnter(drag_data_object, MK_LBUTTON,
                                       screen_ptl, &effect);

  // Finish processing events.
  ReplaySavedEvents();
}

// static
WebMouseEvent::Button EventSendingController::GetButtonTypeFromSingleArg(
    const CppArgumentList& args) {
  if (args.size() > 0 && args[0].isNumber()) {
    int button_code = args[0].ToInt32();
    if (button_code == 1)
      return WebMouseEvent::BUTTON_MIDDLE;
    else if (button_code == 2)
      return WebMouseEvent::BUTTON_RIGHT;
  }
  return WebMouseEvent::BUTTON_LEFT;
}

//
// Implemented javascript methods.
//

 void EventSendingController::mouseDown(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  webview()->Layout();

  WebMouseEvent::Button button_type = GetButtonTypeFromSingleArg(args);

  if ((GetCurrentEventTimeSec() - last_click_time_sec >= kMultiClickTimeSec) ||
      outside_multiclick_radius(last_mouse_pos_, last_click_pos)) {
    click_count = 1;
  } else {
    ++click_count;
  }

  WebMouseEvent event;
  pressed_button_ = button_type;
  InitMouseEvent(WebInputEvent::MOUSE_DOWN, button_type,
                 last_mouse_pos_, &event);
  webview()->HandleInputEvent(&event);
}

 void EventSendingController::mouseUp(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  webview()->Layout();

  WebMouseEvent::Button button_type = GetButtonTypeFromSingleArg(args);

  WebMouseEvent event;
  InitMouseEvent(WebInputEvent::MOUSE_UP, button_type,
                 last_mouse_pos_, &event);
  if (drag_mode() && !replaying_saved_events) {
    mouse_event_queue.push(event);
    ReplaySavedEvents();
  } else {
    DoMouseUp(event);
  }

  last_click_time_sec = event.timestamp_sec;
  last_click_pos = gfx::Point(event.x, event.y);
}

/* static */ void EventSendingController::DoMouseUp(const WebMouseEvent& e) {
  webview()->HandleInputEvent(&e);
  pressed_button_ = WebMouseEvent::BUTTON_NONE;

  // If we're in a drag operation, complete it.
  if (drag_data_object) {
    TestWebViewDelegate* delegate = shell_->delegate();
    // Get screen mouse position.
    POINT screen_pt = { static_cast<LONG>(e.x),
                        static_cast<LONG>(e.y) };
    ClientToScreen(shell_->webViewWnd(), &screen_pt);
    POINTL screen_ptl = { screen_pt.x, screen_pt.y };
    
    DWORD effect = 0;
    delegate->drop_delegate()->DragOver(0, screen_ptl, &effect);
    HRESULT hr = delegate->drag_delegate()->QueryContinueDrag(0, 0);
    if (hr == DRAGDROP_S_DROP && effect != DROPEFFECT_NONE) {
      DWORD effect = 0;
      delegate->drop_delegate()->Drop(drag_data_object.get(), 0, screen_ptl,
                                      &effect);
    } else {
      delegate->drop_delegate()->DragLeave();
    }
    drag_data_object = NULL;
  }
}

 void EventSendingController::mouseMoveTo(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  if (args.size() >= 2 && args[0].isNumber() && args[1].isNumber()) {
    webview()->Layout();

    WebMouseEvent event;
    last_mouse_pos_.SetPoint(args[0].ToInt32(), args[1].ToInt32());
    InitMouseEvent(WebInputEvent::MOUSE_MOVE, pressed_button_,
                   last_mouse_pos_, &event);

    if (drag_mode() && pressed_button_ != WebMouseEvent::BUTTON_NONE &&
        !replaying_saved_events) {
      mouse_event_queue.push(event);
    } else {
      DoMouseMove(event);
    }
  }
}

/* static */ void EventSendingController::DoMouseMove(const WebMouseEvent& e) {
  webview()->HandleInputEvent(&e);

  if (pressed_button_ != WebMouseEvent::BUTTON_NONE && drag_data_object) {
    TestWebViewDelegate* delegate = shell_->delegate();
    // Get screen mouse position.
    POINT screen_pt = { static_cast<LONG>(e.x),
                        static_cast<LONG>(e.y) };
    ClientToScreen(shell_->webViewWnd(), &screen_pt);
    POINTL screen_ptl = { screen_pt.x, screen_pt.y };

    HRESULT hr = delegate->drag_delegate()->QueryContinueDrag(0, MK_LBUTTON);
    DWORD effect = 0;
    delegate->drop_delegate()->DragOver(MK_LBUTTON, screen_ptl, &effect);

    delegate->drag_delegate()->GiveFeedback(effect);
  }
}

 void EventSendingController::keyDown(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  static const int kPercentVirtualKeyCode = 0x25;
  static const int kAmpersandVirtualKeyCode = 0x26;

  static const int kLeftParenthesesVirtualKeyCode = 0x28;
  static const int kRightParenthesesVirtualKeyCode = 0x29;

  static const int kLeftCurlyBracketVirtualKeyCode = 0x7B;
  static const int kRightCurlyBracketVirtualKeyCode = 0x7D;

  bool generate_char = false;

  if (args.size() >= 1 && args[0].isString()) {
    // TODO(mpcomplete): I'm not exactly sure how we should convert the string
    // to a key event.  This seems to work in the cases I tested.
    // TODO(mpcomplete): Should we also generate a KEY_UP?
    std::wstring code_str = UTF8ToWide(args[0].ToString());

    // Convert \n -> VK_RETURN.  Some layout tests use \n to mean "Enter", when
    // Windows uses \r for "Enter".
    wchar_t code;
    bool needs_shift_key_modifier = false;
    if (L"\n" == code_str) {
      generate_char = true;
      code = VK_RETURN;
    } else if (L"rightArrow" == code_str) {
      code = VK_RIGHT;
    } else if (L"downArrow" == code_str) {
      code = VK_DOWN;
    } else if (L"leftArrow" == code_str) {
      code = VK_LEFT;
    } else if (L"upArrow" == code_str) {
      code = VK_UP;
    } else if (L"delete" == code_str) {
      code = VK_BACK;
    } else {
      DCHECK(code_str.length() == 1);
      code = code_str[0];
      needs_shift_key_modifier = NeedsShiftModifer(code);
      generate_char = true;
    }

    // NOTE(jnd):For one keydown event, we need to generate
    // keyDown/keyUp pair, refer EventSender.cpp in
    // WebKit/WebKitTools/DumpRenderTree/win. We may also need
    // to generate a keyChar event in certain cases.
    WebKeyboardEvent event_down, event_up;
    event_down.type = WebInputEvent::KEY_DOWN;
    event_down.modifiers = 0;
    event_down.key_code = code;
    event_down.key_data = code;

    if (args.size() >= 2 && args[1].isObject())
      ApplyKeyModifiers(&(args[1]), &event_down);

    if (needs_shift_key_modifier)
      event_down.modifiers |= WebInputEvent::SHIFT_KEY;

    event_up = event_down;
    event_up.type = WebInputEvent::KEY_UP;
    // EventSendingController.m forces a layout here, with at least one
    // test (fast\forms\focus-control-to-page.html) relying on this.
    webview()->Layout();

    webview()->HandleInputEvent(&event_down);

    if (generate_char) {
      WebKeyboardEvent event_char = event_down;
      if (event_down.modifiers & WebInputEvent::SHIFT_KEY) {
        // Special case for the following characters when the shift key is
        // pressed in conjunction with these characters.
        // Windows generates a WM_KEYDOWN message with the ASCII code of 
        // the character followed by a WM_CHAR for the corresponding
        // virtual key code.
        // We check for these keys to catch regressions in keyEvent handling
        // in webkit.
        switch(code) {
          case '5':
            event_char.key_code = kPercentVirtualKeyCode;
            event_char.key_data = kPercentVirtualKeyCode;
            break;
          case '7':
            event_char.key_code = kAmpersandVirtualKeyCode;
            event_char.key_data = kAmpersandVirtualKeyCode;
            break;
          case '9':
            event_char.key_code = kLeftParenthesesVirtualKeyCode;
            event_char.key_data = kLeftParenthesesVirtualKeyCode;
            break;
          case '0':
            event_char.key_code = kRightParenthesesVirtualKeyCode;
            event_char.key_data = kRightParenthesesVirtualKeyCode;
            break;
          //  '[{' for US
          case VK_OEM_4:
            event_char.key_code = kLeftCurlyBracketVirtualKeyCode;
            event_char.key_data = kLeftCurlyBracketVirtualKeyCode;
            break;
          //  ']}' for US
          case VK_OEM_6:
            event_char.key_code = kRightCurlyBracketVirtualKeyCode;
            event_char.key_data = kRightCurlyBracketVirtualKeyCode;
            break;
          default:
            break;
        }
      }
      event_char.type = WebInputEvent::CHAR;
      webview()->HandleInputEvent(&event_char);
    }

    webview()->HandleInputEvent(&event_up);
  }
}

 bool EventSendingController::NeedsShiftModifer(wchar_t key_code) {
  // If code is an uppercase letter, assign a SHIFT key to
  // event_down.modifier, this logic comes from
  // WebKit/WebKitTools/DumpRenderTree/Win/EventSender.cpp
  if ((LOBYTE(key_code)) >= 'A' && (LOBYTE(key_code)) <= 'Z')
    return true;
  return false;
 }

 void EventSendingController::leapForward(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  // TODO(mpcomplete): DumpRenderTree defers this under certain conditions.

  if (args.size() >=1 && args[0].isNumber()) {
    AdvanceEventTime(args[0].ToInt32());
  }
}

// Apple's port of webkit zooms by a factor of 1.2 (see
// WebKit/WebView/WebView.mm)
 void EventSendingController::textZoomIn(
    const CppArgumentList& args, CppVariant* result) {
  webview()->MakeTextLarger();
  result->SetNull();
}

 void EventSendingController::textZoomOut(
    const CppArgumentList& args, CppVariant* result) {
  webview()->MakeTextSmaller();
  result->SetNull();
}

 void EventSendingController::ReplaySavedEvents() {
  replaying_saved_events = true;
  while (!mouse_event_queue.empty()) {
    WebMouseEvent event = mouse_event_queue.front();
    mouse_event_queue.pop();

    switch (event.type) {
      case WebInputEvent::MOUSE_UP:
        DoMouseUp(event);
        break;
      case WebInputEvent::MOUSE_MOVE:
        DoMouseMove(event);
        break;
      default:
        NOTREACHED();
    }
  }
  
  replaying_saved_events = false;
}

 void EventSendingController::contextClick(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  
  webview()->Layout();

  if (GetCurrentEventTimeSec() - last_click_time_sec >= 1) {
    click_count = 1;
  } else {
    ++click_count;
  }

  // Generate right mouse down and up.

  WebMouseEvent event;
  pressed_button_ = WebMouseEvent::BUTTON_RIGHT;
  InitMouseEvent(WebInputEvent::MOUSE_DOWN, WebMouseEvent::BUTTON_RIGHT,
                 last_mouse_pos_, &event);
  webview()->HandleInputEvent(&event);

  InitMouseEvent(WebInputEvent::MOUSE_UP, WebMouseEvent::BUTTON_RIGHT,
                 last_mouse_pos_, &event);
  webview()->HandleInputEvent(&event);

  pressed_button_ = WebMouseEvent::BUTTON_NONE;
}

//
// Unimplemented stubs
//

 void EventSendingController::enableDOMUIEventLogging(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

 void EventSendingController::fireKeyboardEventsToElement(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

 void EventSendingController::clearKillRing(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

