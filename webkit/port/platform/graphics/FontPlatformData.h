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

#include "StringImpl.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

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
    FontPlatformData(WTF::HashTableDeletedValueType)
    : m_font(hashTableDeletedFontValue())
    , m_size(-1)
    , m_isMLangFont(false)
    {}

    FontPlatformData()
    : m_font(0)
    , m_size(0)
    , m_isMLangFont(false)
    {}

    FontPlatformData(HFONT hfont, float size,
                     bool isMLangFont);
    FontPlatformData(float size, bool bold, bool oblique);

    bool isHashTableDeletedValue() const { return m_font == hashTableDeletedFontValue(); }

    ~FontPlatformData();

    HFONT hfont() const { return m_font ? m_font->hfont() : 0; }
    float size() const { return m_size; }
    bool isMLangFont() const { return m_isMLangFont; }

    unsigned hash() const
    { 
        return m_font ? m_font->hash() : NULL;
    }

    bool operator==(const FontPlatformData& other) const
    { 
        return m_font == other.m_font && m_size == other.m_size;
    }

private:
    // We refcount the internal HFONT so that FontPlatformData can be
    // efficiently copied. WebKit depends on being able to copy it, and we
    // don't really want to re-create the HFONT.
    class RefCountedHFONT : public RefCounted<RefCountedHFONT> {
    public:
        static PassRefPtr<RefCountedHFONT> create(HFONT hfont)
        {
            return adoptRef(new RefCountedHFONT(hfont));
        }

        ~RefCountedHFONT();

        HFONT hfont() const { return m_hfont; }
        unsigned hash() const
        {
            return StringImpl::computeHash(reinterpret_cast<const UChar*>(&m_hfont), sizeof(HFONT) / sizeof(UChar));
        }

    private:
        // The create() function assumes there is already a refcount of one 
        // so it can do adoptRef.
        RefCountedHFONT(HFONT hfont)
            : m_hfont(hfont)
        {
        }

        HFONT m_hfont;
    };

    static RefCountedHFONT* hashTableDeletedFontValue();

    RefPtr<RefCountedHFONT> m_font;
    float m_size;  // Point size of the font in pixels.

    bool m_isMLangFont;
};

}

#endif
