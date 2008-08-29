// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WidgetClientWin_H__
#define WidgetClientWin_H__

#include "base/gfx/native_widget_types.h"
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
    // TODO(pinkerton): this needs a better name, "window" is incorrect on other
    // platforms.
    virtual gfx::ViewHandle containingWindow() = 0;

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

