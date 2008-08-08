/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006, 2007 Apple Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include <windows.h>

#include "FontPlatformData.h"

namespace WebCore {

FontPlatformData::FontPlatformData(HFONT font, float size,
                                   bool isMLangFont)
    : m_font(RefCountedHFONT::create(font))
    , m_size(size)
    , m_isMLangFont(isMLangFont)
{
}

// TODO(jhaas): this ctor is needed for SVG fonts but doesn't seem
// to do much
FontPlatformData::FontPlatformData(float size, bool bold, bool oblique)
    : m_size(size)
    , m_font(0)
{
}


FontPlatformData::~FontPlatformData()
{
}

FontPlatformData::RefCountedHFONT::~RefCountedHFONT()
{
    if (m_hfont != reinterpret_cast<HFONT>(-1))
        DeleteObject(m_hfont);
}

}
