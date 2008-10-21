// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

#include "config.h"
#include <algorithm>
#include <windows.h>
#include <vsstyle.h>
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NativeImageSkia.h"
#include "PlatformMouseEvent.h"
#include "PlatformScrollBar.h"
#include "Range.h"
#include "ScrollView.h"
#include "WidgetClientChromium.h"

#include "graphics/SkiaUtils.h"

#undef LOG
#include "base/gfx/native_theme.h"
#include "base/gfx/platform_canvas_win.h"
#include "base/gfx/skia_utils.h"
#include "base/win_util.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webkit_glue.h"

namespace WebCore {

const int PlatformScrollbar::kOffSideMultiplier = 8;
const int PlatformScrollbar::kOffEndMultiplier = 3;
const double PlatformScrollbar::kAutorepeatInitialDelay = 0.4;
const double PlatformScrollbar::kAutorepeatRepeatInterval = 1. / 15.;

// The scrollbar size in DumpRenderTree on the Mac - so we can match their
// layout results.  Entries are for regular, small, and mini scrollbars.
// Metrics obtained using [NSScroller scrollerWidthForControlSize:]
static const int kMacScrollbarSize[3] = {15, 11, 15};

// Scrollbar button and thumb sizes, for consistent layout results.  The Mac
// value is not readily available, but it's not really needed, since these
// metrics only affect drawing within the scrollbar itself.  These are the
// standard Windows values without Large Fonts.
static const int kLayoutTestScrollbarButtonGirth = 17;
static const int kLayoutTestScrollbarThumbGirth = 17;


/*static*/ void PlatformScrollbar::themeChanged()
{
    // TODO(darin): implement this
}

/*static*/ int PlatformScrollbar::horizontalScrollbarHeight(
    ScrollbarControlSize controlSize)
{
    return webkit_glue::IsLayoutTestMode() ? kMacScrollbarSize[controlSize] :
        GetSystemMetrics(SM_CYHSCROLL);
}

/*static*/ int PlatformScrollbar::verticalScrollbarWidth(
    ScrollbarControlSize controlSize)
{
    return webkit_glue::IsLayoutTestMode() ? kMacScrollbarSize[controlSize] :
        GetSystemMetrics(SM_CXVSCROLL);
}

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client,
                                     ScrollbarOrientation orientation,
                                     ScrollbarControlSize controlSize)
    : Scrollbar(client, orientation, controlSize)
    , m_lastNativePos(-1, -1)  // initialize to bogus values
    , m_mouseOver(None)
    , m_captureStart(None)
#pragma warning(suppress: 4355) // it's okay to pass |this| here!
    , m_autorepeatTimer(this, &PlatformScrollbar::autoscrollTimerFired)
    , m_enabled(true)
    , m_needsLayout(true)
{
}

PlatformScrollbar::~PlatformScrollbar()
{
}

int PlatformScrollbar::width() const
{
    return orientation() == VerticalScrollbar ?
        verticalScrollbarWidth(controlSize()) : Widget::width();
}

int PlatformScrollbar::height() const
{
    return orientation() == HorizontalScrollbar ?
        horizontalScrollbarHeight(controlSize()) : Widget::height();
}

void PlatformScrollbar::setRect(const IntRect& rect)
{
    setFrameGeometry(rect);
}

void PlatformScrollbar::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    invalidate();
}

