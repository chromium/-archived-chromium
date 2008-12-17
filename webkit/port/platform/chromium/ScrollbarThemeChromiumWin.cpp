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
#include "ScrollbarThemeChromium.h"

#include <windows.h>
#include <vsstyle.h>

#include "ChromiumBridge.h"
#include "GraphicsContext.h"
#include "PlatformContextSkia.h"
#include "PlatformMouseEvent.h"
#include "Scrollbar.h"

#include "base/gfx/native_theme.h"
#include "base/win_util.h"

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

int ScrollbarThemeChromium::scrollbarThickness(ScrollbarControlSize controlSize)
{
    static int thickness;
    if (!thickness) {
        if (ChromiumBridge::layoutTestMode())
            return kMacScrollbarSize[controlSize];
        thickness = GetSystemMetrics(SM_CXVSCROLL);
    }
    return thickness;
}

bool ScrollbarThemeChromium::invalidateOnMouseEnterExit()
{
    return runningVista();
}

void ScrollbarThemeChromium::paintTrackPiece(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect, ScrollbarPart partType)
{
    bool horz = scrollbar->orientation() == HorizontalScrollbar;

    skia::PlatformCanvasWin* canvas = gc->platformContext()->canvas();
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

void ScrollbarThemeChromium::paintButton(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect, ScrollbarPart part)
{
    bool horz = scrollbar->orientation() == HorizontalScrollbar;

    skia::PlatformCanvasWin* canvas = gc->platformContext()->canvas();
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

void ScrollbarThemeChromium::paintThumb(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect)
{
    bool horz = scrollbar->orientation() == HorizontalScrollbar;

    skia::PlatformCanvasWin* canvas = gc->platformContext()->canvas();
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

int ScrollbarThemeChromium::getThemeState(Scrollbar* scrollbar, ScrollbarPart part) const
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

int ScrollbarThemeChromium::getThemeArrowState(Scrollbar* scrollbar, ScrollbarPart part) const
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

int ScrollbarThemeChromium::getClassicThemeState(Scrollbar* scrollbar, ScrollbarPart part) const
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
