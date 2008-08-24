// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A collection of utilities for font handling.

#ifndef BASE_GFX_FONT_UTILS_H__
#define BASE_GFX_FONT_UTILS_H__

#include <usp10.h>
#include <wchar.h>
#include <windows.h>

#include <unicode/uscript.h>

namespace gfx {

// The order of family types needs to be exactly the same as
// WebCore::FontDescription::GenericFamilyType. We may lift that restriction
// when we make webkit_glue::WebkitGenericToChromeGenericFamily more
// intelligent.
enum GenericFamilyType {
  GENERIC_FAMILY_NONE = 0,
  GENERIC_FAMILY_STANDARD,
  GENERIC_FAMILY_SERIF,
  GENERIC_FAMILY_SANSSERIF,
  GENERIC_FAMILY_MONOSPACE,
  GENERIC_FAMILY_CURSIVE,
  GENERIC_FAMILY_FANTASY
};

// Return a font family that supports a script and belongs to |generic| font family.
// It can return NULL and a caller has to implement its own fallback.
const wchar_t* GetFontFamilyForScript(UScriptCode script,
                                      GenericFamilyType generic);

// Return a font family that can render |characters| based on
// what script characters belong to. When char_checked is non-NULL,
// it's filled with the character used to determine the script.
// When script_checked is non-NULL, the script used to determine
// the family is returned.
// TODO(jungshik) : This function needs a total overhaul.
const wchar_t* GetFallbackFamily(const wchar_t* characters,
                                 int length,
                                 GenericFamilyType generic,
                                 UChar32 *char_checked,
                                 UScriptCode *script_checked);
// Derive a new HFONT by replacing lfFaceName of LOGFONT with |family|,
// calculate the ascent for the derived HFONT, and initialize SCRIPT_CACHE
// in FontData.
// |style| is only used for cache key generation. |style| is
// bit-wise OR of BOLD(1), UNDERLINED(2) and ITALIC(4) and
// should match what's contained in LOGFONT. It should be calculated
// by calling GetStyleFromLogFont.
// Returns false if the font is not accessible, in which case |ascent| field
// of |fontdata| is set to kUndefinedAscent.
// Be aware that this is not thread-safe.
// TODO(jungshik): Instead of having three out params, we'd better have one
// (|*FontData|), but somehow it mysteriously messes up the layout for
// certain complex script pages (e.g. hi.wikipedia.org) and also crashes
// at the start-up if recently visited page list includes pages with complex
// scripts in their title. Moreover, somehow the very first-pass of
// intl2 page-cycler test is noticeably slower with one out param than
// the current version although the subsequent 9 passes take about the
// same time.
bool GetDerivedFontData(const wchar_t *family,
                        int style,
                        LOGFONT *logfont,
                        int *ascent,
                        HFONT *hfont,
                        SCRIPT_CACHE **script_cache);

enum {
  FONT_STYLE_NORMAL = 0,
  FONT_STYLE_BOLD = 1,
  FONT_STYLE_ITALIC = 2,
  FONT_STYLE_UNDERLINED = 4
};

// Derive style (bit-wise OR of FONT_STYLE_BOLD, FONT_STYLE_UNDERLINED, and
// FONT_STYLE_ITALIC) from LOGFONT. Returns 0 if |*logfont| is NULL.
int GetStyleFromLogfont(const LOGFONT *logfont);

}  // namespace gfx

#endif // BASE_GFX_FONT_UTILS_H__