void PlatformScrollbar::DrawTickmarks(GraphicsContext* context) const
{
    // We don't draw on the horizontal scrollbar. It is too confusing
    // to have the tickmarks appear on both scrollbars.
    const bool horz = orientation() == HorizontalScrollbar;
    if (horz)
      return;

    // We need to as the WidgetClientChromium for the bitmap to use to draw.
    WidgetClientChromium* widget_client = static_cast<WidgetClientChromium*>(
        WebCore::Widget::client());
    if (!widget_client)
      return;  // Cannot draw without access to the bitmap.

    // Get the frame view this scroll bar belongs to.
    FrameView* view = reinterpret_cast<FrameView*>(parent());
    ASSERT(view);

    // A frame can be null if this function is called for the scroll views
    // used when drawing drop-down boxes. We don't need to draw anything in
    // such cases.
    if (!view->frame())
      return;

    // Find out if the frame has any tickmarks.
    const Vector<RefPtr<Range> >& tickmarks =
        WebFrameImpl::FromFrame(view->frame())->tickmarks();
    if (tickmarks.isEmpty())
        return;

    RECT track_area;
    if (m_segmentRects[Track].x() != -1) {
      // Scroll bar is too small to draw a thumb.
      track_area.left   = m_segmentRects[Track].x();
      track_area.top    = m_segmentRects[Track].y();
      track_area.right  = m_segmentRects[Track].right() - 1;
      track_area.bottom = m_segmentRects[Track].bottom() - 1;
    } else {
      // Find the area between the arrows of the scroll bar.
      track_area.left   = m_segmentRects[BeforeThumb].x();
      track_area.top    = m_segmentRects[BeforeThumb].y();
      track_area.right  = m_segmentRects[AfterThumb].right() - 1;
      track_area.bottom = m_segmentRects[AfterThumb].bottom() - 1;
    }

    // We now can figure out the actual height and width of the track.
    const int track_height = track_area.bottom - track_area.top;
    const int track_width = track_area.right - track_area.left;

    if (track_height <= 0 || track_width <= 0)
      return;  // nothing to draw on.

    // NOTE: We tolerate the platformContext() call here because the scrollbars
    // will not be serialized, i.e. composition is done in the renderer and
    // never in the browser.
    // Prepare the bitmap for drawing the tickmarks on the scroll bar.
    gfx::PlatformCanvas* canvas = context->platformContext()->canvas();

    // Load the image for the tickmark.
    static RefPtr<Image> dashImg = Image::loadPlatformResource("tickmarkDash");
    DCHECK(dashImg);
    if (dashImg->isNull()) {
        ASSERT_NOT_REACHED();
        return;
    }
    const NativeImageSkia* dash = dashImg->nativeImageForCurrentFrame();

    for (Vector<RefPtr<Range> >::const_iterator i =
            tickmarks.begin();
            i != tickmarks.end(); ++i) {
        const RefPtr<Range> range = (*i);

        if (!WebFrameImpl::RangeShouldBeHighlighted(range.get()))
            continue;

        const IntRect& bounds = range->boundingBox();

        // Calculate how far down (in %) the tick-mark should appear.
        const float percent = static_cast<float>(bounds.y()) / m_totalSize;

        // Calculate how far down (in pixels) the tick-mark should appear.
        const int y_pos = track_area.top + (track_height * percent);

        // Draw the tick-mark as a rounded rect with a slightly curved edge.
        canvas->drawBitmap(*dash, track_area.left, y_pos);
    }
}

