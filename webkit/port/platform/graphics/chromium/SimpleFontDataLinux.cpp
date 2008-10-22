// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Font.h"
#include "FontCache.h"
#include "SimpleFontData.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include "NotImplemented.h"

namespace WebCore {

// TODO(agl): only stubs

void SimpleFontData::platformInit() { }

void SimpleFontData::platformDestroy() { }

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    notImplemented();
    return NULL;
}

bool SimpleFontData::containsCharacters(const UChar* characters,
                                        int length) const
{
    notImplemented();
    return false;
}

void SimpleFontData::determinePitch()
{
    notImplemented();
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    notImplemented();
    return 0;
}

}  // namespace WebCore
