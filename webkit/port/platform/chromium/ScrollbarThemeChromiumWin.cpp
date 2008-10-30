/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ScrollbarThemeChromiumWin.h"

#include <windows.h>
#include <vsstyle.h>

#include "GraphicsContext.h"
#include "PlatformContextSkia.h"
#include "PlatformMouseEvent.h"
#include "Scrollbar.h"

#include "base/gfx/native_theme.h"
#include "base/win_util.h"
#include "webkit/glue/webkit_glue.h"

namespace WebCore {

// The scrollbar size in DumpRenderTree on the Mac - so we can match their
// layout results.  Entries are for regular, small, and mini scrollbars.
// Metrics obtained using [NSScroller scrollerWidthForControlSize:]
static const int kMacScrollbarSize[3] = { 15, 11, 15 };

static bool runningVista()
{
    return win_util::GetWinVersion() >= win_util::WINVERSION_VISTA;
}

static RECT toRECT(const IntRect& input)
{
    RECT output;
    output.left = input.x();
    output.right = input.right();
    output.top = input.y();
    output.bottom = input.bottom();
    return output;
}

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeChromiumWin theme;
    return &theme;
}

ScrollbarThemeChromiumWin::ScrollbarThemeChromiumWin()
{
}

ScrollbarThemeChromiumWin::~ScrollbarThemeChromiumWin()
{
}

int ScrollbarThemeChromiumWin::scrollbarThickness(ScrollbarControlSize controlSize)
{
    static int thickness;
    if (!thickness) {
        if (webkit_glue::IsLayoutTestMode())
            return kMacScrollbarSize[controlSize];
        thickness = GetSystemMetrics(SM_CXVSCROLL);
    }
    return thickness;
}

void ScrollbarThemeChromiumWin::themeChanged()
{
}

bool ScrollbarThemeChromiumWin::invalidateOnMouseEnterExit()
{
    return runningVista();
}

bool ScrollbarThemeChromiumWin::hasThumb(Scrollbar* scrollbar)
{
    // This method is just called as a paint-time optimization to see if
    // painting the thumb can be skipped.  We don't have to be exact here.
    return thumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeChromiumWin::backButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool)
{
    // Windows just has single arrows.
    if (part == BackButtonEndPart)
        return IntRect();

    IntSize size = buttonSize(scrollbar);
    return IntRect(scrollbar->x(), scrollbar->y(), size.width(), size.height());
}

IntRect ScrollbarThemeChromiumWin::forwardButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool)
{
    // Windows just has single arrows.
    if (part == ForwardButtonStartPart)
        return IntRect();

    IntSize size = buttonSize(scrollbar);
    int x, y;
    if (scrollbar->orientation() == HorizontalScrollbar) {
        x = scrollbar->x() + scrollbar->width() - size.width();
        y = scrollbar->y();
    } else {
        x = scrollbar->x();
        y = scrollbar->y() + scrollbar->height() - size.height();
    }
    return IntRect(x, y, size.width(), size.height());
}

IntRect ScrollbarThemeChromiumWin::trackRect(Scrollbar* scrollbar, bool)
{
    IntSize bs = buttonSize(scrollbar);
    int thickness = scrollbarThickness();
    if (scrollbar->orientation() == HorizontalScrollbar) {
        if (scrollbar->width() < 2 * thickness)
            return IntRect();
        return IntRect(scrollbar->x() + bs.width(), scrollbar->y(), scrollbar->width() - 2 * bs.width(), thickness);
    }
    if (scrollbar->height() < 2 * thickness)
        return IntRect();
    return IntRect(scrollbar->x(), scrollbar->y() + bs.height(), thickness, scrollbar->height() - 2 * bs.height());
}

void ScrollbarThemeChromiumWin::paintTrackBackground(GraphicsContext* context, Scrollbar* scrollbar, const IntRect& rect)
{
    // Just assume a forward track part.  We only paint the track as a single piece when there is no thumb.
    if (!hasThumb(scrollbar))
        paintTrackPiece(context, scrollbar, rect, ForwardTrackPart);
}

void ScrollbarThemeChromiumWin::paintTrackPiece(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect, ScrollbarPart partType)
{
    bool horz = scrollbar->orientation() == HorizontalScrollbar;

    gfx::PlatformCanvasWin* canvas = gc->platformContext()->canvas();
    HDC hdc = canvas->beginPlatformPaint();

    RECT paintRect = toRECT(rect);

    int partId;
    if (partType == BackTrackPart) {
        partId = horz ? SBP_UPPERTRACKHORZ : SBP_UPPERTRACKVERT;
    } else {
        partId = horz ? SBP_LOWERTRACKHORZ : SBP_LOWERTRACKVERT;
    }

    RECT alignRect = toRECT(trackRect(scrollbar, false));

    // Draw the track area before/after the thumb on the scroll bar.
    gfx::NativeTheme::instance()->PaintScrollbarTrack(
        hdc,
        partId,
        getThemeState(scrollbar, partType),
        getClassicThemeState(scrollbar, partType),
        &paintRect,
        &alignRect,
        gc->platformContext()->canvas());

    canvas->endPlatformPaint();
}

