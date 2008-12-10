/*
 * Copyright (C) 2007 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef FontCustomPlatformData_h
#define FontCustomPlatformData_h

#if PLATFORM(DARWIN)
// TODO(port): This #include isn't strictly kosher, but we're currently using
// the Mac font code from upstream WebKit, and we need to pick up their header.
#undef FontCustomPlatformData_h
#include "third_party/WebKit/WebCore/platform/graphics/mac/FontCustomPlatformData.h"
#else

#include <wtf/Noncopyable.h>

#if PLATFORM(WIN_OS)
#include <windows.h>
#endif

namespace WebCore {

class FontPlatformData;
class SharedBuffer;

struct FontCustomPlatformData : Noncopyable {
#if PLATFORM(WIN_OS)
    FontCustomPlatformData(HFONT font)
        : m_font(font)
    {}
#endif

    ~FontCustomPlatformData();

    FontPlatformData fontPlatformData(int size, bool bold, bool italic);

#if PLATFORM(WIN_OS)
    HFONT m_font;
#endif
};

FontCustomPlatformData* createFontCustomPlatformData(SharedBuffer*);

}

#endif
#endif