// paint in the coordinate space of our parent's content area
void PlatformScrollbar::paint(GraphicsContext* gc, const IntRect& damageRect)
{
    if (gc->paintingDisabled())
        return;

    // Don't paint anything if the scrollbar doesn't intersect the damage rect.
    if (!frameGeometry().intersects(damageRect))
        return;

    gc->save();
    gc->translate(x(), y());

    layout();

    HDC hdc = gc->platformContext()->canvas()->beginPlatformPaint();
    const bool horz = orientation() == HorizontalScrollbar;
    const PlatformContextSkia* const skia = gc->platformContext();
    const gfx::NativeTheme* const nativeTheme = skia->nativeTheme();
    gfx::PlatformCanvasWin* const canvas = skia->canvas();
    
    // Draw the up/left arrow of the scroll bar.
    RECT rect = gfx::SkIRectToRECT(m_segmentRects[Arrow1]);
    nativeTheme->PaintScrollbarArrow(hdc, getThemeArrowState(Arrow1),
                                     (horz ? DFCS_SCROLLLEFT : DFCS_SCROLLUP) |
                                         getClassicThemeState(Arrow1),
                                     &rect);

    if (m_segmentRects[Track].x() != -1) {
        // The scroll bar is too small to draw the thumb. Just draw a
        // single track between the arrows.
        rect = gfx::SkIRectToRECT(m_segmentRects[Track]);
        nativeTheme->PaintScrollbarTrack(hdc,
                                         horz ? SBP_UPPERTRACKHORZ :
                                             SBP_UPPERTRACKVERT,
                                         getThemeState(Track),
                                         getClassicThemeState(Track),
                                         &rect, &rect, canvas);
        DrawTickmarks(gc);
    } else {
        // Draw the track area before the thumb on the scroll bar.
        rect = gfx::SkIRectToRECT(m_segmentRects[BeforeThumb]);
        nativeTheme->PaintScrollbarTrack(hdc,
                                         horz ? SBP_UPPERTRACKHORZ :
                                             SBP_UPPERTRACKVERT,
                                         getThemeState(BeforeThumb),
                                         getClassicThemeState(BeforeThumb),
                                         &rect, &rect, canvas);
              
        // Draw the track area after the thumb on the scroll bar.
        rect = gfx::SkIRectToRECT(m_segmentRects[BeforeThumb]);
        RECT rect_after = gfx::SkIRectToRECT(m_segmentRects[AfterThumb]);
        nativeTheme->PaintScrollbarTrack(hdc,
                                         horz ? SBP_LOWERTRACKHORZ :
                                             SBP_LOWERTRACKVERT,
                                         getThemeState(AfterThumb),
                                         getClassicThemeState(AfterThumb),
                                         &rect_after,
                                         &rect,
                                         canvas);
         
        // Draw the tick-marks on the scroll bar, if any tick-marks
        // exist. Note: The thumb will be drawn on top of the tick-marks,
        // which is desired.
        DrawTickmarks(gc);

        // Draw the thumb (the box you drag in the scroll bar to scroll).
        rect = gfx::SkIRectToRECT(m_segmentRects[Thumb]);
        nativeTheme->PaintScrollbarThumb(hdc,
                                         horz ? SBP_THUMBBTNHORZ :
                                             SBP_THUMBBTNVERT,
                                         getThemeState(Thumb),
                                         getClassicThemeState(Thumb),
                                         &rect);
         
        // Draw the gripper (the three little lines on the thumb).
        rect = gfx::SkIRectToRECT(m_segmentRects[Thumb]);
        nativeTheme->PaintScrollbarThumb(hdc,
                                         horz ? SBP_GRIPPERHORZ :
                                             SBP_GRIPPERVERT,
                                         getThemeState(Thumb),
                                         getClassicThemeState(Thumb),
                                         &rect);
    }

    // Draw the down/right arrow of the scroll bar.
    rect = gfx::SkIRectToRECT(m_segmentRects[Arrow2]);
    nativeTheme->PaintScrollbarArrow(hdc, getThemeArrowState(Arrow2),
                                     (horz ?
                                         DFCS_SCROLLRIGHT : DFCS_SCROLLDOWN) |
                                         getClassicThemeState(Arrow2),
                                     &rect);
    gc->platformContext()->canvas()->endPlatformPaint();

    gc->restore();
}

void PlatformScrollbar::setFrameGeometry(const IntRect& rect)
{
    if (rect == frameGeometry())
        return;

    Widget::setFrameGeometry(rect);
    m_needsLayout = true;
    // NOTE: we assume that our caller will invalidate us
}

bool PlatformScrollbar::handleMouseMoveEvent(const PlatformMouseEvent& e)
{
    if (!parent())
        return true;

    if (m_captureStart != None) {
        handleMouseMoveEventWhenCapturing(e);
        return true;
    }

    IntPoint pos = convertFromContainingWindow(e.pos());
    updateMousePosition(pos.x(), pos.y());

    // FIXME: Invalidate only the portions that actually changed
    invalidate();
    return true;
}

bool PlatformScrollbar::handleMouseOutEvent(const PlatformMouseEvent& e)
{
    if (!parent())
        return true;

    ASSERT(m_captureStart == None);

    // Pass bogus values that will never match real mouse coords.
    updateMousePosition(-1, -1);

    // FIXME: Invalidate only the portions that actually changed
    invalidate();
    return true;
}

