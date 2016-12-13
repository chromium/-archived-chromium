// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/ext_lang_enc.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_macros.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_strtoint.h"

// Language names above NUM_LANGUAGES
// These are also the C enum declared names
static const char* const kExtLanguageName[] = {
"X_BORK_BORK_BORK", "X_PIG_LATIN", "X_HACKER", "X_KLINGON", "X_ELMER_FUDD",

// Pseudo-languages for Unicode scripts that express a single language
"X_OGHAM", "X_RUNIC", "X_YI", "X_OLD_ITALIC", "X_GOTHIC",
"X_DESERET", "X_HANUNOO", "X_BUHID", "X_TAGBANWA", "X_TAI_LE",
"X_LINEAR_B", "X_UGARITIC", "X_SHAVIAN", "X_OSMANYA", "X_CYPRIOT",
"X_BUGINESE", "X_COPTIC", "X_NEW_TAI_LUE", "X_GLAGOLITIC", "X_TIFINAGH",
"X_SYLOTI_NAGRI", "X_OLD_PERSIAN", "X_KHAROSHTHI", "X_BALINESE", "X_CUNEIFORM",
"X_PHOENICIAN", "X_PHAGS_PA", "X_NKO",

// Unicode 5.1
"X_SUDANESE", "X_LEPCHA", "X_OL_CHIKI", "X_VAI", "X_SAURASHTRA",
"X_KAYAH_LI", "X_REJANG", "X_LYCIAN", "X_CARIAN", "X_LYDIAN",
"X_CHAM",
};


