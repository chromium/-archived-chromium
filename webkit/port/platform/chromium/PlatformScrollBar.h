/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformScrollbar_h
#define PlatformScrollbar_h

#include "Widget.h"
#include "ScrollBar.h"
#include "Timer.h"

namespace WebCore {

// IMPORTANT NOTES ABOUT SCROLLBARS
//
// WebKit uses scrollbars in two ways.  The first way is as a scroll control
// for a ScrollView.  This scrollbar sits inside the ScrollView's rect and
// modifies its scrollOffset.  Because it is inside the ScrollView's rect, it
// is a child of the ScrollView, but because it is not really part of the
// scrollView's content, it doesn't move as the scrollOffset changes.
//
// The second use is as a scroll control for things other than a ScrollView,
// e.g. a <select>.  A <select> is not a ScrollView, so the scrollbar is not a
// child of it -- instead, it is a child of the ScrollView representing the
// frame in which the <select> (and the scrollbar) are located.  In this case,
// the scrollbar IS part of the ScrollView's content, and it moves when the
// scrollOffset changes.
//
// ScrollViewWin distinguishes these two cases in its convertChildToSelf and
// convertSelfToChild methods, which are used when converting coordinates
// between the ScrollBar's coordinate system and that of the native window.

class PlatformScrollbar : public Widget, public Scrollbar {
public:
    static PassRefPtr<PlatformScrollbar> create(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size)
    {
        return adoptRef(new PlatformScrollbar(client, orientation, size));
    }
    virtual ~PlatformScrollbar();

    virtual bool isWidget() const { return true; }

    virtual int width() const;
    virtual int height() const;
    virtual void setRect(const IntRect&);
    virtual void setEnabled(bool);
    virtual bool isEnabled() const { return m_enabled; }
    virtual void paint(GraphicsContext*, const IntRect& damageRect);

    virtual void setFrameGeometry(const IntRect& rect);

    // All mouse handler functions below receive mouse events in window
    // coordinates.

    // NOTE: These may be called after we've been removed from the widget/
    // window hierarchy, for example because the EventHandler keeps a reference
    // around and tries to feed us MouseOut events.  In this case, doing
    // something would be not only pointless but dangerous, as without a
    // parent() we will end up failing an ASSERT().  So bail early if we get to
    // any of these with no parent().
    virtual bool handleMouseMoveEvent(const PlatformMouseEvent&);
    virtual bool handleMouseOutEvent(const PlatformMouseEvent&);
    virtual bool handleMousePressEvent(const PlatformMouseEvent&);
    virtual bool handleMouseReleaseEvent(const PlatformMouseEvent&);

    virtual IntRect windowClipRect() const;

    static void themeChanged();
    static int horizontalScrollbarHeight(ScrollbarControlSize size = RegularScrollbar);
    static int verticalScrollbarWidth(ScrollbarControlSize size = RegularScrollbar);

    // Scrolls the page when auto-repeat scrolling.
    void autoscrollTimerFired(Timer<PlatformScrollbar>*);

    // This function receives events in window coordinates.
    void handleMouseMoveEventWhenCapturing(const PlatformMouseEvent& e);

protected:
    virtual void updateThumbPosition();
    virtual void updateThumbProportion();

private:
    PlatformScrollbar(ScrollbarClient*, ScrollbarOrientation, ScrollbarControlSize);

    // Scroll bar segment identifiers
    enum Segment {
        Arrow1 = 0,
        Track,        // Only used when scrollbar does not contain a thumb
        BeforeThumb,  // -|
        Thumb,        // -+ Only used when scrollbar contains a thumb
        AfterThumb,   // -|
        Arrow2,
        None,
        NumSegments = None
    };

    // Turns on/off whether we have mouse capture.  This is only for tracking,
    // as the EventHandler is what controls the actual capture.
    void setCapturingMouse(bool capturing);

    // Returns the girth of the scrollbar arrow button for layout, given the
    // system metrics code for the desired direction's button and a limiting
    // height (for vertical scroll bars) or width (for horizontal scrollbars).
    // Also computes the background span (available remaining space).
    int scrollButtonGirth(int systemMetricsCode, int limit,
                          int* backgroundSpan);

    // Returns the girth of the scrollbar thumb for layout, given the system
    // metrics code for the desired direction's thumb and the background span
    // (space remaining after the buttons are drawn).
    int scrollThumbGirth(int systemMetricsCode, int backgroundSpan);

    // Computes the layout of the scroll bar given its current configuration.
    void layout();

    // Sets the current mouse position to the coordinates in |event|.
    void updateMousePosition(int x, int y);

    // Helper routine for updateMousePosition(), used to bypass layout().
    void updateMousePositionInternal();

    // Returns the correct state for the theme engine to draw a segment.
    int getThemeState(Segment target) const;
    int getThemeArrowState(Segment target) const;
    int getClassicThemeState(Segment target) const;

    // Draws the tick-marks on the scrollbar. The tick-marks are visual
    // indicators showing the results from a find-in-page operation.
    void DrawTickmarks(GraphicsContext* context) const;

    IntPoint m_lastNativePos;   // The last (native) mouse coordinate received.
    struct {
        int thumbPos;           // Relevant (window) mouse coordinate, and...
        int scrollVal;          // ...current scrollvalue, when...
    } m_dragOrigin;             // ...user begins dragging the thumb.
    IntRect m_segmentRects[NumSegments];
                                // The native coordinates of the scrollbar
                                // segments.
    Segment m_mouseOver;        // The scrollbar segment the mouse is over.
    Segment m_captureStart;     // The segment on which we started capture.
    Timer<PlatformScrollbar> m_autorepeatTimer;
                                // Timer to start and continue auto-repeat
                                // scrolling when the button is held down.
    bool m_enabled;             // True when the scrollbar is enabled.
    bool m_needsLayout;         // True when cached geometry may have changed

    // Multipliers against scrollbar thickness that determine how far away from
    // the scrollbar track the cursor can go before the thumb "snaps back"
    static const int kOffSideMultiplier;
    static const int kOffEndMultiplier;

    // Auto-repeat delays, in seconds
    static const double kAutorepeatInitialDelay;
    static const double kAutorepeatRepeatInterval;
};

}

#endif // PlatformScrollbar_h

