/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "Cursor.h"
#include "Image.h"
#include "IntPoint.h"
#include "NativeImageSkia.h"
#include "NotImplemented.h"

#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_resources.h"
#include "webkit/glue/webkit_glue.h"

#define ALPHA_CURSORS

namespace WebCore {

Cursor::Cursor(const Cursor& other)
    : m_impl(other.m_impl)
{
}

Cursor::Cursor(Image* img, const IntPoint& hotspot)
{
    // If we don't have a valid bitmap, then fallback to the default
    // cursor (ARROW).
    NativeImageSkia* bitmap = img->nativeImageForCurrentFrame();
    if (!bitmap)
        return;

    m_impl.set_type(WebCursor::CUSTOM);
    m_impl.set_hotspot(hotspot.x(), hotspot.y());
    m_impl.set_bitmap(*bitmap);
}

Cursor::~Cursor()
{
}

Cursor& Cursor::operator=(const Cursor& other)
{
    m_impl = other.m_impl;
    return *this;
}

Cursor::Cursor(PlatformCursor c)
    : m_impl(c)
{
}

const Cursor& pointerCursor()
{
    // The double-parenthesis here and elsewhere is a glorious hack to work
    // around a corner in C++ parsing.  Otherwise, code like
    //    Cursor c(PlatformCursor(...));
    // is parsed as a *function* declaration (since PlatformCursor is a type,
    // after all).
    static const Cursor c((PlatformCursor(WebCursor::ARROW)));
    return c;
}

const Cursor& crossCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::CROSS)));
    return c;
}

const Cursor& handCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::HAND)));
    return c;
}

const Cursor& iBeamCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::IBEAM)));
    return c;
}

const Cursor& waitCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::WAIT)));
    return c;
}

const Cursor& helpCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::HELP)));
    return c;
}

const Cursor& eastResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZEWE)));
    return c;
}

const Cursor& northResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZENS)));
    return c;
}

const Cursor& northEastResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZENESW)));
    return c;
}

const Cursor& northWestResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZENWSE)));
    return c;
}

const Cursor& southResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZENS)));
    return c;
}

const Cursor& southEastResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZENWSE)));
    return c;
}

const Cursor& southWestResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZENESW)));
    return c;
}

const Cursor& westResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZEWE)));
    return c;
}

const Cursor& northSouthResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZENS)));
    return c;
}

const Cursor& eastWestResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZEWE)));
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZENESW)));
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::SIZENWSE)));
    return c;
}

const Cursor& columnResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::COLRESIZE)));
    return c;
}

const Cursor& rowResizeCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::ROWRESIZE)));
    return c;
}

const Cursor& middlePanningCursor()
{
    SkBitmap* bitmap = webkit_glue::GetBitmapResource(IDC_PAN_MIDDLE);
    static const Cursor c((PlatformCursor(WebCursor(bitmap, 7, 7))));
    return c;
}

const Cursor& eastPanningCursor()
{
    SkBitmap* bitmap = webkit_glue::GetBitmapResource(IDC_PAN_EAST);
    static const Cursor c((PlatformCursor(WebCursor(bitmap, 7, 7))));
    return c;
}

const Cursor& northPanningCursor()
{
    SkBitmap* bitmap = webkit_glue::GetBitmapResource(IDC_PAN_NORTH);
    static const Cursor c((PlatformCursor(WebCursor(bitmap, 7, 7))));
    return c;
}

const Cursor& northEastPanningCursor()
{
    SkBitmap* bitmap = webkit_glue::GetBitmapResource(IDC_PAN_NORTH_EAST);
    static const Cursor c((PlatformCursor(WebCursor(bitmap, 7, 7))));
    return c;
}

const Cursor& northWestPanningCursor()
{
    SkBitmap* bitmap = webkit_glue::GetBitmapResource(IDC_PAN_NORTH_WEST);
    static const Cursor c((PlatformCursor(WebCursor(bitmap, 7, 7))));
    return c;
}

const Cursor& southPanningCursor()
{
    SkBitmap* bitmap = webkit_glue::GetBitmapResource(IDC_PAN_SOUTH);
    static const Cursor c((PlatformCursor(WebCursor(bitmap, 7, 7))));
    return c;
}

const Cursor& southEastPanningCursor()
{
    SkBitmap* bitmap = webkit_glue::GetBitmapResource(IDC_PAN_SOUTH_EAST);
    static const Cursor c((PlatformCursor(WebCursor(bitmap, 7, 7))));
    return c;
}

const Cursor& southWestPanningCursor()
{
    SkBitmap* bitmap = webkit_glue::GetBitmapResource(IDC_PAN_SOUTH_WEST);
    static const Cursor c((PlatformCursor(WebCursor(bitmap, 7, 7))));
    return c;
}

const Cursor& westPanningCursor()
{
    SkBitmap* bitmap = webkit_glue::GetBitmapResource(IDC_PAN_WEST);
    static const Cursor c((PlatformCursor(WebCursor(bitmap, 7, 7))));
    return c;
}

const Cursor& moveCursor() 
{
    static const Cursor c((PlatformCursor(WebCursor::SIZEALL)));
    return c;
}

const Cursor& verticalTextCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::VERTICALTEXT)));
    return c;
}

const Cursor& cellCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::CELL)));
    return c;
}

const Cursor& contextMenuCursor()
{
    return pointerCursor();
}

const Cursor& aliasCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::ALIAS)));
    return c;
}

const Cursor& progressCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::APPSTARTING)));
    return c;
}

const Cursor& noDropCursor()
{
    return notAllowedCursor();
}

const Cursor& copyCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::COPYCUR)));
    return c;
}

const Cursor& noneCursor()
{
    return pointerCursor();
}

const Cursor& notAllowedCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::NO)));
    return c;
}

const Cursor& zoomInCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::ZOOMIN)));
    return c;
}

const Cursor& zoomOutCursor()
{
    static const Cursor c((PlatformCursor(WebCursor::ZOOMOUT)));
    return c;
}

}
