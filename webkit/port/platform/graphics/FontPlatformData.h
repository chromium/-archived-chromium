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

#ifndef FontPlatformData_H
#define FontPlatformData_H

#include "config.h"

#include "StringImpl.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

// TODO(tc): Once this file is only included by the ChromiumWin build, we can
// remove the PLATFORM #ifs.
#if PLATFORM(WIN_OS)
#include <usp10.h>
#endif

typedef struct HFONT__ *HFONT;

namespace WebCore {

class FontDescription;

class FontPlatformData
{
public:
    // Used for deleted values in the font cache's hash tables. The hash table
    // will create us with this structure, and it will compare other values
    // to this "Deleted" one. It expects the Deleted one to be differentiable
    // from the NULL one (created with the empty constructor), so we can't just
    // set everything to NULL.
    FontPlatformData(WTF::HashTableDeletedValueType);
    FontPlatformData();
    FontPlatformData(HFONT hfont, float size,
                     bool isMLangFont);
    FontPlatformData(float size, bool bold, bool oblique);
    FontPlatformData(const FontPlatformData& data);

    FontPlatformData& operator=(const FontPlatformData& data);

    bool isHashTableDeletedValue() const { return m_font == hashTableDeletedFontValue(); }

    ~FontPlatformData();

    HFONT hfont() const { return m_font ? m_font->hfont() : 0; }
    float size() const { return m_size; }

    unsigned hash() const
    { 
        return m_font ? m_font->hash() : NULL;
    }

    bool operator==(const FontPlatformData& other) const
    { 
        return m_font == other.m_font && m_size == other.m_size;
    }

#if PLATFORM(WIN_OS)
    SCRIPT_FONTPROPERTIES* scriptFontProperties() const;
    SCRIPT_CACHE* scriptCache() const { return &m_scriptCache; }
#endif

private:
    // We refcount the internal HFONT so that FontPlatformData can be
    // efficiently copied. WebKit depends on being able to copy it, and we
    // don't really want to re-create the HFONT.
    class RefCountedHFONT : public RefCounted<RefCountedHFONT> {
    public:
        static PassRefPtr<RefCountedHFONT> create(HFONT hfont, bool isMLangFont)
        {
            return adoptRef(new RefCountedHFONT(hfont, isMLangFont));
        }

        ~RefCountedHFONT();

        HFONT hfont() const { return m_hfont; }
        unsigned hash() const
        {
            return StringImpl::computeHash(reinterpret_cast<const UChar*>(&m_hfont), sizeof(HFONT) / sizeof(UChar));
        }

        bool operator==(const RefCountedHFONT& other) const
        {
            return m_hfont == other.m_hfont;
        }

    private:
        // The create() function assumes there is already a refcount of one 
        // so it can do adoptRef.
        RefCountedHFONT(HFONT hfont, bool isMLangFont)
            : m_hfont(hfont)
            , m_isMLangFont(isMLangFont)
        {
        }

        HFONT m_hfont;
        bool m_isMLangFont;
    };

    static RefCountedHFONT* hashTableDeletedFontValue();

    RefPtr<RefCountedHFONT> m_font;
    float m_size;  // Point size of the font in pixels.

#if PLATFORM(WIN_OS)
    mutable SCRIPT_CACHE m_scriptCache;
    mutable SCRIPT_FONTPROPERTIES* m_scriptFontProperties;
#endif
};

}

#endif