// These are the C enum declared names, for programs creating C code
static const char* const kExtLangDeclaredName[] = {
  "ENGLISH",      /* 0 */
  "DANISH",       /* 1 */
  "DUTCH",        /* 2 */
  "FINNISH",      /* 3 */
  "FRENCH",       /* 4 */
  "GERMAN",       /* 5 */
  "HEBREW",       /* 6 */
  "ITALIAN",      /* 7 */
  "JAPANESE",     /* 8 */
  "KOREAN",       /* 9 */
  "NORWEGIAN",    /* 10 */
  "POLISH",       /* 11 */
  "PORTUGUESE",   /* 12 */
  "RUSSIAN",      /* 13 */
  "SPANISH",      /* 14 */
  "SWEDISH",      /* 15 */
  "CHINESE",      /* 16 */
  "CZECH",        /* 17 */
  "GREEK",        /* 18 */
  "ICELANDIC",    /* 19 */
  "LATVIAN",      /* 20 */
  "LITHUANIAN",   /* 21 */
  "ROMANIAN",     /* 22 */
  "HUNGARIAN",    /* 23 */
  "ESTONIAN",     /* 24 */
  "TG_UNKNOWN_LANGUAGE",  /* 25 */
  "UNKNOWN_LANGUAGE",     /* 26 */
  "BULGARIAN",    /* 27 */
  "CROATIAN",     /* 28 */
  "SERBIAN",      /* 29 */
  "IRISH",        /* 30 */
  "GALICIAN",     /* 31 */
  "TAGALOG",      /* 32 */
  "TURKISH",      /* 33 */
  "UKRAINIAN",    /* 34 */
  "HINDI",        /* 35 */
  "MACEDONIAN",   /* 36 */
  "BENGALI",      /* 37 */
  "INDONESIAN",   /* 38 */
  "LATIN",        /* 39 */
  "MALAY",        /* 40 */
  "MALAYALAM",    /* 41 */
  "WELSH",        /* 42 */
  "NEPALI",       /* 43 */
  "TELUGU",       /* 44 */
  "ALBANIAN",     /* 45 */
  "TAMIL",        /* 46 */
  "BELARUSIAN",   /* 47 */
  "JAVANESE",     /* 48 */
  "OCCITAN",      /* 49 */
  "URDU",         /* 50 */
  "BIHARI",       /* 51 */
  "GUJARATI",     /* 52 */
  "THAI",         /* 53 */
  "ARABIC",       /* 54 */
  "CATALAN",      /* 55 */
  "ESPERANTO",    /* 56 */
  "BASQUE",       /* 57 */
  "INTERLINGUA",  /* 58 */
  "KANNADA",      /* 59 */
  "PUNJABI",      /* 60 */
  "SCOTS_GAELIC", /* 61 */
  "SWAHILI",      /* 62 */
  "SLOVENIAN",    /* 63 */
  "MARATHI",      /* 64 */
  "MALTESE",      /* 65 */
  "VIETNAMESE",   /* 66 */
  "FRISIAN",      /* 67 */
  "SLOVAK",       /* 68 */
  "CHINESE_T",    /* 69 */
  "FAROESE",      /* 70 */
  "SUNDANESE",    /* 71 */
  "UZBEK",        /* 72 */
  "AMHARIC",      /* 73 */
  "AZERBAIJANI",  /* 74 */
  "GEORGIAN",     /* 75 */
  "TIGRINYA",     /* 76 */
  "PERSIAN",      /* 77 */
  "BOSNIAN",      /* 78 */
  "SINHALESE",    /* 79 */
  "NORWEGIAN_N",  /* 80 */
  "PORTUGUESE_P", /* 81 */
  "PORTUGUESE_B", /* 82 */
  "XHOSA",        /* 83 */
  "ZULU",         /* 84 */
  "GUARANI",      /* 85 */
  "SESOTHO",      /* 86 */
  "TURKMEN",      /* 87 */
  "KYRGYZ",       /* 88 */
  "BRETON",       /* 89 */
  "TWI",          /* 90 */
  "YIDDISH",      /* 91 */
  "SERBO_CROATIAN",       /* 92 */
  "SOMALI",       /* 93 */
  "UIGHUR",       /* 94 */
  "KURDISH",      /* 95 */
  "MONGOLIAN",    /* 96 */
  "ARMENIAN",     /* 97 */
  "LAOTHIAN",     /* 98 */
  "SINDHI",       /* 99 */
  "RHAETO_ROMANCE",  /* 100 */
  "AFRIKAANS",    /* 101 */
  "LUXEMBOURGISH", /* 102 */
  "BURMESE",      /* 103 */
  "KHMER",        /* 104 */
  "TIBETAN",      /* 105 */
  "DHIVEHI",      /* 106 */       // sometimes spelled Divehi; lang of Maldives
  "CHEROKEE",     /* 107 */
  "SYRIAC",       /* 108 */
  "LIMBU",        /* 109 */
  "ORIYA",        /* 110 */
  "ASSAMESE",     /* 111 */
  "CORSICAN",     /* 112 */
  "INTERLINGUE",  /* 113 */
  "KAZAKH",       /* 114 */
  "LINGALA",      /* 115 */
  "MOLDAVIAN",    /* 116 */
  "PASHTO",       /* 117 */
  "QUECHUA",      /* 118 */
  "SHONA",        /* 119 */
  "TAJIK",        /* 120 */
  "TATAR",        /* 121 */
  "TONGA",        /* 122 */
  "YORUBA",       /* 123 */
  "CREOLES_AND_PIDGINS_ENGLISH_BASED",      /* 124 */
  "CREOLES_AND_PIDGINS_FRENCH_BASED",       /* 125 */
  "CREOLES_AND_PIDGINS_PORTUGUESE_BASED",   /* 126 */
  "CREOLES_AND_PIDGINS_OTHER",              /* 127 */
  "MAORI",        /* 128 */
  "WOLOF",        /* 129 */
  "ABKHAZIAN",    /* 130 */
  "AFAR",         /* 131 */
  "AYMARA",       /* 132 */
  "BASHKIR",      /* 133 */
  "BISLAMA",      /* 134 */
  "DZONGKHA",     /* 135 */
  "FIJIAN",       /* 136 */
  "GREENLANDIC",  /* 137 */
  "HAUSA",        /* 138 */
  "HAITIAN_CREOLE",  /* 139 */
  "INUPIAK",      /* 140 */
  "INUKTITUT",    /* 141 */
  "KASHMIRI",     /* 142 */
  "KINYARWANDA",  /* 143 */
  "MALAGASY",     /* 144 */
  "NAURU",        /* 145 */
  "OROMO",        /* 146 */
  "RUNDI",        /* 147 */
  "SAMOAN",       /* 148 */
  "SANGO",        /* 149 */
  "SANSKRIT",     /* 150 */
  "SISWANT",      /* 151 */
  "TSONGA",       /* 152 */
  "TSWANA",       /* 153 */
  "VOLAPUK",      /* 154 */
  "ZHUANG",       /* 155 */
  "KHASI",        /* 156 */
  "SCOTS",        /* 157 */
  "GANDA",        /* 158 */
  "MANX",         /* 159 */
  "MONTENEGRIN",  /* 160 */
  // Add new language declared names just before here
};

