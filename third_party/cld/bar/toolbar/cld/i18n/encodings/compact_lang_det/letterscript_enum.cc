// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/letterscript_enum.h"

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_logging.h"

static const char* kUnicodeLScriptNames[ULScript_NUM_SCRIPTS] = {
  "Common",
  "Latin",
  "Greek",
  "Cyrillic",
  "Armenian",
  "Hebrew",
  "Arabic",
  "Syriac",
  "Thaana",
  "Devanagari",
  "Bengali",
  "Gurmukhi",
  "Gujarati",
  "Oriya",
  "Tamil",
  "Telugu",
  "Kannada",
  "Malayalam",
  "Sinhala",
  "Thai",
  "Lao",
  "Tibetan",
  "Myanmar",
  "Georgian",
  "HanCJK",
  "Ethiopic",
  "Cherokee",
  "Canadian_Aboriginal",
  "Ogham",
  "Runic",
  "Khmer",
  "Mongolian",
  "Yi",
  "Old_Italic",
  "Gothic",
  "Deseret",
  "Inherited",
  "Tagalog",
  "Hanunoo",
  "Buhid",
  "Tagbanwa",
  "Limbu",
  "Tai_Le",
  "Linear_B",
  "Ugaritic",
  "Shavian",
  "Osmanya",
  "Cypriot",
  "Buginese",
  "Coptic",
  "New_Tai_Lue",
  "Glagolitic",
  "Tifinagh",
  "Syloti_Nagri",
  "Old_Persian",
  "Kharoshthi",
  "Balinese",
  "Cuneiform",
  "Phoenician",
  "Phags_Pa",
  "Nko",

  // Unicode 5.1 beta
  "Sundanese",
  "Lepcha",
  "Ol_Chiki",
  "Vai",
  "Saurashtra",
  "Kayah_Li",
  "Rejang",
  "Lycian",
  "Carian",
  "Lydian",
  "Cham",
};


// Unicode 5.1 beta script names from
// http://www.unicode.org/Public/5.1.0/diffs/5.0.0-5.1.0.all.2.diffs
// NOTE: 'Vai ' => "Vaii" to make four letters, not three
// see http://unicode.org/iso15924/iso15924-codes.html
const char* const kLScriptName4[ULScript_NUM_SCRIPTS] = {
  "Zyyy", "Latn", "Grek", "Cyrl",   "Armn", "Hebr", "Arab", "Syrc",
  "Thaa", "Deva", "Beng", "Guru",   "Gujr", "Orya", "Taml", "Telu",
  "Knda", "Mlym", "Sinh", "Thai",   "Laoo", "Tibt", "Mymr", "Geor",
  "Hani", "Ethi", "Cher", "Cans",   "Ogam", "Runr", "Khmr", "Mong",

  "Yiii", "Ital", "Goth", "Dsrt",   "Zzzz", "Tglg", "Hano", "Buhd",
  "Tagb", "Limb", "Tale", "Linb",   "Ugar", "Shaw", "Osma", "Cprt",
  "Bugi", "Copt", "Talu", "Glag",   "Tfng", "Sylo", "Xpeo", "Khar",
  "Bali", "Xsux", "Phnx", "Phag",   "Nkoo",

  // Unicode 5.1 beta
  "Sund", "Lepc", "Olck", "Vaii", "Saur", "Kali", "Rjng", "Lyci",
  "Cari", "Lydi", "Cham",
};


const char* UnicodeLScriptName(const UnicodeLScript ls) {
  CHECK(ls >= 0 && ls < ULScript_NUM_SCRIPTS);
  return kUnicodeLScriptNames[ls];
}


const char* UnicodeLScriptCode(const UnicodeLScript ls) {
  CHECK(ls >= 0 && ls < ULScript_NUM_SCRIPTS);
  return kLScriptName4[ls];
}
