// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef I18N_ENCODINGS_COMPACT_LANG_DET_EXT_LANG_ENC_H__
#define I18N_ENCODINGS_COMPACT_LANG_DET_EXT_LANG_ENC_H__

#include "third_party/cld/bar/toolbar/cld/i18n/languages/public/languages.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/letterscript_enum.h"


// Leave a small gap after the base languages, so adding one or two is easy.
// Just reduce the gap here (currently 5 entries)

#define EXT_LANGUAGE_BASE (NUM_LANGUAGES + 4)

// Google UI languages
#define X_BORK_BORK_BORK (Language)(EXT_LANGUAGE_BASE+0)
#define X_PIG_LATIN (Language)(EXT_LANGUAGE_BASE+1)
#define X_HACKER (Language)(EXT_LANGUAGE_BASE+2)
#define X_KLINGON (Language)(EXT_LANGUAGE_BASE+3)
#define X_ELMER_FUDD (Language)(EXT_LANGUAGE_BASE+4)

// Pseudo-languages for Unicode scripts that express a single language
#define X_OGHAM (Language)(EXT_LANGUAGE_BASE+5)
#define X_RUNIC (Language)(EXT_LANGUAGE_BASE+6)
#define X_YI (Language)(EXT_LANGUAGE_BASE+7)
#define X_OLD_ITALIC (Language)(EXT_LANGUAGE_BASE+8)
#define X_GOTHIC (Language)(EXT_LANGUAGE_BASE+9)
#define X_DESERET (Language)(EXT_LANGUAGE_BASE+10)
#define X_HANUNOO (Language)(EXT_LANGUAGE_BASE+11)
#define X_BUHID (Language)(EXT_LANGUAGE_BASE+12)
#define X_TAGBANWA (Language)(EXT_LANGUAGE_BASE+13)
#define X_TAI_LE (Language)(EXT_LANGUAGE_BASE+14)
#define X_LINEAR_B (Language)(EXT_LANGUAGE_BASE+15)
#define X_UGARITIC (Language)(EXT_LANGUAGE_BASE+16)
#define X_SHAVIAN (Language)(EXT_LANGUAGE_BASE+17)
#define X_OSMANYA (Language)(EXT_LANGUAGE_BASE+18)
#define X_CYPRIOT (Language)(EXT_LANGUAGE_BASE+19)
#define X_BUGINESE (Language)(EXT_LANGUAGE_BASE+20)
#define X_COPTIC (Language)(EXT_LANGUAGE_BASE+21)
#define X_NEW_TAI_LUE (Language)(EXT_LANGUAGE_BASE+22)
#define X_GLAGOLITIC (Language)(EXT_LANGUAGE_BASE+23)
#define X_TIFINAGH (Language)(EXT_LANGUAGE_BASE+24)
#define X_SYLOTI_NAGRI (Language)(EXT_LANGUAGE_BASE+25)
#define X_OLD_PERSIAN (Language)(EXT_LANGUAGE_BASE+26)
#define X_KHAROSHTHI (Language)(EXT_LANGUAGE_BASE+27)
#define X_BALINESE (Language)(EXT_LANGUAGE_BASE+28)
#define X_CUNEIFORM (Language)(EXT_LANGUAGE_BASE+29)
#define X_PHOENICIAN (Language)(EXT_LANGUAGE_BASE+30)
#define X_PHAGS_PA (Language)(EXT_LANGUAGE_BASE+31)
#define X_NKO (Language)(EXT_LANGUAGE_BASE+32)

// Unicode 5.1
#define X_SUDANESE (Language)(EXT_LANGUAGE_BASE+33)
#define X_LEPCHA (Language)(EXT_LANGUAGE_BASE+34)
#define X_OL_CHIKI (Language)(EXT_LANGUAGE_BASE+35)
#define X_VAI (Language)(EXT_LANGUAGE_BASE+36)
#define X_SAURASHTRA (Language)(EXT_LANGUAGE_BASE+37)
#define X_KAYAH_LI (Language)(EXT_LANGUAGE_BASE+38)
#define X_REJANG (Language)(EXT_LANGUAGE_BASE+39)
#define X_LYCIAN (Language)(EXT_LANGUAGE_BASE+40)
#define X_CARIAN (Language)(EXT_LANGUAGE_BASE+41)
#define X_LYDIAN (Language)(EXT_LANGUAGE_BASE+42)
#define X_CHAM (Language)(EXT_LANGUAGE_BASE+43)

#define EXT_NUM_LANGUAGES (Language)(EXT_LANGUAGE_BASE+44)



// ExtLanguageName
// ------------
// Given the Language, returns its string name used as the output by
// the lang/enc identifier, e.g. "Korean"
// "invalid_language" if the input is invalid.
extern const char* ExtLanguageName(const Language lang);

// ExtLanguageDeclaredName
// ------------
// Given the Language, returns its Language enum spelling, for use by
// programs that create C declarations, e.g. "KOREAN"
// "UNKNOWN_LANGUAGE" if the input is invalid.
extern const char* ExtLanguageDeclaredName(const Language lang);

// ExtLanguageCode
// ------------
// Given the Language, return the language code, e.g. "ko"
// This is determined by
// the following (in order of preference):
// - ISO-639-1 two-letter language code
//   (all except those mentioned below)
// - ISO-639-2 three-letter bibliographic language code
//   (Tibetan, Dhivehi, Cherokee, Syriac)
// - Google-specific language code
//   (ChineseT ("zh-TW"), Teragram Unknown, Unknown,
//   Portuguese-Portugal, Portuguese-Brazil, Limbu)
extern const char * ExtLanguageCode(const Language lang);


// Convert "en-Latn-GB" to ENGLISH
// Normalize to PORTUGUESE, not PORTUGUESE_B nor PORTUGUESE_P
// Consider for later: NORWEGIAN, NORWEGIAN_N
// Consider for later: SCOTS, SCOTS_GAELIC
// Consider for later: SERBO_CROATIAN, SERBIAN, CROATIAN, BOSNIAN
//
Language GetLanguageFromNumberOrName(const char* src);

// Convert "en-Latn-GB" to ULScript_Latin
UnicodeLScript GetLScriptFromNumberOrName(const char* src);

// Merge together some languages, such as bo/hr/sr
Language NormalizeLanguage(Language lang);

#endif  // I18N_ENCODINGS_COMPACT_LANG_DET_EXT_LANG_ENC_H__
