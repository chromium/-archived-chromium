/*
 * Copyright (C) 2006-2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebInputEventFactory.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkversion.h>

#include "WebInputEvent.h"

#include "KeyboardCodes.h"
#include "KeyCodeConversion.h"
#include <wtf/Assertions.h>

namespace WebKit {

static double gdkEventTimeToWebEventTime(guint32 time)
{
    // Convert from time in ms to time in sec.
    return time / 1000.0;
}

static int gdkStateToWebEventModifiers(guint state)
{
    int modifiers = 0;
    if (state & GDK_SHIFT_MASK)
        modifiers |= WebInputEvent::ShiftKey;
    if (state & GDK_CONTROL_MASK)
        modifiers |= WebInputEvent::ControlKey;
    if (state & GDK_MOD1_MASK)
        modifiers |= WebInputEvent::AltKey;
#if GTK_CHECK_VERSION(2,10,0)
    if (state & GDK_META_MASK)
        modifiers |= WebInputEvent::MetaKey;
#endif
    if (state & GDK_BUTTON1_MASK)
        modifiers |= WebInputEvent::LeftButtonDown;
    if (state & GDK_BUTTON2_MASK)
        modifiers |= WebInputEvent::MiddleButtonDown;
    if (state & GDK_BUTTON3_MASK)
        modifiers |= WebInputEvent::RightButtonDown;
    return modifiers;
}

// WebKeyboardEvent -----------------------------------------------------------

WebKeyboardEvent WebInputEventFactory::keyboardEvent(const GdkEventKey* event)
{
    WebKeyboardEvent result;

    result.timeStampSeconds = gdkEventTimeToWebEventTime(event->time);
    result.modifiers = gdkStateToWebEventModifiers(event->state);

    switch (event->type) {
    case GDK_KEY_RELEASE:
        result.type = WebInputEvent::KeyUp;
        break;
    case GDK_KEY_PRESS:
        result.type = WebInputEvent::KeyDown;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    // The key code tells us which physical key was pressed (for example, the
    // A key went down or up).  It does not determine whether A should be lower
    // or upper case.  This is what text does, which should be the keyval.
    result.windowsKeyCode = WebCore::windowsKeyCodeForKeyEvent(event->keyval);
    result.nativeKeyCode = event->hardware_keycode;

    switch (event->keyval) {
    // We need to treat the enter key as a key press of character \r.  This
    // is apparently just how webkit handles it and what it expects.
    case GDK_ISO_Enter:
    case GDK_KP_Enter:
    case GDK_Return:
        result.unmodifiedText[0] = result.text[0] = static_cast<WebUChar>('\r');
        break;
    default:
        // This should set text to 0 when it's not a real character.
        // FIXME: fix for non BMP chars
        result.unmodifiedText[0] = result.text[0] =
            static_cast<WebUChar>(gdk_keyval_to_unicode(event->keyval));
    }

    result.setKeyIdentifierFromWindowsKeyCode();

    // FIXME: Do we need to set IsAutoRepeat or IsKeyPad?

    return result;
}

// WebMouseEvent --------------------------------------------------------------

WebMouseEvent WebInputEventFactory::mouseEvent(const GdkEventButton* event)
{
    WebMouseEvent result;

    result.timeStampSeconds = gdkEventTimeToWebEventTime(event->time);

    result.modifiers = gdkStateToWebEventModifiers(event->state);
    result.x = static_cast<int>(event->x);
    result.y = static_cast<int>(event->y);
    result.windowX = result.x;
    result.windowY = result.y;
    result.globalX = static_cast<int>(event->x_root);
    result.globalY = static_cast<int>(event->y_root);
    result.clickCount = 0;

    switch (event->type) {
    case GDK_3BUTTON_PRESS:
        ++result.clickCount;
        // fallthrough
    case GDK_2BUTTON_PRESS:
        ++result.clickCount;
        // fallthrough
    case GDK_BUTTON_PRESS:
        result.type = WebInputEvent::MouseDown;
        ++result.clickCount;
        break;
    case GDK_BUTTON_RELEASE:
        result.type = WebInputEvent::MouseUp;
        break;

    default:
        ASSERT_NOT_REACHED();
    };

    result.button = WebMouseEvent::ButtonNone;
    if (event->button == 1)
        result.button = WebMouseEvent::ButtonLeft;
    else if (event->button == 2)
        result.button = WebMouseEvent::ButtonMiddle;
    else if (event->button == 3)
        result.button = WebMouseEvent::ButtonRight;

    return result;
}

WebMouseEvent WebInputEventFactory::mouseEvent(const GdkEventMotion* event)
{
    WebMouseEvent result;

    result.timeStampSeconds = gdkEventTimeToWebEventTime(event->time);
    result.modifiers = gdkStateToWebEventModifiers(event->state);
    result.x = static_cast<int>(event->x);
    result.y = static_cast<int>(event->y);
    result.windowX = result.x;
    result.windowY = result.y;
    result.globalX = static_cast<int>(event->x_root);
    result.globalY = static_cast<int>(event->y_root);

    switch (event->type) {
    case GDK_MOTION_NOTIFY:
        result.type = WebInputEvent::MouseMove;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    result.button = WebMouseEvent::ButtonNone;
    if (event->state & GDK_BUTTON1_MASK)
        result.button = WebMouseEvent::ButtonLeft;
    else if (event->state & GDK_BUTTON2_MASK)
        result.button = WebMouseEvent::ButtonMiddle;
    else if (event->state & GDK_BUTTON3_MASK)
        result.button = WebMouseEvent::ButtonRight;

    return result;
}

// WebMouseWheelEvent ---------------------------------------------------------

WebMouseWheelEvent WebInputEventFactory::mouseWheelEvent(const GdkEventScroll* event)
{
    WebMouseWheelEvent result;

    result.type = WebInputEvent::MouseWheel;
    result.button = WebMouseEvent::ButtonNone;

    result.timeStampSeconds = gdkEventTimeToWebEventTime(event->time);
    result.modifiers = gdkStateToWebEventModifiers(event->state);
    result.x = static_cast<int>(event->x);
    result.y = static_cast<int>(event->y);
    result.windowX = result.x;
    result.windowY = result.y;
    result.globalX = static_cast<int>(event->x_root);
    result.globalY = static_cast<int>(event->y_root);

    // How much should we scroll per mouse wheel event?
    // - Windows uses 3 lines by default and obeys a system setting.
    // - Mozilla has a pref that lets you either use the "system" number of lines
    //   to scroll, or lets the user override it.
    //   For the "system" number of lines, it appears they've hardcoded 3.
    //   See case NS_MOUSE_SCROLL in content/events/src/nsEventStateManager.cpp
    //   and InitMouseScrollEvent in widget/src/gtk2/nsCommonWidget.cpp .
    // - Gtk makes the scroll amount a function of the size of the scroll bar,
    //   which is not available to us here.
    // Instead, we pick a number that empirically matches Firefox's behavior.
    static const float scrollbarPixelsPerTick = 160.0f / 3.0f;

    switch (event->direction) {
    case GDK_SCROLL_UP:
        result.deltaY = scrollbarPixelsPerTick;
        result.wheelTicksY = 1;
        break;
    case GDK_SCROLL_DOWN:
        result.deltaY = -scrollbarPixelsPerTick;
        result.wheelTicksY = -1;
        break;
    case GDK_SCROLL_LEFT:
        result.deltaX = scrollbarPixelsPerTick;
        result.wheelTicksX = -1;  // Match Windows positive/negative orientation
        break;
    case GDK_SCROLL_RIGHT:
        result.deltaX = -scrollbarPixelsPerTick;
        result.wheelTicksX = 1;
        break;
    }

    return result;
}

} // namespace WebKit
