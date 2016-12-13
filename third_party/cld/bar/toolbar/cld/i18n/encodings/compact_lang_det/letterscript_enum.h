// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef I18N_ENCODINGS_COMPACT_LANG_DET_LETTERSCRIPT_ENUM_H__
#define I18N_ENCODINGS_COMPACT_LANG_DET_LETTERSCRIPT_ENUM_H__

enum UnicodeLScript {
  ULScript_Common,
  ULScript_Latin,
  ULScript_Greek,
  ULScript_Cyrillic,
  ULScript_Armenian,
  ULScript_Hebrew,
  ULScript_Arabic,
  ULScript_Syriac,
  ULScript_Thaana,
  ULScript_Devanagari,
  ULScript_Bengali,
  ULScript_Gurmukhi,
  ULScript_Gujarati,
  ULScript_Oriya,
  ULScript_Tamil,
  ULScript_Telugu,
  ULScript_Kannada,
  ULScript_Malayalam,
  ULScript_Sinhala,
  ULScript_Thai,
  ULScript_Lao,
  ULScript_Tibetan,
  ULScript_Myanmar,
  ULScript_Georgian,
  ULScript_HanCJK,
  ULScript_Ethiopic,
  ULScript_Cherokee,
  ULScript_Canadian_Aboriginal,
  ULScript_Ogham,
  ULScript_Runic,
  ULScript_Khmer,
  ULScript_Mongolian,
  ULScript_Yi,
  ULScript_Old_Italic,
  ULScript_Gothic,
  ULScript_Deseret,
  ULScript_Inherited,
  ULScript_Tagalog,
  ULScript_Hanunoo,
  ULScript_Buhid,
  ULScript_Tagbanwa,
  ULScript_Limbu,
  ULScript_Tai_Le,
  ULScript_Linear_B,
  ULScript_Ugaritic,
  ULScript_Shavian,
  ULScript_Osmanya,
  ULScript_Cypriot,
  ULScript_Buginese,
  ULScript_Coptic,
  ULScript_New_Tai_Lue,
  ULScript_Glagolitic,
  ULScript_Tifinagh,
  ULScript_Syloti_Nagri,
  ULScript_Old_Persian,
  ULScript_Kharoshthi,
  ULScript_Balinese,
  ULScript_Cuneiform,
  ULScript_Phoenician,
  ULScript_Phags_Pa,
  ULScript_Nko,

  // Unicode 5.1
  ULScript_Sundanese,
  ULScript_Lepcha,
  ULScript_Ol_Chiki,
  ULScript_Vai,
  ULScript_Saurashtra,
  ULScript_Kayah_Li,
  ULScript_Rejang,
  ULScript_Lycian,
  ULScript_Carian,
  ULScript_Lydian,
  ULScript_Cham,
  ULScript_NUM_SCRIPTS
};


static const UnicodeLScript UNKNOWN_LSCRIPT = ULScript_Common;


// Return the name corresponding to the script ls, e.g. "Latin".
// It is a fatal error if ls is not a valid UnicodeLScript.
const char* UnicodeLScriptName(const UnicodeLScript ls);


// Return the 4-letter code corresponding to the script ls, e.g. "Latn".
// It is a fatal error if ls is not a valid UnicodeLScript.
const char* UnicodeLScriptCode(const UnicodeLScript ls);

#endif  // I18N_ENCODINGS_COMPACT_LANG_DET_LETTERSCRIPT_ENUM_H__
