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

#include "config.h"
#include "WebCursorInfo.h"

#include "Cursor.h"
#include <wtf/Assertions.h>

using namespace WebCore;

namespace WebKit {

// Ensure that our publicly defined enum values never get out of sync with the
// ones declared for use within WebCore.
#define COMPILE_ASSERT_MATCHING_ENUM(name) \
    COMPILE_ASSERT(int(WebCursorInfo::name) == int(PlatformCursor::name), name)

COMPILE_ASSERT_MATCHING_ENUM(TypePointer);
COMPILE_ASSERT_MATCHING_ENUM(TypeCross);
COMPILE_ASSERT_MATCHING_ENUM(TypeHand);
COMPILE_ASSERT_MATCHING_ENUM(TypeIBeam);
COMPILE_ASSERT_MATCHING_ENUM(TypeWait);
COMPILE_ASSERT_MATCHING_ENUM(TypeHelp);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeWestResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthSouthResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastWestResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastSouthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestSouthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeColumnResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeRowResize);
COMPILE_ASSERT_MATCHING_ENUM(TypeMiddlePanning);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastPanning);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthPanning);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastPanning);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestPanning);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthPanning);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthEastPanning);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthWestPanning);
COMPILE_ASSERT_MATCHING_ENUM(TypeWestPanning);
COMPILE_ASSERT_MATCHING_ENUM(TypeMove);
COMPILE_ASSERT_MATCHING_ENUM(TypeVerticalText);
COMPILE_ASSERT_MATCHING_ENUM(TypeCell);
COMPILE_ASSERT_MATCHING_ENUM(TypeContextMenu);
COMPILE_ASSERT_MATCHING_ENUM(TypeAlias);
COMPILE_ASSERT_MATCHING_ENUM(TypeProgress);
COMPILE_ASSERT_MATCHING_ENUM(TypeNoDrop);
COMPILE_ASSERT_MATCHING_ENUM(TypeCopy);
COMPILE_ASSERT_MATCHING_ENUM(TypeNone);
COMPILE_ASSERT_MATCHING_ENUM(TypeNotAllowed);
COMPILE_ASSERT_MATCHING_ENUM(TypeZoomIn);
COMPILE_ASSERT_MATCHING_ENUM(TypeZoomOut);
COMPILE_ASSERT_MATCHING_ENUM(TypeCustom);

WebCursorInfo::WebCursorInfo(const Cursor& cursor)
{
    type = static_cast<Type>(cursor.impl().type());
    hotSpot = cursor.impl().hotSpot();
    customImage = cursor.impl().customImage();
#ifdef WIN32
    externalHandle = 0;
#endif
}

} // namespace WebKit
