/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebInputEvent_h
#define WebInputEvent_h

#include <string.h>

#include "WebCommon.h"

namespace WebKit {

    // The classes defined in this file are intended to be used with
    // WebWidget's handleInputEvent method.  These event types are cross-
    // platform and correspond closely to WebCore's Platform*Event classes.
    //
    // WARNING! These classes must remain PODs (plain old data).  They are
    // intended to be "serializable" by copying their raw bytes, so they must
    // not contain any non-bit-copyable member variables!

    // WebInputEvent --------------------------------------------------------------

    class WebInputEvent {
    public:
        WebInputEvent(unsigned sizeParam = sizeof(WebInputEvent))
            : size(sizeParam)
            , type(Undefined)
            , modifiers(0)
            , timeStampSeconds(0.0) { }

        // There are two schemes used for keyboard input. On Windows (and,
        // interestingly enough, on Mac Carbon) there are two events for a
        // keypress.  One is a raw keydown, which provides the keycode only.
        // If the app doesn't handle that, then the system runs key translation
        // to create an event containing the generated character and pumps that
        // event.  In such a scheme, those two events are translated to
        // RAW_KEY_DOWN and CHAR events respectively.  In Cocoa and Gtk, key
        // events contain both the keycode and any translation into actual
        // text.  In such a case, WebCore will eventually need to split the
        // events (see disambiguateKeyDownEvent and its callers) but we don't
        // worry about that here.  We just use a different type (KEY_DOWN) to
        // indicate this.

        enum Type {
            Undefined = -1,

            // WebMouseEvent
            MouseDown,
            MouseUp,
            MouseMove,
            MouseEnter,
            MouseLeave,

            // WebMouseWheelEvent
            MouseWheel,

            // WebKeyboardEvent
            RawKeyDown,
            KeyDown,
            KeyUp,
            Char
        };

        enum Modifiers {
            // modifiers for all events:
            ShiftKey         = 1 << 0,
            ControlKey       = 1 << 1,
            AltKey           = 1 << 2,
            MetaKey          = 1 << 3,

            // modifiers for keyboard events:
            IsKeyPad         = 1 << 4,
            IsAutoRepeat     = 1 << 5,

            // modifiers for mouse events:
            LeftButtonDown   = 1 << 6,
            MiddleButtonDown = 1 << 7,
            RightButtonDown  = 1 << 8,
        };

        unsigned size;   // The size of this structure, for serialization.
        Type type;
        int modifiers;
        double timeStampSeconds;   // Seconds since epoch.

        // Returns true if the WebInputEvent |type| is a keyboard event.
        static bool isKeyboardEventType(int type)
        {
            return type == RawKeyDown
                || type == KeyDown
                || type == KeyUp
                || type == Char;
        }
    };

    // WebKeyboardEvent -----------------------------------------------------------

    class WebKeyboardEvent : public WebInputEvent {
    public:
        // Caps on string lengths so we can make them static arrays and keep
        // them PODs.
        static const size_t textLengthCap = 4;

        // http://www.w3.org/TR/DOM-Level-3-Events/keyset.html lists the
        // identifiers.  The longest is 18 characters, so we round up to the
        // next multiple of 4.
        static const size_t keyIdentifierLengthCap = 20;

        // |windowsKeyCode| is the Windows key code associated with this key
        // event.  Sometimes it's direct from the event (i.e. on Windows),
        // sometimes it's via a mapping function.  If you want a list, see
        // webkit/port/platform/chromium/KeyboardCodes* .
        int windowsKeyCode;

        // The actual key code genenerated by the platform.  The DOM spec runs
        // on Windows-equivalent codes (thus |windowsKeyCode| above) but it
        // doesn't hurt to have this one around.
        int nativeKeyCode;

        // |text| is the text generated by this keystroke.  |unmodifiedText| is
        // |text|, but unmodified by an concurrently-held modifiers (except
        // shift).  This is useful for working out shortcut keys.  Linux and
        // Windows guarantee one character per event.  The Mac does not, but in
        // reality that's all it ever gives.  We're generous, and cap it a bit
        // longer.
        WebUChar text[textLengthCap];
        WebUChar unmodifiedText[textLengthCap];

        // This is a string identifying the key pressed.
        char keyIdentifier[keyIdentifierLengthCap];

        // This identifies whether this event was tagged by the system as being
        // a "system key" event (see
        // http://msdn.microsoft.com/en-us/library/ms646286(VS.85).aspx for
        // details).  Other platforms don't have this concept, but it's just
        // easier to leave it always false than ifdef.
        bool isSystemKey;

        WebKeyboardEvent(unsigned sizeParam = sizeof(WebKeyboardEvent))
            : WebInputEvent(sizeParam)
            , windowsKeyCode(0)
            , nativeKeyCode(0)
            , isSystemKey(false)
        {
            memset(&text, 0, sizeof(text));
            memset(&unmodifiedText, 0, sizeof(unmodifiedText));
            memset(&keyIdentifier, 0, sizeof(keyIdentifier));
        }

        // Sets keyIdentifier based on the value of windowsKeyCode.  This is
        // handy for generating synthetic keyboard events.
        WEBKIT_API void setKeyIdentifierFromWindowsKeyCode();
    };

    // WebMouseEvent --------------------------------------------------------------

    class WebMouseEvent : public WebInputEvent {
    public:
        // These values defined for WebCore::MouseButton
        enum Button {
            ButtonNone = -1,
            ButtonLeft,
            ButtonMiddle,
            ButtonRight
        };

        Button button;
        int x;
        int y;
        int windowX;
        int windowY;
        int globalX;
        int globalY;
        int clickCount;

        WebMouseEvent(unsigned sizeParam = sizeof(WebMouseEvent))
            : WebInputEvent(sizeParam)
            , button(ButtonNone)
            , x(0)
            , y(0)
            , windowX(0)
            , windowY(0)
            , globalX(0)
            , globalY(0)
            , clickCount(0)
        {
        }
    };

    // WebMouseWheelEvent ---------------------------------------------------------

    class WebMouseWheelEvent : public WebMouseEvent {
    public:
        float deltaX;
        float deltaY;
        float wheelTicksX;
        float wheelTicksY;
        bool scrollByPage;

        WebMouseWheelEvent(unsigned sizeParam = sizeof(WebMouseWheelEvent))
            : WebMouseEvent(sizeParam)
            , deltaX(0.0f)
            , deltaY(0.0f)
            , wheelTicksX(0.0f)
            , wheelTicksY(0.0f)
            , scrollByPage(false)
        {
        }
    };

} // namespace WebKit

#endif
