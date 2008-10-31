// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontPlatformDataLinux_h
#define FontPlatformDataLinux_h

#include "config.h"
#include "build/build_config.h"

#include <pango/pango.h>

#include "StringImpl.h"
#include "NotImplemented.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class FontDescription;

class FontPlatformData {
public:
    // Used for deleted values in the font cache's hash tables. The hash table
    // will create us with this structure, and it will compare other values
    // to this "Deleted" one. It expects the Deleted one to be differentiable
    // from the NULL one (created with the empty constructor), so we can't just
    // set everything to NULL.
    FontPlatformData(WTF::HashTableDeletedValueType)
        : m_context(0)
        , m_font(hashTableDeletedFontValue())
        { }

    FontPlatformData()
        : m_context(0)
        , m_font(0)
        { }

    FontPlatformData(const FontDescription&, const AtomicString& family);

    FontPlatformData(float size, bool bold, bool oblique);

    ~FontPlatformData();

    static bool init();

    bool isFixedPitch() const;
    float size() const { return m_size; }

    unsigned hash() const
    {
        notImplemented();
        return 0;
    }

    bool operator==(const FontPlatformData& other) const;
    bool isHashTableDeletedValue() const { return m_font == hashTableDeletedFontValue(); }

    static PangoFontMap* m_fontMap;
    static GHashTable* m_hashTable;

    PangoContext* m_context;
    PangoFont* m_font;

    float m_size;
    bool m_syntheticBold;
    bool m_syntheticOblique;

private:
    static PangoFont* hashTableDeletedFontValue() { return reinterpret_cast<PangoFont*>(-1); }
};

}  // namespace WebCore

#endif  // ifdef FontPlatformData_h
