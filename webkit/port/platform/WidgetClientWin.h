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

#ifndef WidgetClientWin_H__
#define WidgetClientWin_H__

#include "WidgetClient.h"

class SkBitmap;

namespace WebCore {

class Cursor;
class IntRect;
class Range;

// Generic interface for features needed by the Widget.
class WidgetClientWin : public WidgetClient {
public:
    // Returns the containing window for the Widget.
    virtual HWND containingWindow() = 0;

    // Invalidate a region of the widget's containing window.
    virtual void invalidateRect(const IntRect& damagedRect) = 0;

    // Scroll the region of the widget's containing window within the given
    // clipRect by the specified dx and dy.
    virtual void scrollRect(int dx, int dy, const IntRect& clipRect) = 0;

    // Notifies the client of a new popup widget.  The client should place
    // and size the widget with the given bounds, relative to the screen.
    virtual void popupOpened(Widget* widget, const IntRect& bounds) = 0;

    // Notifies the client that the given popup widget has closed.
    virtual void popupClosed(Widget* widget) = 0;

    // Indicates that a new cursor should be shown.
    virtual void setCursor(const Cursor& cursor) = 0;

    // Indicates the widget thinks it has focus. This should give focus to the
    // window hosting the widget.
    virtual void setFocus() = 0;

    // This function is called to retrieve a resource bitmap from the
    // renderer that was cached as a result of the renderer receiving a
    // ViewMsg_Preload_Bitmap message from the browser.
    virtual const SkBitmap* getPreloadedResourceBitmap(int resource_id) = 0;

    // Notification that the given widget's scroll position has changed. This
    // function is called AFTER the position has been updated.
    virtual void onScrollPositionChanged(Widget* widget) = 0;

    // Retrieves the tick-marks for a given frame.
    virtual const WTF::Vector<RefPtr<WebCore::Range> >* getTickmarks(
        WebCore::Frame* frame) = 0;

    // Retrieves the index of the active tickmark for a given frame.  If the
    // frame does not have an active tickmark (for example if the active
    // tickmark resides in another frame) this function returns kNoTickmark.
    static const size_t kNoTickmark = -1;
    virtual size_t getActiveTickmarkIndex(WebCore::Frame* frame) = 0;
};

} // namespace WebCore

#endif  // WidgetClientWin_H__