COMPILE_ASSERT(arraysize(kExtLangDeclaredName) == NUM_LANGUAGES,
       kExtLangDeclaredName_has_incorrect_length);


// Language codes above NUM_LANGUAGES
// I made all these up, except Klingon from ISO-639-2
// NOTE: zza is a standard name
static const char* const kExtLanguageCode[] = {
  // "X_BORK_BORK_BORK", "X_PIG_LATIN", "X_HACKER", "X_KLINGON", "X_ELMER_FUDD",
  // All Latin script
  "zzb", "zzp", "zzh", "tlh", "zze",

  // Pseudo-languages for Unicode scripts that express a single language
  "xx-Ogam", "xx-Runr", "xx-Yiii", "xx-Ital", "xx-Goth",
  "xx-Dsrt", "xx-Hano", "xx-Buhd", "xx-Tagb", "xx-Tale",
  "xx-Linb", "xx-Ugar", "xx-Shaw", "xx-Osma", "xx-Cprt",
  "xx-Bugi", "xx-Copt", "xx-Talu", "xx-Glag", "xx-Tfng",
  "xx-Sylo", "xx-Xpeo", "xx-Khar", "xx-Bali", "xx-Xsux",
  "xx-Phnx", "xx-Phag", "xx-Nkoo",

  // Unicode 5.1
  "xx-Sund", "xx-Lepc", "xx-Olck", "xx-Vaii", "xx-Saur",
  "xx-Kali", "xx-Rjng", "xx-Lyci", "xx-Cari", "xx-Lydi",
  "xx-Cham",
};


// Given the Language, returns its string name used as the output by
// the lang/enc identifier, e.g. "Korean"
// "invalid_language" if the input is invalid.
// TG_UNKNOWN_LANGUAGE is used as a placeholder for the "ignore me" language,
// used to subtract out HTML, link farms, DNA strings, and alittle English porn
const char* ExtLanguageName(const Language lang) {
  if (lang < 0) {
    // No-text-at-all result from a Tote
    return "";
  }
  // CompactLanguageDetect extension
  if (lang == TG_UNKNOWN_LANGUAGE) {
    return "Ignore";
  }
  if ((0 <= lang) && (lang < NUM_LANGUAGES)) {
    return LanguageName(lang);
  }
  if ((EXT_LANGUAGE_BASE <= lang) && (lang < EXT_NUM_LANGUAGES)) {
    return kExtLanguageName[lang - EXT_LANGUAGE_BASE];
  }
  return invalid_language_name();
}


// Given the Language, returns its Language enum spelling, for use by
// programs that create C declarations, e.g. "KOREAN"
// "UNKNOWN_LANGUAGE" if the input is invalid.
const char* ExtLanguageDeclaredName(const Language lang) {
  if ((0 <= lang) && (lang < NUM_LANGUAGES)) {
    return kExtLangDeclaredName[lang];
  }
  if ((EXT_LANGUAGE_BASE <= lang) && (lang < EXT_NUM_LANGUAGES)) {
    return kExtLanguageName[lang - EXT_LANGUAGE_BASE];
  }
  return "UNKNOWN_LANGUAGE";
}

// Given the Language, return the language code, e.g. "ko"
const char* ExtLanguageCode(const Language lang) {
  // Hack for ignore/porn pseudo-language
  if (lang == TG_UNKNOWN_LANGUAGE) {
    return "xxx";
  }
  if ((0 <= lang) && (lang < NUM_LANGUAGES)) {
    return LanguageCode(lang);
  }
  if ((EXT_LANGUAGE_BASE <= lang) && (lang < EXT_NUM_LANGUAGES)) {
    return kExtLanguageCode[lang - EXT_LANGUAGE_BASE];
  }
  return "??";
}


