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

static int gdkEventToWindowsKeyCode(const GdkEventKey* event)
{
    static const unsigned int hardwareCodeToGDKKeyval[] = {
        0,                 // 0x00:
        0,                 // 0x01:
        0,                 // 0x02:
        0,                 // 0x03:
        0,                 // 0x04:
        0,                 // 0x05:
        0,                 // 0x06:
        0,                 // 0x07:
        0,                 // 0x08:
        0,                 // 0x09: GDK_Escape
        GDK_1,             // 0x0A: GDK_1
        GDK_2,             // 0x0B: GDK_2
        GDK_3,             // 0x0C: GDK_3
        GDK_4,             // 0x0D: GDK_4
        GDK_5,             // 0x0E: GDK_5
        GDK_6,             // 0x0F: GDK_6
        GDK_7,             // 0x10: GDK_7
        GDK_8,             // 0x11: GDK_8
        GDK_9,             // 0x12: GDK_9
        GDK_0,             // 0x13: GDK_0
        GDK_minus,         // 0x14: GDK_minus
        GDK_equal,         // 0x15: GDK_equal
        0,                 // 0x16: GDK_BackSpace
        0,                 // 0x17: GDK_Tab
        GDK_q,             // 0x18: GDK_q
        GDK_w,             // 0x19: GDK_w
        GDK_e,             // 0x1A: GDK_e
        GDK_r,             // 0x1B: GDK_r
        GDK_t,             // 0x1C: GDK_t
        GDK_y,             // 0x1D: GDK_y
        GDK_u,             // 0x1E: GDK_u
        GDK_i,             // 0x1F: GDK_i
        GDK_o,             // 0x20: GDK_o
        GDK_p,             // 0x21: GDK_p
        GDK_bracketleft,   // 0x22: GDK_bracketleft
        GDK_bracketright,  // 0x23: GDK_bracketright
        0,                 // 0x24: GDK_Return
        0,                 // 0x25: GDK_Control_L
        GDK_a,             // 0x26: GDK_a
        GDK_s,             // 0x27: GDK_s
        GDK_d,             // 0x28: GDK_d
        GDK_f,             // 0x29: GDK_f
        GDK_g,             // 0x2A: GDK_g
        GDK_h,             // 0x2B: GDK_h
        GDK_j,             // 0x2C: GDK_j
        GDK_k,             // 0x2D: GDK_k
        GDK_l,             // 0x2E: GDK_l
        GDK_semicolon,     // 0x2F: GDK_semicolon
        GDK_apostrophe,    // 0x30: GDK_apostrophe
        GDK_grave,         // 0x31: GDK_grave
        0,                 // 0x32: GDK_Shift_L
        GDK_backslash,     // 0x33: GDK_backslash
        GDK_z,             // 0x34: GDK_z
        GDK_x,             // 0x35: GDK_x
        GDK_c,             // 0x36: GDK_c
        GDK_v,             // 0x37: GDK_v
        GDK_b,             // 0x38: GDK_b
        GDK_n,             // 0x39: GDK_n
        GDK_m,             // 0x3A: GDK_m
        GDK_comma,         // 0x3B: GDK_comma
        GDK_period,        // 0x3C: GDK_period
        GDK_slash,         // 0x3D: GDK_slash
        0,                 // 0x3E: GDK_Shift_R
    };

    // |windowsKeyCode| has to include a valid virtual-key code even when we
    // use non-US layouts, e.g. even when we type an 'A' key of a US keyboard
    // on the Hebrew layout, |windowsKeyCode| should be VK_A.
    // On the other hand, |event->keyval| value depends on the current
    // GdkKeymap object, i.e. when we type an 'A' key of a US keyboard on
    // the Hebrew layout, |event->keyval| becomes GDK_hebrew_shin and this
    // WebCore::windowsKeyCodeForKeyEvent() call returns 0.
    // To improve compatibilty with Windows, we use |event->hardware_keycode|
    // for retrieving its Windows key-code for the keys when the
    // WebCore::windowsKeyCodeForEvent() call returns 0.
    // We shouldn't use |event->hardware_keycode| for keys that GdkKeymap
    // objects cannot change because |event->hardware_keycode| doesn't change
    // even when we change the layout options, e.g. when we swap a control
    // key and a caps-lock key, GTK doesn't swap their
    // |event->hardware_keycode| values but swap their |event->keyval| values.
    int windowsKeyCode = WebCore::windowsKeyCodeForKeyEvent(event->keyval);
    if (windowsKeyCode)
        return windowsKeyCode;

    const int tableSize = sizeof(hardwareCodeToGDKKeyval) / sizeof(hardwareCodeToGDKKeyval[0]);
    if (event->hardware_keycode < tableSize) {
        int keyval = hardwareCodeToGDKKeyval[event->hardware_keycode];
        if (keyval)
            return WebCore::windowsKeyCodeForKeyEvent(keyval);
    }

    // This key is one that keyboard-layout drivers cannot change.
    // Use |event->keyval| to retrieve its |windowsKeyCode| value.
    return WebCore::windowsKeyCodeForKeyEvent(event->keyval);
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
        result.type = WebInputEvent::RawKeyDown;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    // The key code tells us which physical key was pressed (for example, the
    // A key went down or up).  It does not determine whether A should be lower
    // or upper case.  This is what text does, which should be the keyval.
    result.windowsKeyCode = gdkEventToWindowsKeyCode(event);
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

WebKeyboardEvent WebInputEventFactory::keyboardEvent(wchar_t character, int state, double timeStampSeconds)
{
    // keyboardEvent(const GdkEventKey*) depends on the GdkEventKey object and
    // it is hard to use/ it from signal handlers which don't use GdkEventKey
    // objects (e.g. GtkIMContext signal handlers.) For such handlers, this
    // function creates a WebInputEvent::Char event without using a
    // GdkEventKey object.
    WebKeyboardEvent result;
    result.type = WebKit::WebInputEvent::Char;
    result.timeStampSeconds = timeStampSeconds;
    result.modifiers = gdkStateToWebEventModifiers(state);
    result.windowsKeyCode = character;
    result.nativeKeyCode = character;
    result.text[0] = character;
    result.unmodifiedText[0] = character;
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
