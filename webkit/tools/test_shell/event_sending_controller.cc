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
// The behavior of queuing events and replaying them can be disabled by a
// layout test by setting eventSender.dragMode to false.

#include "webkit/tools/test_shell/event_sending_controller.h"

#include <queue>

// TODO(darin): This is very wrong.  We should not be including WebCore headers
// directly like this!!
#include "config.h"
#include "KeyboardCodes.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "webkit/glue/webinputevent_util.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell.h"

// TODO(mpcomplete): layout before each event?
// TODO(mpcomplete): do we need modifiers for mouse events?

using base::Time;
using base::TimeTicks;

TestShell* EventSendingController::shell_ = NULL;
gfx::Point EventSendingController::last_mouse_pos_;
WebMouseEvent::Button EventSendingController::pressed_button_ =
    WebMouseEvent::BUTTON_NONE;

int EventSendingController::last_button_number_ = -1;

namespace {

static scoped_ptr<WebDropData> drag_data_object;
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
  return (TimeTicks::Now().ToInternalValue()
            / Time::kMicrosecondsPerMillisecond +
          time_offset_ms) / 1000.0;
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

void ApplyKeyModifier(const std::wstring& arg, WebKeyboardEvent* event) {
  const wchar_t* arg_string = arg.c_str();
  if (!wcscmp(arg_string, L"ctrlKey")) {
    event->modifiers |= WebInputEvent::CTRL_KEY;
  } else if (!wcscmp(arg_string, L"shiftKey")) {
    event->modifiers |= WebInputEvent::SHIFT_KEY;
  } else if (!wcscmp(arg_string, L"altKey")) {
    event->modifiers |= WebInputEvent::ALT_KEY;
#if defined(OS_WIN)
    event->system_key = true;
#endif
  } else if (!wcscmp(arg_string, L"metaKey")) {
    event->modifiers |= WebInputEvent::META_KEY;
  }
}

void ApplyKeyModifiers(const CppVariant* arg, WebKeyboardEvent* event) {
  if (arg->isObject()) {
    std::vector<std::wstring> args = arg->ToStringVector();
    for (std::vector<std::wstring>::const_iterator i = args.begin();
         i != args.end(); ++i) {
      ApplyKeyModifier(*i, event);
    }
  } else if (arg->isString()) {
    ApplyKeyModifier(UTF8ToWide(arg->ToString()), event);
  }
}

}  // anonymous namespace

EventSendingController::EventSendingController(TestShell* shell)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
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
  BindMethod("dispatchMessage", &EventSendingController::dispatchMessage);
  BindMethod("enableDOMUIEventLogging",
             &EventSendingController::enableDOMUIEventLogging);
  BindMethod("fireKeyboardEventsToElement",
             &EventSendingController::fireKeyboardEventsToElement);
  BindMethod("clearKillRing", &EventSendingController::clearKillRing);
  BindMethod("textZoomIn", &EventSendingController::textZoomIn);
  BindMethod("textZoomOut", &EventSendingController::textZoomOut);
  BindMethod("scheduleAsynchronousClick",
             &EventSendingController::scheduleAsynchronousClick);

  // When set to true (the default value), we batch mouse move and mouse up
  // events so we can simulate drag & drop.
  BindProperty("dragMode", &dragMode);
#if defined(OS_WIN)
  BindProperty("WM_KEYDOWN", &wmKeyDown);
  BindProperty("WM_KEYUP", &wmKeyUp);
  BindProperty("WM_CHAR", &wmChar);
  BindProperty("WM_DEADCHAR", &wmDeadChar);
  BindProperty("WM_SYSKEYDOWN", &wmSysKeyDown);
  BindProperty("WM_SYSKEYUP", &wmSysKeyUp);
  BindProperty("WM_SYSCHAR", &wmSysChar);
  BindProperty("WM_SYSDEADCHAR", &wmSysDeadChar);
#endif
}

void EventSendingController::Reset() {
  // The test should have finished a drag and the mouse button state.
  DCHECK(!drag_data_object.get());
  drag_data_object.reset();
  pressed_button_ = WebMouseEvent::BUTTON_NONE;
  dragMode.Set(true);
#if defined(OS_WIN)
  wmKeyDown.Set(WM_KEYDOWN);
  wmKeyUp.Set(WM_KEYUP);
  wmChar.Set(WM_CHAR);
  wmDeadChar.Set(WM_DEADCHAR);
  wmSysKeyDown.Set(WM_SYSKEYDOWN);
  wmSysKeyUp.Set(WM_SYSKEYUP);
  wmSysChar.Set(WM_SYSCHAR);
  wmSysDeadChar.Set(WM_SYSDEADCHAR);
#endif
  last_click_time_sec = 0;
  click_count = 0;
  last_button_number_ = -1;
}

