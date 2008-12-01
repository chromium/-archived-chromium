// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FontCache.h"

#include "AtomicString.h"
#include "CString.h"
#include "FontDescription.h"
#include "FontPlatformData.h"
#include "Logging.h"
#include "NotImplemented.h"

#include "SkPaint.h"
#include "SkTypeface.h"
#include "SkUtils.h"

namespace WebCore {

void FontCache::platformInit()
{
}

const SimpleFontData* FontCache::getFontDataForCharacters(const Font& font,
                                                          const UChar* characters, 
                                                          int length)
{
    notImplemented();
    return NULL;
}

const AtomicString& FontCache::alternateFamilyName(const AtomicString& familyName)
{
    notImplemented();

    // This is just to stop GCC emitting a warning about returning a reference
    // to a temporary variable
    static AtomicString a;
    return a;
}

FontPlatformData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return 0;
}

FontPlatformData* FontCache::getLastResortFallbackFont(const FontDescription& description)
{
    static AtomicString arialStr("Arial");
    return getCachedFontPlatformData(description, arialStr);
}

void FontCache::getTraitsInFamily(const AtomicString& familyName,
                                  Vector<unsigned>& traitsMasks)
{
    notImplemented();
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription,
                                                    const AtomicString& family)
{
    const char* name = 0;
    CString s;
    
    if (family.length() == 0) {
        static const struct {
            FontDescription::GenericFamilyType mType;
            const char* mName;
        } gNames[] = {
            { FontDescription::SerifFamily, "serif" },
            { FontDescription::SansSerifFamily, "sans-serif" },
            { FontDescription::MonospaceFamily, "monospace" },
            { FontDescription::CursiveFamily, "cursive" },
            { FontDescription::FantasyFamily, "fantasy" }
        };

        FontDescription::GenericFamilyType type = fontDescription.genericFamily();
        for (unsigned i = 0; i < SK_ARRAY_COUNT(gNames); i++) {
            if (type == gNames[i].mType) {
                name = gNames[i].mName;
                break;
            }
        }
        // if we fall out of the loop, it's ok for name to still be 0
    }
    else {    // convert the name to utf8
        s = family.string().utf8();
        name = s.data();
    }
    
    int style = SkTypeface::kNormal;
    if (fontDescription.weight() >= FontWeightBold)
        style |= SkTypeface::kBold;
    if (fontDescription.italic())
        style |= SkTypeface::kItalic;

    SkTypeface* tf = SkTypeface::Create(name, (SkTypeface::Style)style);

    FontPlatformData* result =
        new FontPlatformData(tf,
                             fontDescription.computedSize(),
                             (style & SkTypeface::kBold) && !tf->isBold(),
                             (style & SkTypeface::kItalic) && !tf->isItalic());
    tf->unref();
    return result;
}

AtomicString FontCache::getGenericFontForScript(UScriptCode script,
                                                const FontDescription&) 
{
    notImplemented();
    return AtomicString();
}

}  // namespace WebCore