// Convert "en-Latn-GB" to ENGLISH
// Normalize to PORTUGUESE, not PORTUGUESE_B nor PORTUGUESE_P
// Consider for later: NORWEGIAN, NORWEGIAN_N
// Consider for later: SCOTS, SCOTS_GAELIC
// Consider for later: SERBO_CROATIAN, SERBIAN, CROATIAN, BOSNIAN
//
Language GetLanguageFromNumberOrName(const char* src) {
  if (strspn(src, "0123456789") == strlen(src)) {
    // All digits
    return static_cast<Language>(strto32(src, NULL, 10));
  }

  Language retlang = UNKNOWN_LANGUAGE;
  size_t len = strlen(src);

  if (true /*FLAGS_mergepairs*/) {
    // Merge sets of langauges pt-xx en-xx fr-xx, NOT bs/hr/sr
    if (memcmp(src, "pt-", 3) == 0) {return PORTUGUESE;}
    if (memcmp(src, "en-", 3) == 0) {return ENGLISH;}
    if (memcmp(src, "fr-", 3) == 0) {return FRENCH;}
    // Use NormalizeLanguage instead
    if (memcmp(src, "bs-", 3) == 0) {return CROATIAN;}
    if (memcmp(src, "hr-", 3) == 0) {return CROATIAN;}
    if (memcmp(src, "sr-Latn", 7) == 0) {return CROATIAN;}
    if (memcmp(src, "sh-Latn", 7) == 0) {return CROATIAN;}
    if (memcmp(src, "sr-Cyrl", 7) == 0) {return SERBIAN;}
    if (memcmp(src, "sh-Cyrl", 7) == 0) {return SERBIAN;}
  }

  // Extensions
  if (len >= 3) {
    // Standin for ignore/porn "language"
    if (memcmp(src, "xxx", 3) == 0) {return TG_UNKNOWN_LANGUAGE;}

    if (memcmp(src, "zzb", 3) == 0) {return X_BORK_BORK_BORK;}
    if (memcmp(src, "zzp", 3) == 0) {return X_PIG_LATIN;}
    if (memcmp(src, "zzh", 3) == 0) {return X_HACKER;}
    if (memcmp(src, "tlh", 3) == 0) {return X_KLINGON;}
    if (memcmp(src, "zze", 3) == 0) {return X_ELMER_FUDD;}
  }

  // We have a name like en-Latn-GB or pt-BR
  // First, get rid of some special cases
  if (len <= 3) {
    LanguageFromCode(src, &retlang);
  } else if (len == 7) {
    // More Extensions
    if (memcmp(src, "xx-", 3) == 0) {
      if (memcmp(src, "xx-Ogam", 7) == 0) {return X_OGHAM;}
      if (memcmp(src, "xx-Runr", 7) == 0) {return X_RUNIC;}
      if (memcmp(src, "xx-Yiii", 7) == 0) {return X_YI;}
      if (memcmp(src, "xx-Ital", 7) == 0) {return X_OLD_ITALIC;}
      if (memcmp(src, "xx-Goth", 7) == 0) {return X_GOTHIC;}
      if (memcmp(src, "xx-Dsrt", 7) == 0) {return X_DESERET;}
      if (memcmp(src, "xx-Hano", 7) == 0) {return X_HANUNOO;}
      if (memcmp(src, "xx-Buhd", 7) == 0) {return X_BUHID;}
      if (memcmp(src, "xx-Tagb", 7) == 0) {return X_TAGBANWA;}
      if (memcmp(src, "xx-Tale", 7) == 0) {return X_TAI_LE;}
      if (memcmp(src, "xx-Linb", 7) == 0) {return X_LINEAR_B;}
      if (memcmp(src, "xx-Ugar", 7) == 0) {return X_UGARITIC;}
      if (memcmp(src, "xx-Shaw", 7) == 0) {return X_SHAVIAN;}
      if (memcmp(src, "xx-Osma", 7) == 0) {return X_OSMANYA;}
      if (memcmp(src, "xx-Cprt", 7) == 0) {return X_CYPRIOT;}
      if (memcmp(src, "xx-Bugi", 7) == 0) {return X_BUGINESE;}
      if (memcmp(src, "xx-Copt", 7) == 0) {return X_COPTIC;}
      if (memcmp(src, "xx-Talu", 7) == 0) {return X_NEW_TAI_LUE;}
      if (memcmp(src, "xx-Glag", 7) == 0) {return X_GLAGOLITIC;}
      if (memcmp(src, "xx-Tfng", 7) == 0) {return X_TIFINAGH;}
      if (memcmp(src, "xx-Sylo", 7) == 0) {return X_SYLOTI_NAGRI;}
      if (memcmp(src, "xx-Xpeo", 7) == 0) {return X_OLD_PERSIAN;}
      if (memcmp(src, "xx-Khar", 7) == 0) {return X_KHAROSHTHI;}
      if (memcmp(src, "xx-Bali", 7) == 0) {return X_BALINESE;}
      if (memcmp(src, "xx-Xsux", 7) == 0) {return X_CUNEIFORM;}
      if (memcmp(src, "xx-Phnx", 7) == 0) {return X_PHOENICIAN;}
      if (memcmp(src, "xx-Phag", 7) == 0) {return X_PHAGS_PA;}
      if (memcmp(src, "xx-Nkoo", 7) == 0) {return X_NKO;}

      // Unicode 5.1
      if (memcmp(src, "xx-Sund", 7) == 0) {return X_SUDANESE;}
      if (memcmp(src, "xx-Lepc", 7) == 0) {return X_LEPCHA;}
      if (memcmp(src, "xx-Olck", 7) == 0) {return X_OL_CHIKI;}
      if (memcmp(src, "xx-Vaii", 7) == 0) {return X_VAI;}
      if (memcmp(src, "xx-Saur", 7) == 0) {return X_SAURASHTRA;}
      if (memcmp(src, "xx-Kali", 7) == 0) {return X_KAYAH_LI;}
      if (memcmp(src, "xx-Rjng", 7) == 0) {return X_REJANG;}
      if (memcmp(src, "xx-Lyci", 7) == 0) {return X_LYCIAN;}
      if (memcmp(src, "xx-Cari", 7) == 0) {return X_CARIAN;}
      if (memcmp(src, "xx-Lydi", 7) == 0) {return X_LYDIAN;}
      if (memcmp(src, "xx-Cham", 7) == 0) {return X_CHAM;}
    }
  }
  // Some other weird ones
  // Could be Latn or Limb; all our current training data is Latn
  if (strcmp(src, "sit-NP") == 0) {return LIMBU;}
  if (strcmp(src, "un-Latn") == 0) {return UNKNOWN_LANGUAGE;}

  // Multi-country langauges
  if (memcmp(src, "zh", 2) == 0) {
    if (memcmp(&src[len - 2], "TW", 2) == 0) {return CHINESE_T;}
    if (memcmp(&src[len - 2], "HK", 2) == 0) {return CHINESE_T;}
    return CHINESE;
  }
  if (memcmp(src, "pt", 2) == 0) {
    if (memcmp(&src[len - 2], "BR", 2) == 0) {return PORTUGUESE;}
    return PORTUGUESE;
  }
  if (memcmp(src, "fr", 2) == 0) {
    if (memcmp(&src[len -2], "CA", 2) == 0) {return FRENCH;}
    return FRENCH;
  }

  // None of the special cases matched
  if (src[2] == '-') {
    char temp[4];
    memcpy(temp, src, 4);
    temp[2] = '\0';
    LanguageFromCode(temp, &retlang);
  }
  if (src[3] == '-') {
    char temp[4];
    memcpy(temp, src, 4);
    temp[3] = '\0';
    LanguageFromCode(temp, &retlang);
  }
  if (retlang != UNKNOWN_LANGUAGE) {
    return retlang;
  }

  return retlang;
}

