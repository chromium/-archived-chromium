// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FontPlatformData.h"

#include "SkPaint.h"
#include "SkTypeface.h"

namespace WebCore {

FontPlatformData::FontPlatformData(const FontPlatformData& src)
{
    src.m_typeface->safeRef();
    m_typeface = src.m_typeface;

    m_textSize = src.m_textSize;
    m_fakeBold   = src.m_fakeBold;
    m_fakeItalic = src.m_fakeItalic;
}

FontPlatformData::FontPlatformData(SkTypeface* tf, float textSize, bool fakeBold, bool fakeItalic)
    : m_typeface(tf)
    , m_textSize(textSize)
    , m_fakeBold(fakeBold)
    , m_fakeItalic(fakeItalic)
{
    m_typeface->safeRef();
}

FontPlatformData::FontPlatformData(const FontPlatformData& src, float textSize)
    : m_typeface(src.m_typeface)
    , m_textSize(textSize)
    , m_fakeBold(src.m_fakeBold)
    , m_fakeItalic(src.m_fakeItalic)
{
    m_typeface->safeRef();
}

FontPlatformData::~FontPlatformData()
{
    m_typeface->safeUnref();
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& src)
{
    SkRefCnt_SafeAssign(m_typeface, src.m_typeface);

    m_textSize   = src.m_textSize;
    m_fakeBold   = src.m_fakeBold;
    m_fakeItalic = src.m_fakeItalic;
    
    return *this;
}
    
void FontPlatformData::setupPaint(SkPaint* paint) const
{
    const float ts = m_textSize > 0 ? m_textSize : 12;

    paint->setAntiAlias(true);
    paint->setSubpixelText(true);
    paint->setTextSize(SkFloatToScalar(ts));
    paint->setTypeface(m_typeface);
    paint->setFakeBoldText(m_fakeBold);
    paint->setTextSkewX(m_fakeItalic ? -SK_Scalar1/4 : 0);
    paint->setTextEncoding(SkPaint::kUTF16_TextEncoding);
}

bool FontPlatformData::operator==(const FontPlatformData& a) const
{
    // If either of the typeface pointers are invalid (either NULL or the
    // special deleted value) then we test for pointer equality. Otherwise, we
    // call SkTypeface::Equal on the valid pointers.
    const bool typefaces_equal =
      (m_typeface == hashTableDeletedFontValue() ||
       a.m_typeface == hashTableDeletedFontValue() ||
       !m_typeface || !a.m_typeface) ?
      (m_typeface == a.m_typeface) :
      SkTypeface::Equal(m_typeface, a.m_typeface);
    return typefaces_equal &&
           m_textSize == a.m_textSize &&
           m_fakeBold == a.m_fakeBold &&
           m_fakeItalic == a.m_fakeItalic;
}

unsigned FontPlatformData::hash() const
{
    // This is taken from Android code. It is not our fault.
    unsigned h = SkTypeface::UniqueID(m_typeface);
    h ^= 0x01010101 * (((int)m_fakeBold << 1) | (int)m_fakeItalic);
    h ^= *reinterpret_cast<const uint32_t *>(&m_textSize);
    return h;
}

bool FontPlatformData::isFixedPitch() const
{
    notImplemented();
    return false;
}

}  // namespace WebCore
