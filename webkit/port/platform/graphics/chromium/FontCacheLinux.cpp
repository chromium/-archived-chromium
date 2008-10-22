// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FontCache.h"
#include "AtomicString.h"

#include "NotImplemented.h"

namespace WebCore {

// TODO(agl): stubs only


void FontCache::platformInit() { }

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
    notImplemented();
    return 0;
}

void FontCache::getTraitsInFamily(const AtomicString& familyName,
                                  Vector<unsigned>& traitsMasks)
{
    notImplemented();
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription,
                                                    const AtomicString& family)
{
    notImplemented();
    return 0;
}

AtomicString FontCache::getGenericFontForScript(UScriptCode script,
                                                const FontDescription&) 
{
    notImplemented();
    return AtomicString();
}

}  // namespace WebCore