typedef struct {
  const char* name;
  UnicodeLScript lscript;
} NameScriptPair;

// In alphabetic order for binary search
static const NameScriptPair kNameScriptPair[] = {
  // Unicode 5.1 additional scripts
  {"Arab", ULScript_Arabic},
  {"Armn", ULScript_Armenian},
  {"Bali", ULScript_Balinese},
  {"Beng", ULScript_Bengali},
  {"Bugi", ULScript_Buginese},
  {"Buhd", ULScript_Buhid},
  {"Cans", ULScript_Canadian_Aboriginal},
  {"Cari", ULScript_Carian},      // Unicode 5.1
  {"Cham", ULScript_Cham},        // Unicode 5.1
  {"Cher", ULScript_Cherokee},
  {"Copt", ULScript_Coptic},
  {"Cprt", ULScript_Cypriot},
  {"Cyrl", ULScript_Cyrillic},
  {"Deva", ULScript_Devanagari},
  {"Dsrt", ULScript_Deseret},
  {"Ethi", ULScript_Ethiopic},
  {"Geor", ULScript_Georgian},
  {"Glag", ULScript_Glagolitic},
  {"Goth", ULScript_Gothic},
  {"Grek", ULScript_Greek},
  {"Gujr", ULScript_Gujarati},
  {"Guru", ULScript_Gurmukhi},
  {"Hani", ULScript_HanCJK},
  {"Hano", ULScript_Hanunoo},
  {"Hebr", ULScript_Hebrew},
  {"Ital", ULScript_Old_Italic},
  {"Kali", ULScript_Kayah_Li},    // Unicode 5.1
  {"Khar", ULScript_Kharoshthi},
  {"Khmr", ULScript_Khmer},
  {"Knda", ULScript_Kannada},
  {"Laoo", ULScript_Lao},
  {"Latn", ULScript_Latin},
  {"Lepc", ULScript_Lepcha},      // Unicode 5.1
  {"Limb", ULScript_Limbu},
  {"Linb", ULScript_Linear_B},
  {"Lyci", ULScript_Lycian},      // Unicode 5.1
  {"Lydi", ULScript_Lydian},      // Unicode 5.1
  {"Mlym", ULScript_Malayalam},
  {"Mong", ULScript_Mongolian},
  {"Mymr", ULScript_Myanmar},
  {"Nkoo", ULScript_Nko},
  {"Ogam", ULScript_Ogham},
  {"Olck", ULScript_Ol_Chiki},    // Unicode 5.1
  {"Orya", ULScript_Oriya},
  {"Osma", ULScript_Osmanya},
  {"Phag", ULScript_Phags_Pa},
  {"Phnx", ULScript_Phoenician},
  {"Rjng", ULScript_Rejang},      // Unicode 5.1
  {"Runr", ULScript_Runic},
  {"Saur", ULScript_Saurashtra},  // Unicode 5.1
  {"Shaw", ULScript_Shavian},
  {"Sinh", ULScript_Sinhala},
  {"Sund", ULScript_Sundanese},   // Unicode 5.1
  {"Sylo", ULScript_Syloti_Nagri},
  {"Syrc", ULScript_Syriac},
  {"Tagb", ULScript_Tagbanwa},
  {"Tale", ULScript_Tai_Le},
  {"Talu", ULScript_New_Tai_Lue},
  {"Taml", ULScript_Tamil},
  {"Telu", ULScript_Telugu},
  {"Tfng", ULScript_Tifinagh},
  {"Tglg", ULScript_Tagalog},
  {"Thaa", ULScript_Thaana},
  {"Thai", ULScript_Thai},
  {"Tibt", ULScript_Tibetan},
  {"Ugar", ULScript_Ugaritic},
  {"Vaii", ULScript_Vai},         // Unicode 5.1 // NOTE: apparently 'Vai '
  {"Xpeo", ULScript_Old_Persian},
  {"Xsux", ULScript_Cuneiform},
  {"Yiii", ULScript_Yi},
  {"Zyyy", ULScript_Common},
  {"Zzzz", ULScript_Inherited},
};