void ScrollbarThemeChromiumWin::paintButton(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect, ScrollbarPart part)
{
    bool horz = scrollbar->orientation() == HorizontalScrollbar;

    gfx::PlatformCanvasWin* canvas = gc->platformContext()->canvas();
    HDC hdc = canvas->beginPlatformPaint();

    RECT paintRect = toRECT(rect);

    int partId;
    if (part == BackButtonStartPart || part == ForwardButtonStartPart) {
        partId = horz ? DFCS_SCROLLLEFT : DFCS_SCROLLUP;
    } else {
        partId = horz ? DFCS_SCROLLRIGHT : DFCS_SCROLLDOWN;
    }

    // Draw the thumb (the box you drag in the scroll bar to scroll).
    gfx::NativeTheme::instance()->PaintScrollbarArrow(
        hdc,
        getThemeArrowState(scrollbar, part),
        partId | getClassicThemeState(scrollbar, part),
        &paintRect);

    canvas->endPlatformPaint();
}

void ScrollbarThemeChromiumWin::paintThumb(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect)
{
    bool horz = scrollbar->orientation() == HorizontalScrollbar;

    gfx::PlatformCanvasWin* canvas = gc->platformContext()->canvas();
    HDC hdc = canvas->beginPlatformPaint();

    RECT paintRect = toRECT(rect);

    // Draw the thumb (the box you drag in the scroll bar to scroll).
    gfx::NativeTheme::instance()->PaintScrollbarThumb(
        hdc,
        horz ? SBP_THUMBBTNHORZ : SBP_THUMBBTNVERT,
        getThemeState(scrollbar, ThumbPart),
        getClassicThemeState(scrollbar, ThumbPart),
        &paintRect);

    // Draw the gripper (the three little lines on the thumb).
    gfx::NativeTheme::instance()->PaintScrollbarThumb(
        hdc,
        horz ? SBP_GRIPPERHORZ : SBP_GRIPPERVERT,
        getThemeState(scrollbar, ThumbPart),
        getClassicThemeState(scrollbar, ThumbPart),
        &paintRect);

    canvas->endPlatformPaint();
}

void ScrollbarThemeChromiumWin::paintScrollCorner(ScrollView* view, GraphicsContext* context, const IntRect& cornerRect)
{
    // ScrollbarThemeComposite::paintScrollCorner incorrectly assumes that the
    // ScrollView is a FrameView (see FramelessScrollView), so we cannot let
    // that code run.  For FrameView's this is correct since we don't do custom
    // scrollbar corner rendering, which ScrollbarThemeComposite supports.
    ScrollbarTheme::paintScrollCorner(view, context, cornerRect);
}

bool ScrollbarThemeChromiumWin::shouldCenterOnThumb(Scrollbar*, const PlatformMouseEvent& evt)
{
    return evt.shiftKey() && evt.button() == LeftButton;
}

IntSize ScrollbarThemeChromiumWin::buttonSize(Scrollbar* scrollbar)
{
    // Our desired rect is essentially thickness by thickness.
    
    // Our actual rect will shrink to half the available space when we have < 2
    // times thickness pixels left.  This allows the scrollbar to scale down
    // and function even at tiny sizes.
 
    // In layout test mode, we force the button "girth" (i.e., the length of
    // the button along the axis of the scrollbar) to be a fixed size.
    // FIXME: This is retarded!  scrollbarThickness is already fixed in layout
    // test mode so that should be enough to result in repeatable results, but
    // preserving this hack avoids having to rebaseline pixel tests.
    const int kLayoutTestModeGirth = 17;

    int thickness = scrollbarThickness();
    int girth = webkit_glue::IsLayoutTestMode() ? kLayoutTestModeGirth : thickness;
    if (scrollbar->orientation() == HorizontalScrollbar) {
        int width = scrollbar->width() < 2 * girth ? scrollbar->width() / 2 : girth;
        return IntSize(width, thickness);
    }
    
    int height = scrollbar->height() < 2 * girth ? scrollbar->height() / 2 : girth;
    return IntSize(thickness, height);
}

