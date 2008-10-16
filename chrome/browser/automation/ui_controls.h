// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H__
#define CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H__

#include <string>
#include <wtypes.h>

namespace views {
class View;
}

class Task;

namespace ui_controls {

// Many of the functions in this class include a variant that takes a Task.
// The version that takes a Task waits until the generated event is processed.
// Once the generated event is processed the Task is Run (and deleted).

// Send a key press with/without modifier keys.
bool SendKeyPress(wchar_t key, bool control, bool shift, bool alt);
bool SendKeyPressNotifyWhenDone(wchar_t key, bool control, bool shift,
                                bool alt, Task* task);

// Send a key down event. Use VK_CONTROL for ctrl key,
// VK_MENU for alt key and VK_SHIFT for shift key.
// Refer MSDN for more virtual key codes.
bool SendKeyDown(wchar_t key);
bool SendKeyUp(wchar_t key);

// Simulate a mouse move. (x,y) are absolute
// screen coordinates.
bool SendMouseMove(long x, long y);
void SendMouseMoveNotifyWhenDone(long x, long y, Task* task);

enum MouseButton {
  LEFT = 0,
  MIDDLE,
  RIGHT,
};

// Used to indicate the state of the button when generating events.
enum MouseButtonState {
  UP = 1,
  DOWN = 2
};

// Sends a mouse down and or up message.
bool SendMouseEvents(MouseButton type, int state);
void SendMouseEventsNotifyWhenDone(MouseButton type, int state, Task* task);

// Simulate a single mouse click with given button type.
bool SendMouseClick(MouseButton type);

// A combination of SendMouseMove to the middle of the view followed by
// SendMouseEvents.
void MoveMouseToCenterAndPress(views::View* view,
                               MouseButton button,
                               int state,
                               Task* task);

}  // ui_controls

#endif  // CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H__