// Convert "en-Latn-GB" to ULScript_Latin
UnicodeLScript GetLScriptFromNumberOrName(const char* src) {
  if (strspn(src, "0123456789") == strlen(src)) {
    // All digits
    return static_cast<UnicodeLScript>(strto32(src, NULL, 10));
  }

  if (strcmp(src, "zh-TW") == 0) {return ULScript_HanCJK;}
  if (strcmp(src, "zh-CN") == 0) {return ULScript_HanCJK;}
  if (strcmp(src, "pt-BR") == 0) {return ULScript_Latin;}
  if (strcmp(src, "pt-PT") == 0) {return ULScript_Latin;}
  // Could be Latn or Limb; all our current training data is Latn
  if (strcmp(src, "sit-NP") == 0) {return ULScript_Latin;}

  // Isolate just the script field
  char temp[5];
  const char* src2 = strchr(src, '-');
  if (src2 == NULL) {return ULScript_Latin;}
  src2 += 1;      // over the -
  memcpy(temp, src2, 4);
  temp[4] = '\0';

  int lo = 0;
  int hi = ULScript_NUM_SCRIPTS;
  while (lo < hi) {
    int mid = (lo + hi) >> 1;
    if (strcmp(temp, kNameScriptPair[mid].name) < 0) {
      hi = mid;
    } else if (strcmp(temp, kNameScriptPair[mid].name) > 0) {
      lo = mid + 1;
    } else {
      return kNameScriptPair[mid].lscript;
    }
  }
  return ULScript_Latin;
}


// Merge together some languages, such as bo/hr/sr
// Croatian Latin and Serbian Cyrillic now.
Language NormalizeLanguage(Language lang) {
  if (lang == BOSNIAN) {return CROATIAN;}
  if (lang == SERBO_CROATIAN) {return SERBIAN;}

  if (lang == PORTUGUESE_P) {return PORTUGUESE;}
  if (lang == PORTUGUESE_B) {return PORTUGUESE;}

  return lang;
}