/* static */ WebView* EventSendingController::webview() {
  return shell_->webView();
}

/* static */ void EventSendingController::DoDragDrop(const WebDropData& data_obj) {
  WebDropData* drop_data_copy = new WebDropData;
  *drop_data_copy = data_obj;
  drag_data_object.reset(drop_data_copy);

  webview()->DragTargetDragEnter(data_obj, 0, 0, 0, 0);

  // Finish processing events.
  ReplaySavedEvents();
}

WebMouseEvent::Button EventSendingController::GetButtonTypeFromButtonNumber(
    int button_code) {
  if (button_code == 0)
    return WebMouseEvent::BUTTON_LEFT;
  else if (button_code == 2)
    return WebMouseEvent::BUTTON_RIGHT;

  return WebMouseEvent::BUTTON_MIDDLE;
}

// static
int EventSendingController::GetButtonNumberFromSingleArg(
    const CppArgumentList& args) {
  int button_code = 0;

  if (args.size() > 0 && args[0].isNumber()) {
    button_code = args[0].ToInt32();
  }

  return button_code;
}
//
// Implemented javascript methods.
//

void EventSendingController::mouseDown(
    const CppArgumentList& args, CppVariant* result) {
  if (result)  // Could be NULL if invoked asynchronously.
    result->SetNull();

  webview()->Layout();

  int button_number = GetButtonNumberFromSingleArg(args);
  DCHECK(button_number != -1);

  WebMouseEvent::Button button_type = GetButtonTypeFromButtonNumber(
      button_number);

  if ((GetCurrentEventTimeSec() - last_click_time_sec < kMultiClickTimeSec) &&
      (!outside_multiclick_radius(last_mouse_pos_, last_click_pos)) &&
      (button_number == last_button_number_)) {
    ++click_count;
  } else {
    click_count = 1;
  }

  last_button_number_ = button_number;

  WebMouseEvent event;
  pressed_button_ = button_type;
  InitMouseEvent(WebInputEvent::MOUSE_DOWN, button_type,
                 last_mouse_pos_, &event);
  webview()->HandleInputEvent(&event);
}

