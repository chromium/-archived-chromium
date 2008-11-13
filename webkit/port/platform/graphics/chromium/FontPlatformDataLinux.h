// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontPlatformDataLinux_h
#define FontPlatformDataLinux_h

#include "config.h"
#include "build/build_config.h"

#include "StringImpl.h"
#include "NotImplemented.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

class SkPaint;
class SkTypeface;

namespace WebCore {

class FontDescription;

// -----------------------------------------------------------------------------
// FontPlatformData is the handle which WebKit has on a specific face. A face
// is the tuple of (font, size, ...etc). Here we are just wrapping a Skia
// SkTypeface pointer and dealing with the reference counting etc.
// -----------------------------------------------------------------------------
class FontPlatformData {
public:
    // Used for deleted values in the font cache's hash tables. The hash table
    // will create us with this structure, and it will compare other values
    // to this "Deleted" one. It expects the Deleted one to be differentiable
    // from the NULL one (created with the empty constructor), so we can't just
    // set everything to NULL.
    FontPlatformData(WTF::HashTableDeletedValueType)
        : m_typeface(hashTableDeletedFontValue())
        , m_textSize(0)
        , m_fakeBold(false)
        , m_fakeItalic(false)
        { }

    FontPlatformData()
        : m_typeface(0)
        , m_textSize(0)
        , m_fakeBold(false)
        , m_fakeItalic(false)
        { }

    FontPlatformData(float textSize, bool fakeBold, bool fakeItalic)
        : m_typeface(0)
        , m_textSize(textSize)
        , m_fakeBold(fakeBold)
        , m_fakeItalic(fakeItalic)
        { }

    FontPlatformData(const FontPlatformData&);
    FontPlatformData(SkTypeface *, float textSize, bool fakeBold, bool fakeItalic);
    FontPlatformData(const FontPlatformData& src, float textSize);
    ~FontPlatformData();

    // -------------------------------------------------------------------------
    // Return true iff this font is monospaced (i.e. every glyph has an equal x
    // advance)
    // -------------------------------------------------------------------------
    bool isFixedPitch() const;

    // -------------------------------------------------------------------------
    // Setup a Skia painting context to use this font.
    // -------------------------------------------------------------------------
    void setupPaint(SkPaint*) const;

    unsigned hash() const;
    float size() const { return m_textSize; }

    bool operator==(const FontPlatformData& other) const;
    FontPlatformData& operator=(const FontPlatformData& src);
    bool isHashTableDeletedValue() const { return m_typeface == hashTableDeletedFontValue(); }

private:
    SkTypeface* m_typeface;
    float m_textSize;
    bool m_fakeBold;
    bool m_fakeItalic;

    SkTypeface* hashTableDeletedFontValue() const { return reinterpret_cast<SkTypeface*>(-1); }
};

}  // namespace WebCore

#endif  // ifdef FontPlatformData_h
