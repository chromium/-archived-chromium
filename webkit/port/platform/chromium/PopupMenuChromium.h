// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PopupMenuChromium_h
#define PopupMenuChromium_h

#include "config.h"

#if COMPILER(MSVC)
__pragma(warning(push, 0))
#endif
#include "PopupMenuClient.h"

#include "FramelessScrollView.h"
#include "IntRect.h"
#if COMPILER(MSVC)
__pragma(warning(pop))
#endif


namespace WebCore {
    
class PopupListBox;

// TODO(darin): Our FramelessScrollView classes need to implement HostWindow!

// This class holds a PopupListBox (see cpp file).  Its sole purpose is to be
// able to draw a border around its child.  All its paint/event handling is just
// forwarded to the child listBox (with the appropriate transforms).
// NOTE: this class is exposed so it can be instantiated direcly for the
// autofill popup.  We cannot use the Popup class directly in that case as the
// autofill popup should not be focused when shown and we want to forward the
// key events to it (through handleKeyEvent).

class PopupContainer : public FramelessScrollView, public RefCounted<PopupContainer>  {
public:
    static PassRefPtr<PopupContainer> create(PopupMenuClient* client,
                                             bool focusOnShow);

    // Whether a key event should be sent to this popup.
    virtual bool isInterestedInEventForKey(int key_code);

    // FramelessScrollView
    virtual void paint(GraphicsContext* gc, const IntRect& rect);
    virtual void hide();
    virtual bool handleMouseDownEvent(const PlatformMouseEvent& event);
    virtual bool handleMouseMoveEvent(const PlatformMouseEvent& event);
    virtual bool handleMouseReleaseEvent(const PlatformMouseEvent& event);
    virtual bool handleWheelEvent(const PlatformWheelEvent& event);
    virtual bool handleKeyEvent(const PlatformKeyboardEvent& event);

    // PopupContainer methods

    // Show the popup
    void showPopup(FrameView* view);

    // Show the popup in the specified rect for the specified frame.
    // Note: this code was somehow arbitrarily factored-out of the Popup class
    // so WebViewImpl can create a PopupContainer.
    void show(const IntRect& r, FrameView* v, int index);

    // Hide the popup.  Do not call this directly: use client->hidePopup().
    void hidePopup();

    // Compute size of widget and children.
    void layout();

    // Sets whether the PopupMenuClient should be told to change its text when a
    // new item is selected (by using the arrow keys).  Default is true.
    void setTextOnIndexChange(bool value);

    // Sets whether the selection should be accepted when the popup menu is
    // closed (through ESC being pressed or the focus going away).
    // Default is true.
    // Note that when TAB is pressed, the selection is always accepted
    // regardless of this setting.
    void setAcceptOnAbandon(bool value);

    PopupListBox* listBox() const { return m_listBox.get(); }

    // Refresh the popup values from the PopupMenuClient.
    void refresh();

private:
    friend class WTF::RefCounted<PopupContainer>;

    PopupContainer(PopupMenuClient* client, bool focusOnShow);
    ~PopupContainer();

    // Paint the border.
    void paintBorder(GraphicsContext* gc, const IntRect& rect);

    RefPtr<PopupListBox> m_listBox;

    // Whether the window showing this popup should be focused when shown.
    bool m_focusOnShow;
};

}

#endif // PopupMenuChromium_h