int ScrollbarThemeChromiumWin::getThemeState(Scrollbar* scrollbar, ScrollbarPart part) const
{
    // When dragging the thumb, draw thumb pressed and other segments normal
    // regardless of where the cursor actually is.  See also four places in
    // getThemeArrowState().
    if (scrollbar->pressedPart() == ThumbPart) {
        if (part == ThumbPart)
            return SCRBS_PRESSED;
        return runningVista() ? SCRBS_HOVER : SCRBS_NORMAL;
    }
    if (!scrollbar->enabled())
        return SCRBS_DISABLED;
    if (scrollbar->hoveredPart() != part || part == BackTrackPart || part == ForwardTrackPart)
        return (scrollbar->hoveredPart() == NoPart || !runningVista()) ? SCRBS_NORMAL : SCRBS_HOVER;
    if (scrollbar->pressedPart() == NoPart)
        return SCRBS_HOT;
    return (scrollbar->pressedPart() == part) ? SCRBS_PRESSED : SCRBS_NORMAL;
}

int ScrollbarThemeChromiumWin::getThemeArrowState(Scrollbar* scrollbar, ScrollbarPart part) const
{
    // We could take advantage of knowing the values in the state enum to write
    // some simpler code, but treating the state enum as a black box seems
    // clearer and more future-proof.
    if (part == BackButtonStartPart || part == ForwardButtonStartPart) {
        if (scrollbar->orientation() == HorizontalScrollbar) {
            if (scrollbar->pressedPart() == ThumbPart)
                return !runningVista() ? ABS_LEFTNORMAL : ABS_LEFTHOVER;
            if (!scrollbar->enabled())
                return ABS_LEFTDISABLED;
            if (scrollbar->hoveredPart() != part)
                return ((scrollbar->hoveredPart() == NoPart) || !runningVista()) ? ABS_LEFTNORMAL : ABS_LEFTHOVER;
            if (scrollbar->pressedPart() == NoPart)
                return ABS_LEFTHOT;
            return (scrollbar->pressedPart() == part) ?
                ABS_LEFTPRESSED : ABS_LEFTNORMAL;
        }
        if (scrollbar->pressedPart() == ThumbPart)
            return !runningVista() ? ABS_UPNORMAL : ABS_UPHOVER;
        if (!scrollbar->enabled())
            return ABS_UPDISABLED;
        if (scrollbar->hoveredPart() != part)
            return ((scrollbar->hoveredPart() == NoPart) || !runningVista()) ? ABS_UPNORMAL : ABS_UPHOVER;
        if (scrollbar->pressedPart() == NoPart)
            return ABS_UPHOT;
        return (scrollbar->pressedPart() == part) ? ABS_UPPRESSED : ABS_UPNORMAL;
    }
    if (scrollbar->orientation() == HorizontalScrollbar) {
        if (scrollbar->pressedPart() == ThumbPart)
            return !runningVista() ? ABS_RIGHTNORMAL : ABS_RIGHTHOVER;
        if (!scrollbar->enabled())
            return ABS_RIGHTDISABLED;
        if (scrollbar->hoveredPart() != part)
            return ((scrollbar->hoveredPart() == NoPart) || !runningVista()) ? ABS_RIGHTNORMAL : ABS_RIGHTHOVER;
        if (scrollbar->pressedPart() == NoPart)
            return ABS_RIGHTHOT;
        return (scrollbar->pressedPart() == part) ? ABS_RIGHTPRESSED : ABS_RIGHTNORMAL;
    }
    if (scrollbar->pressedPart() == ThumbPart)
        return !runningVista() ? ABS_DOWNNORMAL : ABS_DOWNHOVER;
    if (!scrollbar->enabled())
        return ABS_DOWNDISABLED;
    if (scrollbar->hoveredPart() != part)
        return ((scrollbar->hoveredPart() == NoPart) || !runningVista()) ? ABS_DOWNNORMAL : ABS_DOWNHOVER;
    if (scrollbar->pressedPart() == NoPart)
        return ABS_DOWNHOT;
    return (scrollbar->pressedPart() == part) ? ABS_DOWNPRESSED : ABS_DOWNNORMAL;
}

int ScrollbarThemeChromiumWin::getClassicThemeState(Scrollbar* scrollbar, ScrollbarPart part) const
{
    // When dragging the thumb, draw the buttons normal even when hovered.
    if (scrollbar->pressedPart() == ThumbPart)
        return 0;
    if (!scrollbar->enabled())
        return DFCS_INACTIVE;
    if (scrollbar->hoveredPart() != part || part == BackTrackPart || part == ForwardTrackPart)
        return 0;
    if (scrollbar->pressedPart() == NoPart)
        return DFCS_HOT;
    return (scrollbar->pressedPart() == part) ? (DFCS_PUSHED | DFCS_FLAT) : 0;
}

}