bool PlatformScrollbar::handleMouseReleaseEvent(const PlatformMouseEvent& e)
{
    ScrollView* parentView = parent();
    if (!parentView)
        return true;

    IntPoint pos = convertFromContainingWindow(e.pos());
    updateMousePosition(pos.x(), pos.y());

    setCapturingMouse(false);

    // FIXME: Invalidate only the portions that actually changed
    invalidate();
    return true;
}

bool PlatformScrollbar::handleMousePressEvent(const PlatformMouseEvent& e)
{
    if (!parent())
        return true;

    // TODO(pkasting): http://b/583875 Right-click should invoke a context menu
    // (maybe this would be better handled elsewhere?)
    if (!m_enabled || (e.button() != LeftButton)) {
        return true;
    }

    ASSERT(m_captureStart == None);

    IntPoint pos = convertFromContainingWindow(e.pos());

    const bool horz = (orientation() == HorizontalScrollbar);
    updateMousePosition(pos.x(), pos.y());
    switch (m_mouseOver) {
    case Arrow1:
        scroll(horz ? ScrollLeft : ScrollUp, ScrollByLine);
        break;
    case Track:
        return true;
    case BeforeThumb:
        scroll(horz ? ScrollLeft : ScrollUp, ScrollByPage);
        break;
    case Thumb:
        m_dragOrigin.thumbPos = horz ? pos.x() : pos.y();
        m_dragOrigin.scrollVal = value();
        break;
    case AfterThumb:
        scroll(horz ? ScrollRight : ScrollDown, ScrollByPage);
        break;
    case Arrow2:
        scroll(horz ? ScrollRight : ScrollDown, ScrollByLine);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    setCapturingMouse(true);

    // Kick off auto-repeat timer
    if (m_mouseOver != Thumb)
        m_autorepeatTimer.start(kAutorepeatInitialDelay,
                                kAutorepeatRepeatInterval);

    m_needsLayout = true;
    // FIXME: Invalidate only the portions that actually changed
    invalidate();

    return true;
}

void PlatformScrollbar::handleMouseMoveEventWhenCapturing(const PlatformMouseEvent& e)
{
    IntPoint pos = convertFromContainingWindow(e.pos());
    updateMousePosition(pos.x(), pos.y());

    if (m_captureStart != Thumb) {
        // FIXME: Invalidate only the portions that actually changed
        invalidate();
        return;
    }

    int xCancelDistance, yCancelDistance, backgroundSpan, thumbGirth, delta;
    // NOTE: The cancel distance calculations are based on the behavior of the
    // MSVC8 main window scrollbar + some guessing/extrapolation
    if (orientation() == HorizontalScrollbar) {
        xCancelDistance = kOffEndMultiplier * horizontalScrollbarHeight();
        yCancelDistance = kOffSideMultiplier * horizontalScrollbarHeight();
        backgroundSpan = m_segmentRects[AfterThumb].right() -
            m_segmentRects[BeforeThumb].x();
        thumbGirth = m_segmentRects[Thumb].right() - m_segmentRects[Thumb].x();
        delta = pos.x() - m_dragOrigin.thumbPos;
    } else {
        xCancelDistance = kOffSideMultiplier * verticalScrollbarWidth();
        yCancelDistance = kOffEndMultiplier * verticalScrollbarWidth();
        backgroundSpan = m_segmentRects[AfterThumb].bottom() -
            m_segmentRects[BeforeThumb].y();
        thumbGirth = m_segmentRects[Thumb].bottom() - m_segmentRects[Thumb].y();
        delta = pos.y() - m_dragOrigin.thumbPos;
    }

    // Snap scrollbar back to drag origin if mouse gets too far away
    if ((m_lastNativePos.x() <
            (m_segmentRects[BeforeThumb].x() - xCancelDistance)) ||
        (m_lastNativePos.x() >
            (m_segmentRects[AfterThumb].right() + xCancelDistance)) ||
        (m_lastNativePos.y() <
            (m_segmentRects[BeforeThumb].y() - yCancelDistance)) ||
        (m_lastNativePos.y() >
            (m_segmentRects[AfterThumb].bottom() + yCancelDistance)))
        delta = 0;

    // Convert delta from pixel coords to scrollbar logical coords
    if (backgroundSpan > thumbGirth) {
        if (setValue(m_dragOrigin.scrollVal + (delta *
            (m_totalSize - m_visibleSize) / (backgroundSpan - thumbGirth)))) {
            m_needsLayout = true;
            // FIXME: Invalidate only the portions that actually changed
            invalidate();
        }
    }
}

IntRect PlatformScrollbar::windowClipRect() const
{
    if (m_client)
        return m_client->windowClipRect();

    return convertToContainingWindow(IntRect(0, 0, width(), height()));
}

void PlatformScrollbar::updateThumbPosition()
{
    m_needsLayout = true;
    // FIXME: Invalidate only the portions that actually changed
    invalidate();
}

void PlatformScrollbar::updateThumbProportion()
{
    // RenderLayer::updateScrollInfoAfterLayout changes the enabled state when
    // the style is OSCROLL, however it doesn't change it when the style is OAUTO.
    // As a workaround we enable the scrollbar if the visible size is less than
    // the total size
    if (!m_enabled && m_visibleSize < m_totalSize) {
        setEnabled(true);
    }

    // If the thumb was at the end of the track and the scrollbar was resized
    // smaller, we need to cap the value to the new maximum.
    if (setValue(value()))
        return; // updateThumbPosition() already invalidated as needed

    m_needsLayout = true;
    // FIXME: Invalidate only the portions that actually changed
    invalidate();
}

void PlatformScrollbar::autoscrollTimerFired(Timer<PlatformScrollbar>*)
{
    ASSERT((m_captureStart != None) && (m_mouseOver == m_captureStart));

    const bool horz = (orientation() == HorizontalScrollbar);
    switch (m_captureStart) {
    case Arrow1:
        scroll(horz ? ScrollLeft : ScrollUp, ScrollByLine);
        break;
    case BeforeThumb:
        scroll(horz ? ScrollLeft : ScrollUp, ScrollByPage);
        break;
    case AfterThumb:
        scroll(horz ? ScrollRight : ScrollDown, ScrollByPage);
        break;
    case Arrow2:
        scroll(horz ? ScrollRight : ScrollDown, ScrollByLine);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void PlatformScrollbar::setCapturingMouse(bool capturing)
{
    if (capturing) {
        m_captureStart = m_mouseOver;
    } else {
        m_captureStart = None;
        m_autorepeatTimer.stop();
    }
}

int PlatformScrollbar::scrollButtonGirth(int systemMetricsCode, int limit,
                                         int* backgroundSpan)
{
    const int girth = webkit_glue::IsLayoutTestMode() ? 
        kLayoutTestScrollbarButtonGirth : GetSystemMetrics(systemMetricsCode);
    *backgroundSpan = limit - 2 * girth;
    if (*backgroundSpan < 0) {
        *backgroundSpan = 0;
        return limit / 2;
    }
    return girth;
}

int PlatformScrollbar::scrollThumbGirth(int systemMetricsCode,
                                        int backgroundSpan)
{
    const int minimumGirth = webkit_glue::IsLayoutTestMode() ? 
        kLayoutTestScrollbarThumbGirth : GetSystemMetrics(systemMetricsCode);
    return std::max<int>(m_visibleSize * backgroundSpan / m_totalSize,
                         minimumGirth);
}

void PlatformScrollbar::layout()
{
    if (!m_needsLayout)
        return;
    m_needsLayout = false;

    const IntRect invalid(-1, -1, 0, 0);
    if (m_totalSize <= 0) {
        for (int i = 0; i < NumSegments; ++i)
            m_segmentRects[i] = invalid;
        return;
    }

    int buttonGirth, backgroundSpan, thumbGirth;
    RECT box = {0};
    LONG* changingCoord1, * changingCoord2;
    // For both orientations, we allow the buttonGirth to determine the
    // backgroundSpan directly, to avoid rounding errors.
    if (orientation() == HorizontalScrollbar) {
        buttonGirth = scrollButtonGirth(SM_CXHSCROLL, width(), &backgroundSpan);
        thumbGirth = scrollThumbGirth(SM_CXHTHUMB, backgroundSpan);
        box.bottom += horizontalScrollbarHeight();
        changingCoord1 = &box.left;
        changingCoord2 = &box.right;
    } else {
        buttonGirth = scrollButtonGirth(SM_CYVSCROLL, height(),
                                        &backgroundSpan);
        thumbGirth = scrollThumbGirth(SM_CYVTHUMB, backgroundSpan);
        box.right += verticalScrollbarWidth();
        changingCoord1 = &box.top;
        changingCoord2 = &box.bottom;
    }

    // Scrollbar:       |<|--------|XXX|------|>|
    // Start arrow:     |<|
    *changingCoord2 += buttonGirth;
    m_segmentRects[Arrow1] = gfx::RECTToSkIRect(box);

    if (thumbGirth >= backgroundSpan) {
        if (backgroundSpan == 0) {
            m_segmentRects[Track] = invalid;
        } else {
            // Track:     |-------------------|
            *changingCoord1 = *changingCoord2;
            *changingCoord2 += backgroundSpan;
            m_segmentRects[Track] = gfx::RECTToSkIRect(box);
        }

        m_segmentRects[BeforeThumb] = invalid;
        m_segmentRects[Thumb] = invalid;
        m_segmentRects[AfterThumb] = invalid;
    } else {
        m_segmentRects[Track] = invalid;

        const int thumbOffset = (m_totalSize <= m_visibleSize) ? 0 : (value() *
            (backgroundSpan - thumbGirth) / (m_totalSize - m_visibleSize));
        // Before thumb:  |--------|
        *changingCoord1 = *changingCoord2;
        *changingCoord2 += thumbOffset;
        m_segmentRects[BeforeThumb] = gfx::RECTToSkIRect(box);

        // Thumb:                  |XXX|
        *changingCoord1 = *changingCoord2;
        *changingCoord2 += thumbGirth;
        m_segmentRects[Thumb] = gfx::RECTToSkIRect(box);

        // After thumb:                |------|
        *changingCoord1 = *changingCoord2;
        *changingCoord2 += backgroundSpan - (thumbOffset + thumbGirth);
        m_segmentRects[AfterThumb] = gfx::RECTToSkIRect(box);
    }

    // End arrow:                             |>|
    *changingCoord1 = *changingCoord2;
    *changingCoord2 += buttonGirth;
    m_segmentRects[Arrow2] = gfx::RECTToSkIRect(box);

    // Changed layout, so need to update m_mouseOver and m_autorepeatTimer
    updateMousePositionInternal();

    // DO NOT invalidate() here.  We already invalidate()d for this layout when
    // setting m_needsLayout = true; by the time we reach this point, we're
    // called by paint(), so invalidate() is not only unnecessary but will
    // waste effort.
}

void PlatformScrollbar::updateMousePosition(int x, int y)
{
    m_lastNativePos.setX(x);
    m_lastNativePos.setY(y);

    if (m_needsLayout)
        layout();   // Calls updateMousePositionInternal()
    else
        updateMousePositionInternal();
}

void PlatformScrollbar::updateMousePositionInternal()
{
    m_mouseOver = None;
    for (int i = 0; i < NumSegments; ++i) {
        if ((m_lastNativePos.x() >= m_segmentRects[i].x()) &&
            (m_lastNativePos.x() < m_segmentRects[i].right()) &&
            (m_lastNativePos.y() >= m_segmentRects[i].y()) &&
            (m_lastNativePos.y() < m_segmentRects[i].bottom())) {
            m_mouseOver = static_cast<Segment>(i);
            break;
        }
    }

    // Start/stop autorepeat timer if capturing something other than the thumb.
    if ((m_captureStart != None) && (m_captureStart != Thumb)) {
        if (m_mouseOver != m_captureStart)
            m_autorepeatTimer.stop();   // Safe to call when already stopped
        else if (!m_autorepeatTimer.isActive())
            m_autorepeatTimer.startRepeating(kAutorepeatRepeatInterval);
    }
}

int PlatformScrollbar::getThemeState(Segment target) const
{
    // When dragging the thumb, draw thumb pressed and other segments normal
    // regardless of where the cursor actually is.  See also four places in
    // getThemeArrowState().
    if (m_captureStart == Thumb) {
        if (target == Thumb)
            return SCRBS_PRESSED;
        return (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) ?
            SCRBS_NORMAL : SCRBS_HOVER;
    }
    if (!m_enabled)
        return SCRBS_DISABLED;
    if ((m_mouseOver != target) || (target == Track)) {
        return ((m_mouseOver == None) ||
                (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)) ?
            SCRBS_NORMAL : SCRBS_HOVER;
    }
    if (m_captureStart == None)
        return SCRBS_HOT;
    return (m_captureStart == target) ? SCRBS_PRESSED : SCRBS_NORMAL;
}

int PlatformScrollbar::getThemeArrowState(Segment target) const
{
    // We could take advantage of knowing the values in the state enum to write
    // some simpler code, but treating the state enum as a black box seems
    // clearer and more future-proof.
    if (target == Arrow1) {
        if (orientation() == HorizontalScrollbar) {
            if (m_captureStart == Thumb) {
                return
                    (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) ?
                    ABS_LEFTNORMAL : ABS_LEFTHOVER;
            }
            if (!m_enabled)
                return ABS_LEFTDISABLED;
            if (m_mouseOver != target) {
                return ((m_mouseOver == None) ||
                    (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)) ?
                    ABS_LEFTNORMAL : ABS_LEFTHOVER;
            }
            if (m_captureStart == None)
                return ABS_LEFTHOT;
            return (m_captureStart == target) ?
                ABS_LEFTPRESSED : ABS_LEFTNORMAL;
        }
        if (m_captureStart == Thumb) {
            return (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) ?
                ABS_UPNORMAL : ABS_UPHOVER;
        }
        if (!m_enabled)
            return ABS_UPDISABLED;
        if (m_mouseOver != target) {
            return ((m_mouseOver == None) ||
                    (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)) ?
                ABS_UPNORMAL : ABS_UPHOVER;
        }
        if (m_captureStart == None)
            return ABS_UPHOT;
        return (m_captureStart == target) ? ABS_UPPRESSED : ABS_UPNORMAL;
    }
    if (orientation() == HorizontalScrollbar) {
        if (m_captureStart == Thumb) {
            return (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) ?
                ABS_RIGHTNORMAL : ABS_RIGHTHOVER;
        }
        if (!m_enabled)
            return ABS_RIGHTDISABLED;
        if (m_mouseOver != target) {
            return ((m_mouseOver == None) ||
                    (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)) ?
                ABS_RIGHTNORMAL : ABS_RIGHTHOVER;
        }
        if (m_captureStart == None)
            return ABS_RIGHTHOT;
        return (m_captureStart == target) ? ABS_RIGHTPRESSED : ABS_RIGHTNORMAL;
    }
    if (m_captureStart == Thumb) {
        return (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) ?
            ABS_DOWNNORMAL : ABS_DOWNHOVER;
    }
    if (!m_enabled)
        return ABS_DOWNDISABLED;
    if (m_mouseOver != target) {
        return ((m_mouseOver == None) ||
                (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)) ?
            ABS_DOWNNORMAL : ABS_DOWNHOVER;
    }
    if (m_captureStart == None)
        return ABS_DOWNHOT;
    return (m_captureStart == target) ? ABS_DOWNPRESSED : ABS_DOWNNORMAL;
}

int PlatformScrollbar::getClassicThemeState(Segment target) const
{
    // When dragging the thumb, draw the buttons normal even when hovered.
    if (m_captureStart == Thumb)
        return 0;
    if (!m_enabled)
        return DFCS_INACTIVE;
    if ((m_mouseOver != target) || (target == Track))
        return 0;
    if (m_captureStart == None)
        return DFCS_HOT;
    return (m_captureStart == target) ? (DFCS_PUSHED | DFCS_FLAT) : 0;
}

}  // namespace WebCore
