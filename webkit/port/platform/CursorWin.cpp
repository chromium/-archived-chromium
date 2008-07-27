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
#include "webkit/glue/webkit_resources.h"

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
  NativeImageSkia* bitmap = img->getBitmap();
  if (bitmap) {
    m_impl.set_type(WebCursor::CUSTOM);
    m_impl.set_hotspot(hotspot.x(), hotspot.y());
    m_impl.set_bitmap(*bitmap);
  }
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
    static Cursor c = WebCursor::ARROW;
    return c;
}

const Cursor& crossCursor()
{
    static Cursor c = WebCursor::CROSS;
    return c;
}

const Cursor& handCursor()
{
    static Cursor c = WebCursor::HAND;
    return c;
}

const Cursor& iBeamCursor()
{
    static Cursor c = WebCursor::IBEAM;
    return c;
}

const Cursor& waitCursor()
{
    static Cursor c = WebCursor::WAIT;
    return c;
}

const Cursor& helpCursor()
{
    static Cursor c = WebCursor::HELP;
    return c;
}

const Cursor& eastResizeCursor()
{
    static Cursor c = WebCursor::SIZEWE;
    return c;
}

const Cursor& northResizeCursor()
{
    static Cursor c = WebCursor::SIZENS;
    return c;
}

const Cursor& northEastResizeCursor()
{
    static Cursor c = WebCursor::SIZENESW;
    return c;
}

const Cursor& northWestResizeCursor()
{
    static Cursor c = WebCursor::SIZENWSE;
    return c;
}

const Cursor& southResizeCursor()
{
    static Cursor c = WebCursor::SIZENS;
    return c;
}

const Cursor& southEastResizeCursor()
{
    static Cursor c = WebCursor::SIZENWSE;
    return c;
}

const Cursor& southWestResizeCursor()
{
    static Cursor c = WebCursor::SIZENESW;
    return c;
}

const Cursor& westResizeCursor()
{
    static Cursor c = WebCursor::SIZEWE;
    return c;
}

const Cursor& northSouthResizeCursor()
{
    static Cursor c = WebCursor::SIZENS;
    return c;
}

const Cursor& eastWestResizeCursor()
{
    static Cursor c = WebCursor::SIZEWE;
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    static Cursor c = WebCursor::SIZENESW;
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    static Cursor c = WebCursor::SIZENWSE;
    return c;
}

const Cursor& columnResizeCursor()
{
    static Cursor c = WebCursor::COLRESIZE;
    return c;
}

const Cursor& rowResizeCursor()
{
    static Cursor c = WebCursor::ROWRESIZE;
    return c;
}

const Cursor& moveCursor() 
{
    static Cursor c = WebCursor::SIZEALL;
    return c;
}

const Cursor& verticalTextCursor()
{
    static Cursor c = WebCursor::VERTICALTEXT;
    return c;
}

const Cursor& cellCursor()
{
    static Cursor c = WebCursor::CELL;
    return c;
}

const Cursor& contextMenuCursor()
{
    return pointerCursor();
}

const Cursor& aliasCursor()
{
    static Cursor c = WebCursor::ALIAS;
    return c;
}

const Cursor& progressCursor()
{
    static Cursor c = WebCursor::APPSTARTING;
    return c;
}

const Cursor& noDropCursor()
{
    return notAllowedCursor();
}

const Cursor& copyCursor()
{
    static Cursor c = WebCursor::COPYCUR;
    return c;
}

const Cursor& noneCursor()
{
    return pointerCursor();
}

const Cursor& notAllowedCursor()
{
    static Cursor c = WebCursor::NO;
    return c;
}

const Cursor& zoomInCursor()
{
    static Cursor c = WebCursor::ZOOMIN;
    return c;
}

const Cursor& zoomOutCursor()
{
    static Cursor c = WebCursor::ZOOMOUT;
    return c;
}

}