void EventSendingController::mouseUp(
    const CppArgumentList& args, CppVariant* result) {
  if (result)  // Could be NULL if invoked asynchronously.
    result->SetNull();

  webview()->Layout();

  int button_number = GetButtonNumberFromSingleArg(args);
  DCHECK(button_number != -1);

  WebMouseEvent::Button button_type = GetButtonTypeFromButtonNumber(
      button_number);

  last_button_number_ = button_number;

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
  if (drag_data_object.get()) {
    bool valid = webview()->DragTargetDragOver(e.x, e.y, e.global_x,
                                               e.global_y);
    if (valid) {
      webview()->DragSourceEndedAt(e.x, e.y, e.global_x, e.global_y);
      webview()->DragTargetDrop(e.x, e.y, e.global_x, e.global_y);
    } else {
      webview()->DragSourceEndedAt(e.x, e.y, e.global_x, e.global_y);
      webview()->DragTargetDragLeave();
    }
    drag_data_object.reset();
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

  if (pressed_button_ != WebMouseEvent::BUTTON_NONE && drag_data_object.get()) {
    webview()->DragSourceMovedTo(e.x, e.y, e.global_x, e.global_y);
    webview()->DragTargetDragOver(e.x, e.y, e.global_x, e.global_y);
  }
}

void EventSendingController::keyDown(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  bool generate_char = false;

  if (args.size() >= 1 && args[0].isString()) {
    // TODO(mpcomplete): I'm not exactly sure how we should convert the string
    // to a key event.  This seems to work in the cases I tested.
    // TODO(mpcomplete): Should we also generate a KEY_UP?
    std::wstring code_str = UTF8ToWide(args[0].ToString());

    // Convert \n -> VK_RETURN.  Some layout tests use \n to mean "Enter", when
    // Windows uses \r for "Enter".
    int code;
    bool needs_shift_key_modifier = false;
    if (L"\n" == code_str) {
      generate_char = true;
      code = WebCore::VKEY_RETURN;
    } else if (L"rightArrow" == code_str) {
      code = WebCore::VKEY_RIGHT;
    } else if (L"downArrow" == code_str) {
      code = WebCore::VKEY_DOWN;
    } else if (L"leftArrow" == code_str) {
      code = WebCore::VKEY_LEFT;
    } else if (L"upArrow" == code_str) {
      code = WebCore::VKEY_UP;
    } else if (L"delete" == code_str) {
      code = WebCore::VKEY_BACK;
    } else if (L"pageUp" == code_str) {
      code = WebCore::VKEY_PRIOR;
    } else if (L"pageDown" == code_str) {
      code = WebCore::VKEY_NEXT;
    } else {
      DCHECK(code_str.length() == 1);
      code = code_str[0];
      needs_shift_key_modifier = NeedsShiftModifier(code);
      generate_char = true;
    }

    // For one generated keyboard event, we need to generate a keyDown/keyUp
    // pair; refer to EventSender.cpp in WebKit/WebKitTools/DumpRenderTree/win.
    // On Windows, we might also need to generate a char event to mimic the
    // Windows event flow; on other platforms we create a merged event and test
    // the event flow that that platform provides.
    WebKeyboardEvent event_down, event_up;
#if defined(OS_WIN)
    event_down.type = WebInputEvent::RAW_KEY_DOWN;
#else
    event_down.type = WebInputEvent::KEY_DOWN;
#endif
    event_down.modifiers = 0;
    event_down.windows_key_code = code;
    if (generate_char) {
      event_down.text[0] = code;
      event_down.unmodified_text[0] = code;
    }
    std::string key_identifier_str =
        webkit_glue::GetKeyIdentifierForWindowsKeyCode(code);

    base::strlcpy(event_down.key_identifier, key_identifier_str.c_str(),
                  kIdentifierLengthCap);

    if (args.size() >= 2 && (args[1].isObject() || args[1].isString()))
      ApplyKeyModifiers(&(args[1]), &event_down);

    if (needs_shift_key_modifier)
      event_down.modifiers |= WebInputEvent::SHIFT_KEY;

    event_up = event_down;
    event_up.type = WebInputEvent::KEY_UP;
    // EventSendingController.m forces a layout here, with at least one
    // test (fast\forms\focus-control-to-page.html) relying on this.
    webview()->Layout();

    webview()->HandleInputEvent(&event_down);

#if defined(OS_WIN)
    if (generate_char) {
      WebKeyboardEvent event_char = event_down;
      event_char.type = WebInputEvent::CHAR;
      event_char.key_identifier[0] = '\0';
      webview()->HandleInputEvent(&event_char);
    }
#endif

    webview()->HandleInputEvent(&event_up);
  }
}

void EventSendingController::dispatchMessage(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

#if defined(OS_WIN)
  if (args.size() == 3) {
    // Grab the message id to see if we need to dispatch it.
    int msg = args[0].ToInt32();

    // WebKit's version of this function stuffs a MSG struct and uses
    // TranslateMessage and DispatchMessage. We use a WebKeyboardEvent, which
    // doesn't need to receive the DeadChar and SysDeadChar messages.
    if (msg == WM_DEADCHAR || msg == WM_SYSDEADCHAR)
      return;

    webview()->Layout();

    unsigned long lparam = static_cast<unsigned long>(args[2].ToDouble());
    WebKeyboardEvent key_event(0, msg, args[1].ToInt32(), lparam);
    webview()->HandleInputEvent(&key_event);
  } else {
    NOTREACHED() << L"Wrong number of arguments";
  }
#endif
}

bool EventSendingController::NeedsShiftModifier(int key_code) {
  // If code is an uppercase letter, assign a SHIFT key to
  // event_down.modifier, this logic comes from
  // WebKit/WebKitTools/DumpRenderTree/Win/EventSender.cpp
  if ((key_code & 0xFF) >= 'A' && (key_code & 0xFF) <= 'Z')
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

// Apple's port of WebKit zooms by a factor of 1.2 (see
// WebKit/WebView/WebView.mm)
void EventSendingController::textZoomIn(
    const CppArgumentList& args, CppVariant* result) {
  webview()->ZoomIn(true);
  result->SetNull();
}

void EventSendingController::textZoomOut(
    const CppArgumentList& args, CppVariant* result) {
  webview()->ZoomOut(true);
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

void EventSendingController::scheduleAsynchronousClick(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&EventSendingController::mouseDown,
                                        args, static_cast<CppVariant*>(NULL)));
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&EventSendingController::mouseUp,
                                        args, static_cast<CppVariant*>(NULL)));
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
