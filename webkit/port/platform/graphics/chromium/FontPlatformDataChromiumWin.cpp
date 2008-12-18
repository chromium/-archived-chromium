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
#include "FontPlatformData.h"

#include <windows.h>
#include <objidl.h>
#include <mlang.h>

#include "ChromiumBridge.h"
#include "SkiaFontWin.h"

namespace WebCore {

FontPlatformData::FontPlatformData(WTF::HashTableDeletedValueType)
    : m_font(hashTableDeletedFontValue())
    , m_size(-1)
    , m_scriptCache(0)
    , m_scriptFontProperties(0)
{
}

FontPlatformData::FontPlatformData()
    : m_font(0)
    , m_size(0)
    , m_scriptCache(0)
    , m_scriptFontProperties(0)
{
}

FontPlatformData::FontPlatformData(HFONT font, float size)
    : m_font(RefCountedHFONT::create(font))
    , m_size(size)
    , m_scriptCache(0)
    , m_scriptFontProperties(0)
{
}

// TODO(jhaas): this ctor is needed for SVG fonts but doesn't seem
// to do much
FontPlatformData::FontPlatformData(float size, bool bold, bool oblique)
    : m_font(0)
    , m_size(size)
    , m_scriptCache(0)
    , m_scriptFontProperties(0)
{
}

FontPlatformData::FontPlatformData(const FontPlatformData& data)
    : m_font(data.m_font)
    , m_size(data.m_size)
    , m_scriptCache(0)
    , m_scriptFontProperties(0)
{
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& data)
{
    if (this != &data) {
        m_font = data.m_font;
        m_size = data.m_size;

        // The following fields will get re-computed if necessary.

        ScriptFreeCache(&m_scriptCache);
        m_scriptCache = 0;

        delete m_scriptFontProperties;
        m_scriptFontProperties = 0;
    } 
    return *this;
}

FontPlatformData::~FontPlatformData()
{
    ScriptFreeCache(&m_scriptCache);
    m_scriptCache = 0;

    delete m_scriptFontProperties;
    m_scriptFontProperties = 0;
}

FontPlatformData::RefCountedHFONT::~RefCountedHFONT()
{
    if (m_hfont != reinterpret_cast<HFONT>(-1)) {
        RemoveFontFromSkiaFontWinCache(m_hfont);
        DeleteObject(m_hfont);
    }
}

FontPlatformData::RefCountedHFONT* FontPlatformData::hashTableDeletedFontValue()
{
    static RefPtr<RefCountedHFONT> deletedValue =
        RefCountedHFONT::create(reinterpret_cast<HFONT>(-1));
    return deletedValue.get();
}

SCRIPT_FONTPROPERTIES* FontPlatformData::scriptFontProperties() const
{
    if (!m_scriptFontProperties) {
        m_scriptFontProperties = new SCRIPT_FONTPROPERTIES;
        memset(m_scriptFontProperties, 0, sizeof(SCRIPT_FONTPROPERTIES));
        m_scriptFontProperties->cBytes = sizeof(SCRIPT_FONTPROPERTIES);
        HRESULT result = ScriptGetFontProperties(0, scriptCache(),
                                                 m_scriptFontProperties);
        if (result == E_PENDING) {
            HDC dc = GetDC(0);
            HGDIOBJ oldFont = SelectObject(dc, hfont());
            HRESULT hr = ScriptGetFontProperties(dc, scriptCache(),
                                                 m_scriptFontProperties);
            if (S_OK != hr) {
                if (ChromiumBridge::ensureFontLoaded(hfont())) {
                    // Retry ScriptGetFontProperties.
                    // TODO(nsylvain): Handle gracefully the error if this call
                    // also fails. See bug 1136944.
                    hr = ScriptGetFontProperties(dc, scriptCache(),
                                                 m_scriptFontProperties);
                    if (S_OK != hr) {
                        ASSERT_NOT_REACHED();
                    }
                }
            }

            SelectObject(dc, oldFont);
            ReleaseDC(0, dc);
        }
    }
    return m_scriptFontProperties;
}

}
