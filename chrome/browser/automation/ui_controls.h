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

#ifndef CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H__
#define CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H__

#include <string>
#include <wtypes.h>

namespace ChromeViews {
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
void MoveMouseToCenterAndPress(
    ChromeViews::View* view, MouseButton button, int state, Task* task);

}  // ui_controls

#endif  // CHROME_BROWSER_AUTOMATION_UI_CONTROLS_H__