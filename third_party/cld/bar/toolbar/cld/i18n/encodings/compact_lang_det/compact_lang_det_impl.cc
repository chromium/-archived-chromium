// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <string>

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/lang_enc.h"

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_impl.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/getonescriptspan.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/letterscript_enum.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/tote.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/utf8propjustletter.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/utf8propletterscriptnum.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/utf8scannotjustletterspecial.h"

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/cldutil_dbg.h"

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_basictypes.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_commandlineflags.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_google.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8statetable.h"

// Linker supplies the right tables
extern UTF8PropObj compact_lang_det_generated_ctjkvz_b1_obj;
extern const cld::CLDTableSummary kCjkBiTable_obj;
extern cld::CLDTableSummary kQuadTable_obj;
extern const cld::CLDTableSummary kLongWord8Table_obj;

DEFINE_bool(cld_html, false, "Print language spans in HTML on stderr");
DEFINE_bool(cld_forcewords, false, "Score all words, in addition to quads");

DEFINE_bool(cld_showme, false, "Put squeeze/repeat points into HTML text");
DEFINE_bool(cld_echotext, false, "Print each scriptspan to stderr");
DEFINE_int32(cld_textlimit, 160, "Examine only initial n KB of actual text");
// 20 quadgrams is about 80 bytes or about 12 words in real text
DEFINE_int32(cld_smoothwidth, 20, "Smoothing window width in quadgrams");


static const int kLangHintInitial = 12;  // Boost language by N initially
static const int kLangHintBoost = 12;    // Boost language by N/16 per quadgram

static const int kShortSpanThresh = 32;       // Bytes
static const int kMaxSecondChanceLen = 1024;  // Look at first 1K of short spans

static const int kCheapSqueezeTestThresh = 4096;  // Only look for squeezing
                                                  // after this many text bytes
static const int kCheapSqueezeTestLen = 256;   // Bytes to test to trigger sqz
static const int kSpacesTriggerPercent = 25;   // Trigger sqz if >=25% spaces
static const int kPredictTriggerPercent = 67;  // Trigger sqz if >=67% predicted

static const int kChunksizeDefault = 48;      // Squeeze 48-byte chunks
static const int kSpacesThreshPercent = 25;   // Squeeze if >=25% spaces
static const int kPredictThreshPercent = 40;  // Squeeze if >=40% predicted

static const int kMaxSpaceScan = 32;          // Bytes

static const int kGoodLang1Percent = 70;
static const int kGoodLang1and2Percent = 93;
static const int kShortTextThresh = 256;      // Bytes

static const int kMinChunkSizeQuads = 4;      // Chunk is at least four quads
static const int kMaxChunkSizeQuads = 1024;   // Chunk is at most 1K quads

static const int kDefaultWordSpan = 256;      // Scan at least this many initial
                                              // bytes with word scoring
static const int kReallyBigWordSpan = 9999999;  // Forces word scoring all text

static const int kMinReliableSeq = 50;      // Record in seq if >= 50% reliable

static const int kPredictionTableSize = 4096;   // Must be exactly 4096 for
                                                // cheap compressor

// Three packed language probs, subscripted by Encoding
static const uint32 kEncodingHintProbs[] = {
  0x00000000,  // ASCII
  0x18120cd5,  // Latin2   POLISH.11 CZECH.5 HUNGARIAN.3
  0x1d3a4bc9,  // Latin3   AZERBAIJANI.10 BASQUE.3 CROATIAN.1
  0x030819d4,  // Latin4   ESTONIAN.11 ITALIAN.4 DUTCH.2
  0x00000000,  // ISO-8859-5
  0x00003742,  // Arabic   ARABIC.12
  0x00000000,  // Greek
  0x00000742,  // Hebrew   HEBREW.12
  0x00002242,  // Latin5   TURKISH.12
  0x060419c9,  // Latin6   ESTONIAN.10 FINNISH.3 GERMAN.1
  0x00000942,  // EUC-JP   Japanese.12
  0x00000942,  // SJS   Japanese.12
  0x00000942,  // JIS   Japanese.12
  0x00004642,  // BIG5   ChineseT.12
  0x00001142,  // GB   Chinese.12
  0x46295fcd,  // EUC-CN   UIGHUR.10 MALAY.6 ChineseT.5
  0x00000a42,  // KSC   Korean.12
  0x00000000,  // Unicode
  0x03104674,  // EUC   ChineseT.9 SWEDISH.8 DUTCH.3
  0x00000000,  // CNS
  0x0f1146c3,  // BIG5-CP950   ChineseT.9 Chinese.5 SPANISH.4
  0x00000942,  // CP932   Japanese.12
  0x00000000,  // UTF8
  0x00000000,  // Unknown
  0x00000000,  // ASCII-7-bit
  0x00000000,  // KOI8R
  0x00000000,  // CP1251
  0x00000000,  // CP1252
  0x00000000,  // KOI8U
  0x451d12cd,  // CP1250   CZECH.10 CROATIAN.6 SLOVAK.5
  0x0d06052a,  // ISO-8859-15   FRENCH.9 GERMAN.8 PORTUGUESE.7
  0x00002242,  // CP1254   TURKISH.12
  0x191516be,  // CP1257   LITHUANIAN.8 LATVIAN.7 ESTONIAN.7
  0x08003642,  // ISO-8859-11   THAI.12 ITALIAN.1
  0x00000000,  // CP874
  0x00003742,  // CP1256   ARABIC.12
  0x00000742,  // CP1255   HEBREW.12
  0x00000000,  // ISO-8859-8-I
  0x00000000,  // VISUAL
  0x00000000,  // CP852
  0x39001242,  // CSN_369103   CZECH.12 ESPERANTO.1
  0x00000000,  // CP1253
  0x00000000,  // CP866
  0x2e001944,  // ISO-8859-13   ESTONIAN.12 ALBANIAN.3
  0x08090a74,  // ISO-2022-KR   Korean.9 Japanese.8 ITALIAN.3
  0x00001142,  // GBK   Chinese.12
  0x4600113d,  // GB18030   Chinese.11 ChineseT.7
  0x00004642,  // BIG5_HKSCS   ChineseT.12
  0x00000000,  // ISO_2022_CN
  0x00000000,  // TSCII
  0x00000000,  // TAM
  0x00000000,  // TAB
  0x00000000,  // JAGRAN
  0x00000000,  // MACINTOSH
  0x00000000,  // UTF7
  0x00000000,  // BHASKAR
  0x00000000,  // HTCHANAKYA
  0x090646ca,  // UTF-16BE   ChineseT.10 GERMAN.4 Japanese.2
  0x00000000,  // UTF-16LE
  0x00000000,  // UTF-32BE
  0x00000000,  // UTF-32LE
  0x00000000,  // X-BINARYENC
  0x06001142,  // HZ-GB-2312   Chinese.12 GERMAN.1
  0x461109c2,  // X-UTF8UTF8   Japanese.9 Chinese.5 ChineseT.3
  0x00000000,  // X-TAM-ELANGO
  0x00000000,  // X-TAM-LTTMBARANI
  0x00000000,  // X-TAM-SHREE
  0x00000000,  // X-TAM-TBOOMIS
  0x00000000,  // X-TAM-TMNEWS
  0x00000000,  // X-TAM-WEBTAMIL
  0x00000000,  // X-KDDI-Shift_JIS
  0x00000000,  // X-DoCoMo-Shift_JIS
  0x00000000,  // X-SoftBank-Shift_JIS
  0x00000000,  // X-KDDI-ISO-2022-JP
  0x00000000,  // X-SoftBank-ISO-2022-JP
};

COMPILE_ASSERT(arraysize(kEncodingHintProbs) == NUM_ENCODINGS,
               kEncodingHintProbs_has_incorrect_size);

// Three packed language probs, subscripted by (anchor) language
static const uint32 kLanguageHintProbs[] = {
  0x00000000,  // ENGLISH
  0x00000242,  // DANISH   DANISH.12
  0x00000342,  // DUTCH   DUTCH.12
  0x00000442,  // FINNISH   FINNISH.12
  0x00000542,  // FRENCH   FRENCH.12
  0x00000642,  // GERMAN   GERMAN.12
  0x00000742,  // HEBREW   HEBREW.12
  0x00000842,  // ITALIAN   ITALIAN.12
  0x00000942,  // Japanese   Japanese.12
  0x00000a42,  // Korean   Korean.12
  0x51000b43,  // NORWEGIAN   NORWEGIAN.12 NORWEGIAN_N.2
  0x00000c42,  // POLISH   POLISH.12
  0x00000d42,  // PORTUGUESE   PORTUGUESE.12
  0x00000000,  // RUSSIAN
  0x00000f42,  // SPANISH   SPANISH.12
  0x00001042,  // SWEDISH   SWEDISH.12
  0x00001142,  // Chinese   Chinese.12
  0x00001242,  // CZECH   CZECH.12
  0x00000000,  // GREEK
  0x47001442,  // ICELANDIC   ICELANDIC.12 FAROESE.1
  0x00001542,  // LATVIAN   LATVIAN.12
  0x00001642,  // LITHUANIAN   LITHUANIAN.12
  0x00001742,  // ROMANIAN   ROMANIAN.12
  0x00001842,  // HUNGARIAN   HUNGARIAN.12
  0x00001942,  // ESTONIAN   ESTONIAN.12
  0x00000000,  // TG_UNKNOWN_LANGUAGE
  0x00000000,  // Unknown
  0x00001c42,  // BULGARIAN   BULGARIAN.12
  0x00001d42,  // CROATIAN   CROATIAN.12
  0x1e001d46,  // SERBIAN   CROATIAN.12 SERBIAN.5
  0x00000000,  // IRISH
  0x0f00203d,  // GALICIAN   GALICIAN.11 SPANISH.7
  0x5e00213a,  // TAGALOG   TAGALOG.11 SOMALI.4
  0x00002242,  // TURKISH   TURKISH.12
  0x00002342,  // UKRAINIAN   UKRAINIAN.12
  0x00000000,  // HINDI
  0x1c1e25d4,  // MACEDONIAN   MACEDONIAN.11 SERBIAN.4 BULGARIAN.2
  0x00002642,  // BENGALI   BENGALI.12
  0x00002742,  // INDONESIAN   INDONESIAN.12
  0x00000000,  // LATIN
  0x2700293c,  // MALAY   MALAY.11 INDONESIAN.6
  0x00000000,  // MALAYALAM
  0x00000000,  // WELSH
  0x00000000,  // NEPALI
  0x00000000,  // TELUGU
  0x00002e42,  // ALBANIAN   ALBANIAN.12
  0x00000000,  // TAMIL
  0x00003042,  // BELARUSIAN   BELARUSIAN.12
  0x00000000,  // JAVANESE
  0x00000000,  // OCCITAN
  0x375f3330,  // URDU   URDU.10 UIGHUR.7 ARABIC.4
  0x41003436,  // BIHARI   BIHARI.10 MARATHI.10
  0x00000000,  // GUJARATI
  0x0a4636b2,  // THAI   THAI.7 ChineseT.3 Korean.2
  0x00003742,  // ARABIC   ARABIC.12
  0x00003842,  // CATALAN   CATALAN.12
  0x00003942,  // ESPERANTO   ESPERANTO.12
  0x00003a42,  // BASQUE   BASQUE.12
  0x00000000,  // INTERLINGUA
  0x00000000,  // KANNADA
  0x05060cca,  // PUNJABI   POLISH.10 GERMAN.4 FRENCH.2
  0x00000000,  // SCOTS_GAELIC
  0x00003f42,  // SWAHILI   SWAHILI.12
  0x00004042,  // SLOVENIAN   SLOVENIAN.12
  0x00004142,  // MARATHI   MARATHI.12
  0x00004242,  // MALTESE   MALTESE.12
  0x00004342,  // VIETNAMESE   VIETNAMESE.12
  0x00000000,  // FRISIAN
  0x12004543,  // SLOVAK   SLOVAK.12 CZECH.2
  0x00004642,  // ChineseT   ChineseT.12
  0x00000000,  // FAROESE
  0x00000000,  // SUNDANESE
  0x79004944,  // UZBEK   UZBEK.12 TAJIK.3
  0x4d004a46,  // AMHARIC   AMHARIC.12 TIGRINYA.5
  0x00004b42,  // AZERBAIJANI   AZERBAIJANI.12
  0x00000000,  // GEORGIAN
  0x00000000,  // TIGRINYA
  0x00004e42,  // PERSIAN   PERSIAN.12
  0x00000000,  // BOSNIAN
  0x00000000,  // SINHALESE
  0x00000000,  // NORWEGIAN_N
  0x00000000,  // PORTUGUESE_P
  0x00000000,  // PORTUGUESE_B
  0x00000000,  // XHOSA
  0x00000000,  // ZULU
  0x00000000,  // GUARANI
  0x00000000,  // SESOTHO
  0x00000000,  // TURKMEN
  0x7a005933,  // KYRGYZ   KYRGYZ.10 TATAR.7
  0x00000000,  // BRETON
  0x00000000,  // TWI
  0x00000000,  // YIDDISH
  0x00000000,  // SERBO_CROATIAN
  0x00000000,  // SOMALI
  0x00005f42,  // UIGHUR   UIGHUR.12
  0x00006042,  // KURDISH   KURDISH.12
  0x00006142,  // MONGOLIAN   MONGOLIAN.12
  0x051130c9,  // ARMENIAN   BELARUSIAN.10 Chinese.3 FRENCH.1
  0x020f0521,  // LAOTHIAN   FRENCH.8 SPANISH.7 DANISH.6
  0x64004e35,  // SINDHI   PERSIAN.10 SINDHI.9
  0x00000000,  // RHAETO_ROMANCE
  0x00006642,  // AFRIKAANS   AFRIKAANS.12
  0x00000000,  // LUXEMBOURGISH
  0x00006842,  // BURMESE   BURMESE.12
  0x00002242,  // KHMER   TURKISH.12
  0x88006a3c,  // TIBETAN   TIBETAN.11 DZONGKHA.6
  0x00000000,  // DHIVEHI
  0x00000000,  // CHEROKEE
  0x00000000,  // SYRIAC
  0x00000000,  // LIMBU
  0x00000000,  // ORIYA
  0x00000000,  // ASSAMESE
  0x00000000,  // CORSICAN
  0x00000000,  // INTERLINGUE
  0x00007342,  // KAZAKH   KAZAKH.12
  0x00000000,  // LINGALA
  0x00000000,  // MOLDAVIAN
  0x5f007645,  // PASHTO   PASHTO.12 UIGHUR.4
  0x00000000,  // QUECHUA
  0x00000000,  // SHONA
  0x00007942,  // TAJIK   TAJIK.12
  0x00000000,  // TATAR
  0x00000000,  // TONGA
  0x00000000,  // YORUBA
  0x00000000,  // CREOLES_AND_PIDGINS_ENGLISH_BASED
  0x00000000,  // CREOLES_AND_PIDGINS_FRENCH_BASED
  0x00000000,  // CREOLES_AND_PIDGINS_PORTUGUESE_BASED
  0x00000000,  // CREOLES_AND_PIDGINS_OTHER
  0x00000000,  // MAORI
  0x00000000,  // WOLOF
  0x00000000,  // ABKHAZIAN
  0x00000000,  // AFAR
  0x00000000,  // AYMARA
  0x00000000,  // BASHKIR
  0x00000000,  // BISLAMA
  0x00000000,  // DZONGKHA
  0x00000000,  // FIJIAN
  0x00000000,  // GREENLANDIC
  0x00000000,  // HAUSA
  0x00000000,  // HAITIAN_CREOLE
  0x00000000,  // INUPIAK
  0x00000542,  // INUKTITUT   FRENCH.12
  0x00000000,  // KASHMIRI
  0x00000000,  // KINYARWANDA
  0x00000000,  // MALAGASY
  0x00000000,  // NAURU
  0x00000000,  // OROMO
  0x00000000,  // RUNDI
  0x00000000,  // SAMOAN
  0x00000000,  // SANGO
  0x344197d3,  // SANSKRIT   SANSKRIT.11 MARATHI.4 BIHARI.1
  0x00000000,  // SISWANT
  0x00000000,  // TSONGA
  0x00000000,  // TSWANA
  0x00000000,  // VOLAPUK
  0x00000000,  // ZHUANG
  0x00000000,  // KHASI
  0x00000000,  // SCOTS
  0x00000000,  // GANDA
  0x00000000,  // MANX
  0x00000000,  // MONTENEGRIN
  // Add new language hints just before here (just use 0x00000000)
};

COMPILE_ASSERT(arraysize(kLanguageHintProbs) == NUM_LANGUAGES,
               kLanguageHintProbs_has_incorrect_size);

typedef struct {
  char key[4];
  uint32 probs;
} HintEntry;


// Massaged TLD, followed by three packed language probs
static const int kTLDHintProbsSize = 201;
static const HintEntry kTLDHintProbs[kTLDHintProbsSize] = {   // MaxRange 12
  {{0x61,0x63,0x5f,0x5f}, 0x0a000945},   // ac__ Japanese.12 Korean.4
  {{0x61,0x64,0x5f,0x5f}, 0x00003842},   // ad__ CATALAN.12
  {{0x61,0x65,0x5f,0x5f}, 0x00003742},   // ae__ ARABIC.12
  {{0x61,0x66,0x5f,0x5f}, 0x4e00763d},   // af__ PASHTO.11 PERSIAN.7
  {{0x61,0x67,0x5f,0x5f}, 0x09000643},   // ag__ GERMAN.12 Japanese.2
  {{0x61,0x69,0x5f,0x5f}, 0x0c180938},   // ai__ Japanese.11 HUNGARIAN.7 POLISH.2
  {{0x61,0x6c,0x5f,0x5f}, 0x00002e42},   // al__ ALBANIAN.12
  {{0x61,0x6e,0x5f,0x5f}, 0x6e00033d},   // an__ DUTCH.11 LIMBU.7
  {{0x61,0x6f,0x5f,0x5f}, 0x05000d42},   // ao__ PORTUGUESE.12 FRENCH.1
  {{0x61,0x71,0x5f,0x5f}, 0x05000f29},   // aq__ SPANISH.9 FRENCH.6
  {{0x61,0x72,0x5f,0x5f}, 0x00000f42},   // ar__ SPANISH.12
  {{0x61,0x73,0x5f,0x5f}, 0x0f120bcd},   // as__ NORWEGIAN.10 CZECH.6 SPANISH.5
  {{0x61,0x74,0x5f,0x5f}, 0x00000642},   // at__ GERMAN.12
  {{0x61,0x77,0x5f,0x5f}, 0x0f000345},   // aw__ DUTCH.12 SPANISH.4
  {{0x61,0x78,0x5f,0x5f}, 0x00001042},   // ax__ SWEDISH.12
  {{0x61,0x7a,0x5f,0x5f}, 0x00004b42},   // az__ AZERBAIJANI.12
  {{0x62,0x61,0x5f,0x5f}, 0x00001d42},   // ba__ CROATIAN.12
  {{0x62,0x62,0x5f,0x5f}, 0x00002842},   // bb__ LATIN.12
  {{0x62,0x64,0x5f,0x5f}, 0x00002642},   // bd__ BENGALI.12
  {{0x62,0x65,0x5f,0x5f}, 0x05000335},   // be__ DUTCH.10 FRENCH.9
  {{0x62,0x66,0x5f,0x5f}, 0x00000542},   // bf__ FRENCH.12
  {{0x62,0x67,0x5f,0x5f}, 0x00001c42},   // bg__ BULGARIAN.12
  {{0x62,0x68,0x5f,0x5f}, 0x00003742},   // bh__ ARABIC.12
  {{0x62,0x69,0x5f,0x5f}, 0x0f00053f},   // bi__ FRENCH.11 SPANISH.9
  {{0x62,0x6a,0x5f,0x5f}, 0x00000542},   // bj__ FRENCH.12
  {{0x62,0x6d,0x5f,0x5f}, 0x98043929},   // bm__ ESPERANTO.9 FINNISH.8 SISWANT.6
  {{0x62,0x6e,0x5f,0x5f}, 0x00002942},   // bn__ MALAY.12
  {{0x62,0x6f,0x5f,0x5f}, 0x00000f42},   // bo__ SPANISH.12
  {{0x62,0x72,0x5f,0x5f}, 0x00000d42},   // br__ PORTUGUESE.12
  {{0x62,0x74,0x5f,0x5f}, 0x00008842},   // bt__ DZONGKHA.12
  {{0x62,0x77,0x5f,0x5f}, 0x06059ac4},   // bw__ TSWANA.9 FRENCH.6 GERMAN.5
  {{0x62,0x79,0x5f,0x5f}, 0x00003024},   // by__ BELARUSIAN.9
  {{0x62,0x7a,0x5f,0x5f}, 0x0f0a0924},   // bz__ Japanese.9 Korean.5 SPANISH.1
  {{0x63,0x61,0x5f,0x5f}, 0x00000542},   // ca__ FRENCH.12
  {{0x63,0x61,0x74,0x5f}, 0x00003842},   // cat_ CATALAN.12
  {{0x63,0x64,0x5f,0x5f}, 0x06051224},   // cd__ CZECH.9 FRENCH.5 GERMAN.1
  {{0x63,0x66,0x5f,0x5f}, 0x00000542},   // cf__ FRENCH.12
  {{0x63,0x67,0x5f,0x5f}, 0x00000542},   // cg__ FRENCH.12
  {{0x63,0x68,0x5f,0x5f}, 0x08050638},   // ch__ GERMAN.11 FRENCH.7 ITALIAN.2
  {{0x63,0x69,0x5f,0x5f}, 0x00000542},   // ci__ FRENCH.12
  {{0x63,0x6c,0x5f,0x5f}, 0x00000f42},   // cl__ SPANISH.12
  {{0x63,0x6d,0x5f,0x5f}, 0x00000542},   // cm__ FRENCH.12
  {{0x63,0x6e,0x5f,0x5f}, 0x00001142},   // cn__ Chinese.12
  {{0x63,0x6f,0x5f,0x5f}, 0x00000f42},   // co__ SPANISH.12
// {{0x63,0x6f,0x6f,0x70}, 0x0f0509cd},   // coop Japanese.10 FRENCH.6 SPANISH.5
  {{0x63,0x72,0x5f,0x5f}, 0x00000f42},   // cr__ SPANISH.12
  {{0x63,0x75,0x5f,0x5f}, 0x00000f42},   // cu__ SPANISH.12
  {{0x63,0x76,0x5f,0x5f}, 0x00000d42},   // cv__ PORTUGUESE.12
  {{0x63,0x78,0x5f,0x5f}, 0x223a091f},   // cx__ Japanese.8 BASQUE.6 TURKISH.4
  {{0x63,0x79,0x5f,0x5f}, 0x150622ba},   // cy__ TURKISH.8 GERMAN.4 LATVIAN.3
  {{0x63,0x7a,0x5f,0x5f}, 0x00001242},   // cz__ CZECH.12
  {{0x64,0x65,0x5f,0x5f}, 0x00000642},   // de__ GERMAN.12
  {{0x64,0x6b,0x5f,0x5f}, 0x00000242},   // dk__ DANISH.12
  {{0x64,0x6f,0x5f,0x5f}, 0x21000f42},   // do__ SPANISH.12 TAGALOG.1
  {{0x64,0x7a,0x5f,0x5f}, 0x37000535},   // dz__ FRENCH.10 ARABIC.9
  {{0x65,0x63,0x5f,0x5f}, 0x00000f42},   // ec__ SPANISH.12
// {{0x65,0x64,0x75,0x5f}, 0x2e0f3873},   // edu_ CATALAN.9 SPANISH.7 ALBANIAN.2
  {{0x65,0x65,0x5f,0x5f}, 0x00001942},   // ee__ ESTONIAN.12
  {{0x65,0x67,0x5f,0x5f}, 0x05003742},   // eg__ ARABIC.12 FRENCH.1
  {{0x65,0x72,0x5f,0x5f}, 0x00000b42},   // er__ NORWEGIAN.12
  {{0x65,0x73,0x5f,0x5f}, 0x38200fd4},   // es__ SPANISH.11 GALICIAN.4 CATALAN.2
  {{0x65,0x74,0x5f,0x5f}, 0x39004a39},   // et__ AMHARIC.11 ESPERANTO.3
  {{0x66,0x69,0x5f,0x5f}, 0x10000444},   // fi__ FINNISH.12 SWEDISH.3
  {{0x66,0x6a,0x5f,0x5f}, 0x050489e0},   // fj__ FIJIAN.12 FINNISH.5 FRENCH.3
  {{0x66,0x6f,0x5f,0x5f}, 0x00004742},   // fo__ FAROESE.12
  {{0x66,0x72,0x5f,0x5f}, 0x00000542},   // fr__ FRENCH.12
  {{0x67,0x61,0x5f,0x5f}, 0x00000542},   // ga__ FRENCH.12
  {{0x67,0x64,0x5f,0x5f}, 0x061d05d5},   // gd__ FRENCH.11 CROATIAN.5 GERMAN.3
  {{0x67,0x65,0x5f,0x5f}, 0x00004c2d},   // ge__ GEORGIAN.10
  {{0x67,0x66,0x5f,0x5f}, 0x00000542},   // gf__ FRENCH.12
  {{0x67,0x67,0x5f,0x5f}, 0x06002244},   // gg__ TURKISH.12 GERMAN.3
  {{0x67,0x68,0x5f,0x5f}, 0x05000436},   // gh__ FINNISH.10 FRENCH.10
  {{0x67,0x69,0x5f,0x5f}, 0x0f0538ce},   // gi__ CATALAN.10 FRENCH.7 SPANISH.6
  {{0x67,0x6c,0x5f,0x5f}, 0x398a0238},   // gl__ DANISH.11 GREENLANDIC.7 ESPERANTO.2
  {{0x67,0x6d,0x5f,0x5f}, 0x0600043e},   // gm__ FINNISH.11 GERMAN.8
  {{0x67,0x6e,0x5f,0x5f}, 0x00000542},   // gn__ FRENCH.12
// {{0x67,0x6f,0x76,0x5f}, 0x05000f25},   // gov_ SPANISH.9 FRENCH.2
  {{0x67,0x70,0x5f,0x5f}, 0x00000542},   // gp__ FRENCH.12
  {{0x67,0x71,0x5f,0x5f}, 0x0f000547},   // gq__ FRENCH.12 SPANISH.6
  {{0x67,0x73,0x5f,0x5f}, 0x00000942},   // gs__ Japanese.12
  {{0x67,0x74,0x5f,0x5f}, 0x00000f42},   // gt__ SPANISH.12
  {{0x68,0x6b,0x5f,0x5f}, 0x11004643},   // hk__ ChineseT.12 Chinese.2
  {{0x68,0x6d,0x5f,0x5f}, 0x4606092e},   // hm__ Japanese.10 GERMAN.6 ChineseT.2
  {{0x68,0x6e,0x5f,0x5f}, 0x00000f42},   // hn__ SPANISH.12
  {{0x68,0x72,0x5f,0x5f}, 0x00001d42},   // hr__ CROATIAN.12
  {{0x68,0x74,0x5f,0x5f}, 0x0f000542},   // ht__ FRENCH.12 SPANISH.1
  {{0x68,0x75,0x5f,0x5f}, 0x00001842},   // hu__ HUNGARIAN.12
  {{0x69,0x64,0x5f,0x5f}, 0x00002742},   // id__ INDONESIAN.12
  {{0x69,0x65,0x5f,0x5f}, 0x050c1f24},   // ie__ IRISH.9 POLISH.5 FRENCH.1
  {{0x69,0x6c,0x5f,0x5f}, 0x00000742},   // il__ HEBREW.12
  {{0x69,0x6e,0x74,0x5f}, 0x0f060574},   // int_ FRENCH.9 GERMAN.8 SPANISH.3
  {{0x69,0x6f,0x5f,0x5f}, 0x11090fd5},   // io__ SPANISH.11 Japanese.5 Chinese.3
  {{0x69,0x71,0x5f,0x5f}, 0x60003744},   // iq__ ARABIC.12 KURDISH.3
  {{0x69,0x72,0x5f,0x5f}, 0x00004e42},   // ir__ PERSIAN.12
  {{0x69,0x73,0x5f,0x5f}, 0x00001442},   // is__ ICELANDIC.12
  {{0x69,0x74,0x5f,0x5f}, 0x00000842},   // it__ ITALIAN.12
  {{0x6a,0x65,0x5f,0x5f}, 0x29050328},   // je__ DUTCH.9 FRENCH.7 MALAY.5
  {{0x6a,0x6d,0x5f,0x5f}, 0x040f0576},   // jm__ FRENCH.9 SPANISH.8 FINNISH.5
  {{0x6a,0x6f,0x5f,0x5f}, 0x00003742},   // jo__ ARABIC.12
// {{0x6a,0x6f,0x62,0x73}, 0x0f060329},   // jobs DUTCH.9 GERMAN.8 SPANISH.6
  {{0x6a,0x70,0x5f,0x5f}, 0x00000942},   // jp__ Japanese.12
  {{0x6b,0x65,0x5f,0x5f}, 0x040f3fc3},   // ke__ SWAHILI.9 SPANISH.5 FINNISH.4
  {{0x6b,0x69,0x5f,0x5f}, 0x04000643},   // ki__ GERMAN.12 FINNISH.2
  {{0x6b,0x6d,0x5f,0x5f}, 0x00000542},   // km__ FRENCH.12
  {{0x6b,0x70,0x5f,0x5f}, 0x00000a42},   // kp__ Korean.12
  {{0x6b,0x72,0x5f,0x5f}, 0x00000a42},   // kr__ Korean.12
  {{0x6b,0x77,0x5f,0x5f}, 0x00003742},   // kw__ ARABIC.12
  {{0x6b,0x79,0x5f,0x5f}, 0x0500083f},   // ky__ ITALIAN.11 FRENCH.9
  {{0x6b,0x7a,0x5f,0x5f}, 0x0000732d},   // kz__ KAZAKH.10
  {{0x6c,0x62,0x5f,0x5f}, 0x05003747},   // lb__ ARABIC.12 FRENCH.6
  {{0x6c,0x63,0x5f,0x5f}, 0x09000645},   // lc__ GERMAN.12 Japanese.4
  {{0x6c,0x69,0x5f,0x5f}, 0x1600063d},   // li__ GERMAN.11 LITHUANIAN.7
  {{0x6c,0x73,0x5f,0x5f}, 0x00005742},   // ls__ SESOTHO.12
  {{0x6c,0x74,0x5f,0x5f}, 0x00001642},   // lt__ LITHUANIAN.12
  {{0x6c,0x75,0x5f,0x5f}, 0x0600053d},   // lu__ FRENCH.11 GERMAN.7
  {{0x6c,0x76,0x5f,0x5f}, 0x00001542},   // lv__ LATVIAN.12
  {{0x6c,0x79,0x5f,0x5f}, 0x05003744},   // ly__ ARABIC.12 FRENCH.3
  {{0x6d,0x61,0x5f,0x5f}, 0x3700053d},   // ma__ FRENCH.11 ARABIC.7
  {{0x6d,0x63,0x5f,0x5f}, 0x00000542},   // mc__ FRENCH.12
  {{0x6d,0x64,0x5f,0x5f}, 0x00001724},   // md__ ROMANIAN.9
  {{0x6d,0x65,0x5f,0x5f}, 0x00001d42},   // me__ CROATIAN.12
  {{0x6d,0x67,0x5f,0x5f}, 0x00000542},   // mg__ FRENCH.12
  {{0x6d,0x6b,0x5f,0x5f}, 0x1c002543},   // mk__ MACEDONIAN.12 BULGARIAN.2
  {{0x6d,0x6c,0x5f,0x5f}, 0x00000542},   // ml__ FRENCH.12
  {{0x6d,0x6e,0x5f,0x5f}, 0x00006142},   // mn__ MONGOLIAN.12
  {{0x6d,0x6f,0x5f,0x5f}, 0x110d4631},   // mo__ ChineseT.10 PORTUGUESE.8 Chinese.5
  {{0x6d,0x71,0x5f,0x5f}, 0x00000542},   // mq__ FRENCH.12
  {{0x6d,0x72,0x5f,0x5f}, 0x37000535},   // mr__ FRENCH.10 ARABIC.9
  {{0x6d,0x73,0x5f,0x5f}, 0x090f06d5},   // ms__ GERMAN.11 SPANISH.5 Japanese.3
  {{0x6d,0x74,0x5f,0x5f}, 0x00004242},   // mt__ MALTESE.12
  {{0x6d,0x75,0x5f,0x5f}, 0x05000934},   // mu__ Japanese.10 FRENCH.8
  {{0x6d,0x76,0x5f,0x5f}, 0x28000436},   // mv__ FINNISH.10 LATIN.10
  {{0x6d,0x77,0x5f,0x5f}, 0x0611092a},   // mw__ Japanese.9 Chinese.8 GERMAN.7
  {{0x6d,0x78,0x5f,0x5f}, 0x00000f42},   // mx__ SPANISH.12
  {{0x6d,0x79,0x5f,0x5f}, 0x00002942},   // my__ MALAY.12
  {{0x6d,0x7a,0x5f,0x5f}, 0x00000d42},   // mz__ PORTUGUESE.12
  {{0x6e,0x61,0x5f,0x5f}, 0x06006644},   // na__ AFRIKAANS.12 GERMAN.3
  {{0x6e,0x63,0x5f,0x5f}, 0x00000542},   // nc__ FRENCH.12
  {{0x6e,0x65,0x5f,0x5f}, 0x8b000542},   // ne__ FRENCH.12 HAUSA.1
  {{0x6e,0x66,0x5f,0x5f}, 0x00000542},   // nf__ FRENCH.12
  {{0x6e,0x69,0x5f,0x5f}, 0x00000f42},   // ni__ SPANISH.12
  {{0x6e,0x6c,0x5f,0x5f}, 0x00000342},   // nl__ DUTCH.12
  {{0x6e,0x6f,0x5f,0x5f}, 0x51000b43},   // no__ NORWEGIAN.12 NORWEGIAN_N.2
  {{0x6e,0x75,0x5f,0x5f}, 0x0300103b},   // nu__ SWEDISH.11 DUTCH.5
  {{0x6f,0x6d,0x5f,0x5f}, 0x00003742},   // om__ ARABIC.12
  {{0x70,0x61,0x5f,0x5f}, 0x00000f42},   // pa__ SPANISH.12
  {{0x70,0x65,0x5f,0x5f}, 0x00000f42},   // pe__ SPANISH.12
  {{0x70,0x66,0x5f,0x5f}, 0x00000542},   // pf__ FRENCH.12
  {{0x70,0x67,0x5f,0x5f}, 0x00000f24},   // pg__ SPANISH.9
  {{0x70,0x68,0x5f,0x5f}, 0x00002142},   // ph__ TAGALOG.12
  {{0x70,0x6b,0x5f,0x5f}, 0x00003342},   // pk__ URDU.12
  {{0x70,0x6c,0x5f,0x5f}, 0x30000c42},   // pl__ POLISH.12 BELARUSIAN.1
  {{0x70,0x6e,0x5f,0x5f}, 0x04000644},   // pn__ GERMAN.12 FINNISH.3
  {{0x70,0x72,0x5f,0x5f}, 0x00000f42},   // pr__ SPANISH.12
  {{0x70,0x72,0x6f,0x5f}, 0x46050fd5},   // pro_ SPANISH.11 FRENCH.5 ChineseT.3
  {{0x70,0x73,0x5f,0x5f}, 0x00003742},   // ps__ ARABIC.12
  {{0x70,0x74,0x5f,0x5f}, 0x00000d42},   // pt__ PORTUGUESE.12
  {{0x70,0x79,0x5f,0x5f}, 0x00000f42},   // py__ SPANISH.12
  {{0x71,0x61,0x5f,0x5f}, 0x00003742},   // qa__ ARABIC.12
  {{0x72,0x65,0x5f,0x5f}, 0x00000542},   // re__ FRENCH.12
  {{0x72,0x6f,0x5f,0x5f}, 0x00001742},   // ro__ ROMANIAN.12
  {{0x72,0x73,0x5f,0x5f}, 0x00001d42},   // rs__ CROATIAN.12
  {{0x72,0x77,0x5f,0x5f}, 0x9000053e},   // rw__ FRENCH.11 KINYARWANDA.8
  {{0x73,0x61,0x5f,0x5f}, 0x00003742},   // sa__ ARABIC.12
  {{0x73,0x62,0x5f,0x5f}, 0x00000442},   // sb__ FINNISH.12
  {{0x73,0x63,0x5f,0x5f}, 0x060f092f},   // sc__ Japanese.10 SPANISH.7 GERMAN.3
  {{0x73,0x64,0x5f,0x5f}, 0x00003742},   // sd__ ARABIC.12
  {{0x73,0x65,0x5f,0x5f}, 0x00001042},   // se__ SWEDISH.12
  {{0x73,0x69,0x5f,0x5f}, 0x00004042},   // si__ SLOVENIAN.12
  {{0x73,0x6b,0x5f,0x5f}, 0x12004543},   // sk__ SLOVAK.12 CZECH.2
  {{0x73,0x6d,0x5f,0x5f}, 0x00000842},   // sm__ ITALIAN.12
  {{0x73,0x6e,0x5f,0x5f}, 0x00000542},   // sn__ FRENCH.12
  {{0x73,0x72,0x5f,0x5f}, 0x03001e44},   // sr__ SERBIAN.12 DUTCH.3
  {{0x73,0x76,0x5f,0x5f}, 0x00000f42},   // sv__ SPANISH.12
  {{0x73,0x79,0x5f,0x5f}, 0x00003742},   // sy__ ARABIC.12
  {{0x74,0x63,0x5f,0x5f}, 0x0a2206cd},   // tc__ GERMAN.10 TURKISH.6 Korean.5
  {{0x74,0x66,0x5f,0x5f}, 0x00000642},   // tf__ GERMAN.12
  {{0x74,0x67,0x5f,0x5f}, 0x00000542},   // tg__ FRENCH.12
  {{0x74,0x68,0x5f,0x5f}, 0x9e0936c9},   // th__ THAI.10 Japanese.3 SCOTS.1
  {{0x74,0x6a,0x5f,0x5f}, 0x00007924},   // tj__ TAJIK.9
  {{0x74,0x6c,0x5f,0x5f}, 0x060f0dcd},   // tl__ PORTUGUESE.10 SPANISH.6 GERMAN.5
  {{0x74,0x6e,0x5f,0x5f}, 0x3700053e},   // tn__ FRENCH.11 ARABIC.8
  {{0x74,0x6f,0x5f,0x5f}, 0x064609c5},   // to__ Japanese.9 ChineseT.7 GERMAN.6
  {{0x74,0x70,0x5f,0x5f}, 0x06000944},   // tp__ Japanese.12 GERMAN.3
  {{0x74,0x72,0x5f,0x5f}, 0x00002242},   // tr__ TURKISH.12
  {{0x74,0x72,0x61,0x76}, 0x064509c3},   // trav Japanese.9 SLOVAK.5 GERMAN.4
  {{0x74,0x74,0x5f,0x5f}, 0x0f00063e},   // tt__ GERMAN.11 SPANISH.8
  {{0x74,0x77,0x5f,0x5f}, 0x00004642},   // tw__ ChineseT.12
  {{0x74,0x7a,0x5f,0x5f}, 0x00003f42},   // tz__ SWAHILI.12
  {{0x75,0x61,0x5f,0x5f}, 0x0000232d},   // ua__ UKRAINIAN.10
  {{0x75,0x79,0x5f,0x5f}, 0x00000f42},   // uy__ SPANISH.12
  {{0x75,0x7a,0x5f,0x5f}, 0x0000492d},   // uz__ UZBEK.10
  {{0x76,0x61,0x5f,0x5f}, 0x060f0828},   // va__ ITALIAN.9 SPANISH.7 GERMAN.5
  {{0x76,0x63,0x5f,0x5f}, 0x0d000939},   // vc__ Japanese.11 PORTUGUESE.3
  {{0x76,0x65,0x5f,0x5f}, 0x00000f42},   // ve__ SPANISH.12
  {{0x76,0x67,0x5f,0x5f}, 0x09000f43},   // vg__ SPANISH.12 Japanese.2
  {{0x76,0x69,0x5f,0x5f}, 0x00002942},   // vi__ MALAY.12
  {{0x76,0x6e,0x5f,0x5f}, 0x00004342},   // vn__ VIETNAMESE.12
  {{0x76,0x75,0x5f,0x5f}, 0x00000642},   // vu__ GERMAN.12
  {{0x77,0x73,0x5f,0x5f}, 0x4b0f0624},   // ws__ GERMAN.9 SPANISH.5 AZERBAIJANI.1
  {{0x79,0x65,0x5f,0x5f}, 0x00003742},   // ye__ ARABIC.12
  {{0x79,0x75,0x5f,0x5f}, 0x1e001d3d},   // yu__ CROATIAN.11 SERBIAN.7
  {{0x7a,0x61,0x5f,0x5f}, 0x00006642},   // za__ AFRIKAANS.12
  {{0x7a,0x6d,0x5f,0x5f}, 0x0b000435},   // zm__ FINNISH.10 NORWEGIAN.9
  {{0x7a,0x77,0x5f,0x5f}, 0x3f00783e},   // zw__ SHONA.11 SWAHILI.8
};


// Statistically closest language, based on quadgram table
// Those that are far from other languges map to UNKNOWN_LANGUAGE
// Subscripted by Language
//
// From lang_correlation.txt and hand-edits
// sed 's/^\([^ ]*\) \([^ ]*\) coef=0\.\(..\).*$/
//   (\3 >= kMinCorrPercent) ? \2 : UNKNOWN_LANGUAGE,
//   \/\/ \1/' lang_correlation.txt >/tmp/closest_lang_decl.txt
//
static const int kMinCorrPercent = 24;        // Pick off how close you want
                                              // 24 catches PERSIAN <== ARABIC
                                              // but not SPANISH <== PORTUGESE
static Language Unknown = UNKNOWN_LANGUAGE;

// Subscripted by Language
static const Language kClosestAltLanguage[] = {
  (28 >= kMinCorrPercent) ? SCOTS : UNKNOWN_LANGUAGE,  // ENGLISH
  (36 >= kMinCorrPercent) ? NORWEGIAN : UNKNOWN_LANGUAGE,  // DANISH
  (31 >= kMinCorrPercent) ? AFRIKAANS : UNKNOWN_LANGUAGE,  // DUTCH
  (15 >= kMinCorrPercent) ? ESTONIAN : UNKNOWN_LANGUAGE,  // FINNISH
  (11 >= kMinCorrPercent) ? OCCITAN : UNKNOWN_LANGUAGE,  // FRENCH
  (17 >= kMinCorrPercent) ? LUXEMBOURGISH : UNKNOWN_LANGUAGE,  // GERMAN
  (27 >= kMinCorrPercent) ? YIDDISH : UNKNOWN_LANGUAGE,  // HEBREW
  (16 >= kMinCorrPercent) ? CORSICAN : UNKNOWN_LANGUAGE,  // ITALIAN
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // Japanese
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // Korean
  (41 >= kMinCorrPercent) ? NORWEGIAN_N : UNKNOWN_LANGUAGE,  // NORWEGIAN
  ( 5 >= kMinCorrPercent) ? SLOVAK : UNKNOWN_LANGUAGE,  // POLISH
  (23 >= kMinCorrPercent) ? SPANISH : UNKNOWN_LANGUAGE,  // PORTUGUESE
  (33 >= kMinCorrPercent) ? BULGARIAN : UNKNOWN_LANGUAGE,  // RUSSIAN
  (28 >= kMinCorrPercent) ? GALICIAN : UNKNOWN_LANGUAGE,  // SPANISH
  (17 >= kMinCorrPercent) ? NORWEGIAN : UNKNOWN_LANGUAGE,  // SWEDISH
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // Chinese
  (42 >= kMinCorrPercent) ? SLOVAK : UNKNOWN_LANGUAGE,  // CZECH
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // GREEK
  (35 >= kMinCorrPercent) ? FAROESE : UNKNOWN_LANGUAGE,  // ICELANDIC
  ( 7 >= kMinCorrPercent) ? LITHUANIAN : UNKNOWN_LANGUAGE,  // LATVIAN
  ( 7 >= kMinCorrPercent) ? LATVIAN : UNKNOWN_LANGUAGE,  // LITHUANIAN
  ( 4 >= kMinCorrPercent) ? LATIN : UNKNOWN_LANGUAGE,  // ROMANIAN
  ( 4 >= kMinCorrPercent) ? SLOVAK : UNKNOWN_LANGUAGE,  // HUNGARIAN
  (15 >= kMinCorrPercent) ? FINNISH : UNKNOWN_LANGUAGE,  // ESTONIAN
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // Ignore
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // Unknown
  (33 >= kMinCorrPercent) ? RUSSIAN : UNKNOWN_LANGUAGE,  // BULGARIAN
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // CROATIAN
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // SERBIAN
  (24 >= kMinCorrPercent) ? SCOTS_GAELIC : UNKNOWN_LANGUAGE,  // IRISH
  (28 >= kMinCorrPercent) ? SPANISH : UNKNOWN_LANGUAGE,  // GALICIAN
  ( 8 >= kMinCorrPercent) ? INDONESIAN : UNKNOWN_LANGUAGE,  // TAGALOG
  (29 >= kMinCorrPercent) ? AZERBAIJANI : UNKNOWN_LANGUAGE,  // TURKISH
  (28 >= kMinCorrPercent) ? RUSSIAN : UNKNOWN_LANGUAGE,  // UKRAINIAN
  (37 >= kMinCorrPercent) ? MARATHI : UNKNOWN_LANGUAGE,  // HINDI
  (29 >= kMinCorrPercent) ? BULGARIAN : UNKNOWN_LANGUAGE,  // MACEDONIAN
  (14 >= kMinCorrPercent) ? ASSAMESE : UNKNOWN_LANGUAGE,  // BENGALI
  (46 >= kMinCorrPercent) ? MALAY : UNKNOWN_LANGUAGE,  // INDONESIAN
  ( 9 >= kMinCorrPercent) ? INTERLINGUA : UNKNOWN_LANGUAGE,  // LATIN
  (46 >= kMinCorrPercent) ? INDONESIAN : UNKNOWN_LANGUAGE,  // MALAY
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // MALAYALAM
  ( 4 >= kMinCorrPercent) ? BRETON : UNKNOWN_LANGUAGE,  // WELSH
  ( 8 >= kMinCorrPercent) ? HINDI : UNKNOWN_LANGUAGE,  // NEPALI
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // TELUGU
  ( 3 >= kMinCorrPercent) ? ESPERANTO : UNKNOWN_LANGUAGE,  // ALBANIAN
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // TAMIL
  (22 >= kMinCorrPercent) ? UKRAINIAN : UNKNOWN_LANGUAGE,  // BELARUSIAN
  (15 >= kMinCorrPercent) ? SUNDANESE : UNKNOWN_LANGUAGE,  // JAVANESE
  (19 >= kMinCorrPercent) ? CATALAN : UNKNOWN_LANGUAGE,  // OCCITAN
  (27 >= kMinCorrPercent) ? PERSIAN : UNKNOWN_LANGUAGE,  // URDU
  (36 >= kMinCorrPercent) ? HINDI : UNKNOWN_LANGUAGE,  // BIHARI
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // GUJARATI
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // THAI
  (24 >= kMinCorrPercent) ? PERSIAN : UNKNOWN_LANGUAGE,  // ARABIC
  (19 >= kMinCorrPercent) ? OCCITAN : UNKNOWN_LANGUAGE,  // CATALAN
  ( 4 >= kMinCorrPercent) ? LATIN : UNKNOWN_LANGUAGE,  // ESPERANTO
  ( 3 >= kMinCorrPercent) ? GERMAN : UNKNOWN_LANGUAGE,  // BASQUE
  ( 9 >= kMinCorrPercent) ? LATIN : UNKNOWN_LANGUAGE,  // INTERLINGUA
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // KANNADA
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // PUNJABI
  (24 >= kMinCorrPercent) ? IRISH : UNKNOWN_LANGUAGE,  // SCOTS_GAELIC
  ( 7 >= kMinCorrPercent) ? KINYARWANDA : UNKNOWN_LANGUAGE,  // SWAHILI
  (28 >= kMinCorrPercent) ? SERBO_CROATIAN : UNKNOWN_LANGUAGE,  // SLOVENIAN
  (37 >= kMinCorrPercent) ? HINDI : UNKNOWN_LANGUAGE,  // MARATHI
  ( 3 >= kMinCorrPercent) ? ITALIAN : UNKNOWN_LANGUAGE,  // MALTESE
  ( 1 >= kMinCorrPercent) ? YORUBA : UNKNOWN_LANGUAGE,  // VIETNAMESE
  (15 >= kMinCorrPercent) ? DUTCH : UNKNOWN_LANGUAGE,  // FRISIAN
  (42 >= kMinCorrPercent) ? CZECH : UNKNOWN_LANGUAGE,  // SLOVAK
  // Original ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // ChineseT
  (24 >= kMinCorrPercent) ? CHINESE : UNKNOWN_LANGUAGE,  // ChineseT
  (35 >= kMinCorrPercent) ? ICELANDIC : UNKNOWN_LANGUAGE,  // FAROESE
  (15 >= kMinCorrPercent) ? JAVANESE : UNKNOWN_LANGUAGE,  // SUNDANESE
  (17 >= kMinCorrPercent) ? TAJIK : UNKNOWN_LANGUAGE,  // UZBEK
  ( 7 >= kMinCorrPercent) ? TIGRINYA : UNKNOWN_LANGUAGE,  // AMHARIC
  (29 >= kMinCorrPercent) ? TURKISH : UNKNOWN_LANGUAGE,  // AZERBAIJANI
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // GEORGIAN
  ( 7 >= kMinCorrPercent) ? AMHARIC : UNKNOWN_LANGUAGE,  // TIGRINYA
  (27 >= kMinCorrPercent) ? URDU : UNKNOWN_LANGUAGE,  // PERSIAN
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // BOSNIAN
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // SINHALESE
  (41 >= kMinCorrPercent) ? NORWEGIAN : UNKNOWN_LANGUAGE,  // NORWEGIAN_N
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // PORTUGUESE_P
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // PORTUGUESE_B
  (37 >= kMinCorrPercent) ? ZULU : UNKNOWN_LANGUAGE,  // XHOSA
  (37 >= kMinCorrPercent) ? XHOSA : UNKNOWN_LANGUAGE,  // ZULU
  ( 2 >= kMinCorrPercent) ? SPANISH : UNKNOWN_LANGUAGE,  // GUARANI
  (29 >= kMinCorrPercent) ? TSWANA : UNKNOWN_LANGUAGE,  // SESOTHO
  ( 7 >= kMinCorrPercent) ? TURKISH : UNKNOWN_LANGUAGE,  // TURKMEN
  ( 8 >= kMinCorrPercent) ? KAZAKH : UNKNOWN_LANGUAGE,  // KYRGYZ
  ( 5 >= kMinCorrPercent) ? FRENCH : UNKNOWN_LANGUAGE,  // BRETON
  ( 3 >= kMinCorrPercent) ? GANDA : UNKNOWN_LANGUAGE,  // TWI
  (27 >= kMinCorrPercent) ? HEBREW : UNKNOWN_LANGUAGE,  // YIDDISH
  (28 >= kMinCorrPercent) ? SLOVENIAN : UNKNOWN_LANGUAGE,  // SERBO_CROATIAN
  (12 >= kMinCorrPercent) ? OROMO : UNKNOWN_LANGUAGE,  // SOMALI
  ( 9 >= kMinCorrPercent) ? UZBEK : UNKNOWN_LANGUAGE,  // UIGHUR
  (15 >= kMinCorrPercent) ? PERSIAN : UNKNOWN_LANGUAGE,  // KURDISH
  ( 6 >= kMinCorrPercent) ? KYRGYZ : UNKNOWN_LANGUAGE,  // MONGOLIAN
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // ARMENIAN
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // LAOTHIAN
  ( 8 >= kMinCorrPercent) ? URDU : UNKNOWN_LANGUAGE,  // SINDHI
  (10 >= kMinCorrPercent) ? ITALIAN : UNKNOWN_LANGUAGE,  // RHAETO_ROMANCE
  (31 >= kMinCorrPercent) ? DUTCH : UNKNOWN_LANGUAGE,  // AFRIKAANS
  (17 >= kMinCorrPercent) ? GERMAN : UNKNOWN_LANGUAGE,  // LUXEMBOURGISH
  ( 2 >= kMinCorrPercent) ? SCOTS : UNKNOWN_LANGUAGE,  // BURMESE
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // KHMER
  (45 >= kMinCorrPercent) ? DZONGKHA : UNKNOWN_LANGUAGE,  // TIBETAN
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // DHIVEHI
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // CHEROKEE
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // SYRIAC
  ( 8 >= kMinCorrPercent) ? DUTCH : UNKNOWN_LANGUAGE,  // LIMBU
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // ORIYA
  (14 >= kMinCorrPercent) ? BENGALI : UNKNOWN_LANGUAGE,  // ASSAMESE
  (16 >= kMinCorrPercent) ? ITALIAN : UNKNOWN_LANGUAGE,  // CORSICAN
  ( 5 >= kMinCorrPercent) ? INTERLINGUA : UNKNOWN_LANGUAGE,  // INTERLINGUE
  ( 8 >= kMinCorrPercent) ? KYRGYZ : UNKNOWN_LANGUAGE,  // KAZAKH
  ( 4 >= kMinCorrPercent) ? SWAHILI : UNKNOWN_LANGUAGE,  // LINGALA
  (11 >= kMinCorrPercent) ? RUSSIAN : UNKNOWN_LANGUAGE,  // MOLDAVIAN
  (19 >= kMinCorrPercent) ? PERSIAN : UNKNOWN_LANGUAGE,  // PASHTO
  ( 5 >= kMinCorrPercent) ? AYMARA : UNKNOWN_LANGUAGE,  // QUECHUA
  ( 5 >= kMinCorrPercent) ? KINYARWANDA : UNKNOWN_LANGUAGE,  // SHONA
  (17 >= kMinCorrPercent) ? UZBEK : UNKNOWN_LANGUAGE,  // TAJIK
  (13 >= kMinCorrPercent) ? BASHKIR : UNKNOWN_LANGUAGE,  // TATAR
  (11 >= kMinCorrPercent) ? SAMOAN : UNKNOWN_LANGUAGE,  // TONGA
  ( 2 >= kMinCorrPercent) ? TWI : UNKNOWN_LANGUAGE,  // YORUBA
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // CREOLES_AND_PIDGINS_ENGLISH_BASED
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // CREOLES_AND_PIDGINS_FRENCH_BASED
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // CREOLES_AND_PIDGINS_PORTUGUESE_BASED
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // CREOLES_AND_PIDGINS_OTHER
  ( 6 >= kMinCorrPercent) ? TONGA : UNKNOWN_LANGUAGE,  // MAORI
  ( 3 >= kMinCorrPercent) ? OROMO : UNKNOWN_LANGUAGE,  // WOLOF
  ( 1 >= kMinCorrPercent) ? MONGOLIAN : UNKNOWN_LANGUAGE,  // ABKHAZIAN
  ( 8 >= kMinCorrPercent) ? SOMALI : UNKNOWN_LANGUAGE,  // AFAR
  ( 5 >= kMinCorrPercent) ? QUECHUA : UNKNOWN_LANGUAGE,  // AYMARA
  (13 >= kMinCorrPercent) ? TATAR : UNKNOWN_LANGUAGE,  // BASHKIR
  ( 3 >= kMinCorrPercent) ? ENGLISH : UNKNOWN_LANGUAGE,  // BISLAMA
  (45 >= kMinCorrPercent) ? TIBETAN : UNKNOWN_LANGUAGE,  // DZONGKHA
  ( 4 >= kMinCorrPercent) ? TONGA : UNKNOWN_LANGUAGE,  // FIJIAN
  ( 7 >= kMinCorrPercent) ? INUPIAK : UNKNOWN_LANGUAGE,  // GREENLANDIC
  ( 3 >= kMinCorrPercent) ? AFAR : UNKNOWN_LANGUAGE,  // HAUSA
  ( 3 >= kMinCorrPercent) ? OCCITAN : UNKNOWN_LANGUAGE,  // HAITIAN_CREOLE
  ( 7 >= kMinCorrPercent) ? GREENLANDIC : UNKNOWN_LANGUAGE,  // INUPIAK
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // INUKTITUT
  ( 4 >= kMinCorrPercent) ? HINDI : UNKNOWN_LANGUAGE,  // KASHMIRI
  (30 >= kMinCorrPercent) ? RUNDI : UNKNOWN_LANGUAGE,  // KINYARWANDA
  ( 2 >= kMinCorrPercent) ? TAGALOG : UNKNOWN_LANGUAGE,  // MALAGASY
  (17 >= kMinCorrPercent) ? GERMAN : UNKNOWN_LANGUAGE,  // NAURU
  (12 >= kMinCorrPercent) ? SOMALI : UNKNOWN_LANGUAGE,  // OROMO
  (30 >= kMinCorrPercent) ? KINYARWANDA : UNKNOWN_LANGUAGE,  // RUNDI
  (11 >= kMinCorrPercent) ? TONGA : UNKNOWN_LANGUAGE,  // SAMOAN
  ( 1 >= kMinCorrPercent) ? LINGALA : UNKNOWN_LANGUAGE,  // SANGO
  (32 >= kMinCorrPercent) ? MARATHI : UNKNOWN_LANGUAGE,  // SANSKRIT
  (16 >= kMinCorrPercent) ? ZULU : UNKNOWN_LANGUAGE,  // SISWANT
  ( 5 >= kMinCorrPercent) ? SISWANT : UNKNOWN_LANGUAGE,  // TSONGA
  (29 >= kMinCorrPercent) ? SESOTHO : UNKNOWN_LANGUAGE,  // TSWANA
  ( 2 >= kMinCorrPercent) ? ESTONIAN : UNKNOWN_LANGUAGE,  // VOLAPUK
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // ZHUANG
  ( 1 >= kMinCorrPercent) ? MALAY : UNKNOWN_LANGUAGE,  // KHASI
  (28 >= kMinCorrPercent) ? ENGLISH : UNKNOWN_LANGUAGE,  // SCOTS
  (15 >= kMinCorrPercent) ? KINYARWANDA : UNKNOWN_LANGUAGE,  // GANDA
  ( 7 >= kMinCorrPercent) ? ENGLISH : UNKNOWN_LANGUAGE,  // MANX
  ( 0 >= kMinCorrPercent) ? Unknown : UNKNOWN_LANGUAGE,  // MONTENEGRIN
};

COMPILE_ASSERT(arraysize(kClosestAltLanguage) == NUM_LANGUAGES,
               kClosestAltLanguage_has_incorrect_size);


inline bool FlagFinish(int flags) {return (flags & kCLDFlagFinish) != 0;}
inline bool FlagSqueeze(int flags) {return (flags & kCLDFlagSqueeze) != 0;}
inline bool FlagRepeats(int flags) {return (flags & kCLDFlagRepeats) != 0;}
inline bool FlagTop40(int flags) {return (flags & kCLDFlagTop40) != 0;}
inline bool FlagShort(int flags) {return (flags & kCLDFlagShort) != 0;}
inline bool FlagHint(int flags) {return (flags & kCLDFlagHint) != 0;}
inline bool FlagUseWords(int flags) {return (flags & kCLDFlagUseWords) != 0;}




//------------------------------------------------------------------------------
// For --cld_html debugging output. Not thread safe
//------------------------------------------------------------------------------
static Language prior_lang = UNKNOWN_LANGUAGE;
static bool prior_unreliable = false;

//------------------------------------------------------------------------------
// End For --cld_html debugging output
//------------------------------------------------------------------------------


// Backscan to word boundary, returning how many bytes n to go back
// so that src - n is non-space ans src - n - 1 is space.
// If not found in kMaxSpaceScan bytes, return 0
int BackscanToSpace(const char* src, int limit) {
  int n = 0;
  limit = cld::minint(limit, kMaxSpaceScan);
  while (n < limit) {
    if (src[-n - 1] == ' ') {return n;}    // We are at _X
    ++n;
  }
  return 0;
}

// Forwardscan to word boundary, returning how many bytes n to go forward
// so that src + n is non-space ans src + n - 1 is space.
// If not found in kMaxSpaceScan bytes, return 0
int ForwardscanToSpace(const char* src, int limit) {
  int n = 0;
  limit = cld::minint(limit, kMaxSpaceScan);
  while (n < limit) {
    if (src[n] == ' ') {return n + 1;}    // We are at _X
    ++n;
  }
  return 0;
}


// This uses a cheap predictor to get a measure of compression, and
// hence a measure of repetitiveness. It works on complete UTF-8 characters
// instead of bytes, because three-byte UTF-8 Indic, etc. text compress highly
// all the time when done with a byte-based count. Sigh.
//
// To allow running prediction across multiple chunks, caller passes in current
// 12-bit hash value and int[4096] prediction table. Caller inits these to 0.
//
// Returns the number of *bytes* correctly predicted, increments by 1..4 for
// each correctly-predicted character.
//
// NOTE: Overruns by up to three bytes. Not a problem with valid UTF-8 text
//
int CountPredictedBytes(const char* isrc, int srclen, int* hash, int* tbl) {
  int p_count = 0;
  const uint8* src = reinterpret_cast<const uint8*>(isrc);
  const uint8* srclimit = src + srclen;
  int local_hash = *hash;

  while (src < srclimit) {
    int c = src[0];
    int incr = 1;

    // Pick up one char and length
    if (c < 0xc0) {
      // One-byte or continuation byte: 00xxxxxx 01xxxxxx 10xxxxxx
      // Do nothing more
    } else if ((c & 0xe0) == 0xc0) {
      // Two-byte
      c = (c << 8) | src[1];
      incr = 2;
    } else if ((c & 0xf0) == 0xe0) {
      // Three-byte
      c = (c << 16) | (src[1] << 8) | src[2];
      incr = 3;
    } else {
      // Four-byte
      c = (c << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
      incr = 4;
    }
    src += incr;

    int p = tbl[local_hash];            // Prediction
    tbl[local_hash] = c;                // Update prediction
    p_count += (c == p);                // Count good predictions

    local_hash = ((local_hash << 4) ^ c) & 0xfff;
  }

  *hash = local_hash;
  return p_count;
}



// Counts number of spaces; a little faster than one-at-a-time
// Doesn't count odd bytes at end
int CountSpaces4(const char* src, int src_len) {
  int s_count = 0;
  for (int i = 0; i < (src_len & ~3); i += 4) {
    s_count += (src[i] == ' ');
    s_count += (src[i+1] == ' ');
    s_count += (src[i+2] == ' ');
    s_count += (src[i+3] == ' ');
  }
  return s_count;
}


// Remove words of text that have more than half their letters predicted
// correctly by our cheap predictor, moving the remaining words in-place
// to the front of the input buffer.
//
// To allow running prediction across multiple chunks, caller passes in current
// 12-bit hash value and int[4096] prediction table. Caller inits these to 0.
//
// Return the new, possibly-shorter length
int CheapRepWordsInplace(char* isrc, int srclen, int* hash, int* tbl) {
  const uint8* src = reinterpret_cast<const uint8*>(isrc);
  const uint8* srclimit = src + srclen;
  char* dst = isrc;
  int local_hash = *hash;
  char* word_dst = dst;           // Start of next word
  int good_predict_bytes = 0;
  int word_length_bytes = 0;

  while (src < srclimit) {
    int c = src[0];
    int incr = 1;
    *dst++ = c;

    if (c == ' ') {
      if ((good_predict_bytes * 2) > word_length_bytes) {
        // Word is well-predicted: backup to start of this word
        dst = word_dst;
        if (FLAGS_cld_showme) {
          // Mark the deletion point with period
          // Don't repeat multiple periods
          // Cannot mark with more bytes or may overwrite unseen input
          if ((isrc < (dst - 2)) && (dst[-2] != '.')) {
            *dst++ = '.';
            *dst++ = ' ';
          }
        }
      }
      word_dst = dst;              // Start of next word
      good_predict_bytes = 0;
      word_length_bytes = 0;
    }

    // Pick up one char and length
    if (c < 0xc0) {
      // One-byte or continuation byte: 00xxxxxx 01xxxxxx 10xxxxxx
      // Do nothing more
    } else if ((c & 0xe0) == 0xc0) {
      // Two-byte
      *dst++ = src[1];
      c = (c << 8) | src[1];
      incr = 2;
    } else if ((c & 0xf0) == 0xe0) {
      // Three-byte
      *dst++ = src[1];
      *dst++ = src[2];
      c = (c << 16) | (src[1] << 8) | src[2];
      incr = 3;
    } else {
      // Four-byte
      *dst++ = src[1];
      *dst++ = src[2];
      *dst++ = src[3];
      c = (c << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
      incr = 4;
    }
    src += incr;
    word_length_bytes += incr;

    int p = tbl[local_hash];            // Prediction
    tbl[local_hash] = c;                // Update prediction
    if (c == p) {
      good_predict_bytes += incr;       // Count good predictions
    }

    local_hash = ((local_hash << 4) ^ c) & 0xfff;
  }

  *hash = local_hash;

  if ((dst - isrc) < srclen) {
    // Make last char clean UTF-8 by putting following space off the end
    dst[0] = ' ';
  }

  return static_cast<int>(dst - isrc);
}


// Remove portions of text that have a high density of spaces, or that are
// overly repetitive, squeezing the remaining text in-place to the front of the
// input buffer.
//
// Squeezing looks at density of space/prediced chars in fixed-size chunks,
// specified by chunksize. A chunksize <= 0 uses the default size of 48 bytes.
//
// Return the new, possibly-shorter length
int CompactLangDetImpl::CheapSqueezeInplace(char* isrc,
                                            int srclen,
                                            int ichunksize) {
  char* src = isrc;
  char* dst = src;
  char* srclimit = src + srclen;
  bool skipping = false;

  int hash = 0;
  // Allocate local prediction table.
  int* predict_tbl = new int[kPredictionTableSize];
  memset(predict_tbl, 0, kPredictionTableSize * sizeof(predict_tbl[0]));

  int chunksize = ichunksize;
  if (chunksize == 0) {chunksize = kChunksizeDefault;}
  int space_thresh = (chunksize * kSpacesThreshPercent) / 100;
  int predict_thresh = (chunksize * kPredictThreshPercent) / 100;

  while (src < srclimit) {
    int remaining_bytes = srclimit - src;
    int len = cld::minint(chunksize, remaining_bytes);
    int space_n = CountSpaces4(src, len);
    int predb_n = CountPredictedBytes(src, len, &hash, predict_tbl);
    if ((space_n >= space_thresh) || (predb_n >= predict_thresh)) {
      // Skip the text
      if (!skipping) {
        // Keeping-to-skipping transition; do it at a space
        int n = BackscanToSpace(dst, static_cast<int>(dst - isrc));
        dst -= n;
        skipping = true;
        if (FLAGS_cld_showme) {
          // Mark the deletion point with black square U+25A0
          *dst++ = 0xe2;
          *dst++ = 0x96;
          *dst++ = 0xa0;
          *dst++ = ' ';
        }
      }
    } else {
      // Keep the text
      if (skipping) {
        // Skipping-to-keeping transition; do it at a space
        int n = ForwardscanToSpace(src, len);
        src += n;
        remaining_bytes -= n;   // Shrink remaining length
        len -= n;
        skipping = false;
      }
      // "len" can be negative in some cases
      if (len > 0) {
        memcpy(dst, src, len);
        dst += len;
      }
    }
    src += len;
  }

  if ((dst - isrc) < srclen) {
    // Make last char clean UTF-8 by putting following space off the end
    dst[0] = ' ';
  }

  // Deallocate local prediction table
  delete[] predict_tbl;
  return static_cast<int>(dst - isrc);
}

// Timing 2.8GHz P4 with 170KB input
//  About 90 MB/sec, with or without memcpy, chunksize 48 or 4096
//  Just CountSpaces is about 340 MB/sec
//  Byte-only CountPredictedBytes is about 150 MB/sec
//  Byte-only CountPredictedBytes, conditional tbl[] = is about 85! MB/sec
//  Byte-only CountPredictedBytes is about 180 MB/sec, byte tbl, byte/int c
//  Unjammed byte-only both = 170 MB/sec
//  Jammed byte-only both = 120 MB/sec
//  Back to original w/slight updates, 110 MB/sec
//
bool CheapSqueezeTriggerTest(const char* src, int srclen, int testsize) {
  // Don't trigger at all on short text
  if (srclen < testsize) {return false;}
  int space_thresh = (testsize * kSpacesTriggerPercent) / 100;
  int predict_thresh = (testsize * kPredictTriggerPercent) / 100;
  int hash = 0;
  // Allocate local prediction table.
  int* predict_tbl = new int[kPredictionTableSize];
  memset(predict_tbl, 0, kPredictionTableSize * sizeof(predict_tbl[0]));

  bool retval = false;
  if ((CountSpaces4(src, testsize) >= space_thresh) ||
      (CountPredictedBytes(src, testsize, &hash, predict_tbl) >=
       predict_thresh)) {
    retval = true;
  }
  // Deallocate local prediction table
  delete[] predict_tbl;
  return retval;
}



// Close pairs (correlation) language_enum/language_enum
//  id/ms (0.47)    38/40    [1]
//  bo/dz (0.46)    105/135  [2]
//  cz/sk (0.43)    17/68    [3]
//  no/nn (0.42)    10/80    [4]
//  hi/mr (0.38)    35/64    [5]
//  xh/zu (0.37)    83/84    [6]
// Subscripted by packed language, gives 0 or a subscript in closepair
// scoring array inside doc_tote
static const uint8 kClosePair[EXT_NUM_LANGUAGES + 1] = {
  0,
  0,0,0,0,0,0,0,0, 0,0,4,0,0,0,0,0, 0,3,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,5,0,0,1,0, 1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  5,0,0,0,3,0,0,0, 0,0,0,0,0,0,0,0, 4,0,0,6,6,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,2,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,2, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  // Add new language close-pair number just before here (just use 0)
};


// Delete any extended languages from doc_tote
void RemoveExtendedLanguages(ToteWithReliability* doc_tote) {
  for (int sub = 0; sub < doc_tote->MaxSize(); ++sub) {
    if (cld::UnpackLanguage(doc_tote->Key(sub)) >= NUM_LANGUAGES) {
      // Effectively remove the extended language by setting key&score to zero
      if (FLAGS_dbgscore) {
        fprintf(stderr, "{-%s} ",
                ExtLanguageCode(cld::UnpackLanguage(doc_tote->Key(sub))));
      }

      // Delete entry
      doc_tote->SetKey(sub, 0);
      doc_tote->SetValue(sub, 0);
      doc_tote->SetReliability(sub, 0);
    }
  }
}

static const int kMinReliableKeepPercent = 41;  // Remove lang if reli < this

// For Tier3 languages, require a minimum number of bytes to be first-place lang
static const int kGoodFirstT3MinBytes = 24;         // <this => no first

// Move bytes for unreliable langs to another lang or UNKNOWN
// doc_tote is sorted, so cannot Add
//
// If both CHINESE and CHINESET are present and unreliable, do not delete both;
// merge both into CHINESE.
//
// we also want to remove Tier3 languages as the first lang if there is very
// little text like ej1 ej2 ej3 ej4
// maybe fold this back in earlier
//
void RemoveUnreliableLanguages(ToteWithReliability* doc_tote) {
  // Prepass to merge some low-reliablility languages
  int total_bytes = 0;
  for (int sub = 0; sub < doc_tote->MaxSize(); ++sub) {
    int plang = doc_tote->Key(sub);
    if (plang == 0) {continue;}                     // Empty slot

    Language lang = cld::UnpackLanguage(plang);
    int bytes = doc_tote->Value(sub);
    int reli = doc_tote->Reliability(sub);
    if (bytes == 0) {continue;}                     // Zero bytes
    total_bytes += bytes;

    // Reliable percent is stored reliable score over stored bytecount
    int reliable_percent = reli / bytes;
    if (reliable_percent >= kMinReliableKeepPercent) {continue;}   // Keeper

    // This language is too unreliable to keep, but we might merge it.
    Language altlang = UNKNOWN_LANGUAGE;
    if (lang < NUM_LANGUAGES) {altlang = kClosestAltLanguage[lang];}
    if (altlang == UNKNOWN_LANGUAGE) {continue;}    // No alternative

    // Look for alternative in doc_tote
    int altsub = doc_tote->Find(cld::PackLanguage(altlang));
    if (altsub < 0) {continue;}                     // No alternative text

    int bytes2 = doc_tote->Value(altsub);
    int reli2 = doc_tote->Reliability(altsub);
    if (bytes2 == 0) {continue;}                    // Zero bytes

    // Reliable percent is stored reliable score over stored bytecount
    int reliable_percent2 = reli2 / bytes2;

    // Merge one language into the other. Break ties toward lower lang #
    int tosub = altsub;
    int fromsub = sub;
    bool into_lang = false;
    if ((reliable_percent2 < reliable_percent) ||
        ((reliable_percent2 == reliable_percent) && (lang < altlang))) {
      tosub = sub;
      fromsub = altsub;
      into_lang = true;
    }

    // Make sure reliability doesn't drop and is enough to avoid delete
    int newpercent = cld::maxint(reliable_percent, reliable_percent2);
    newpercent = cld::maxint(newpercent, kMinReliableKeepPercent);
    int newbytes = bytes + bytes2;
    int newreli = newpercent * newbytes;

    doc_tote->SetKey(fromsub, 0);
    doc_tote->SetValue(fromsub, 0);
    doc_tote->SetReliability(fromsub, 0);
    doc_tote->SetValue(tosub, newbytes);
    doc_tote->SetReliability(tosub, newreli);

    // Show fate of unreliable languages if at least 10 bytes
    if (FLAGS_cld_html /*&& (newpercent >= 10)*/ && (newbytes >= 10)) {
      if (into_lang) {
        fprintf(stderr, "{Unreli %s.%d(%dB) => %s} ",
                ExtLanguageCode(altlang), reliable_percent2, bytes2,
                ExtLanguageCode(lang));
      } else {
        fprintf(stderr, "{Unreli %s.%d(%dB) => %s} ",
                ExtLanguageCode(lang), reliable_percent, bytes,
                ExtLanguageCode(altlang));
      }
    }
  }


  // Pass to delete any remaining unreliable languages
  for (int sub = 0; sub < doc_tote->MaxSize(); ++sub) {
    int plang = doc_tote->Key(sub);
    if (plang == 0) {continue;}                     // Empty slot

    Language lang = cld::UnpackLanguage(plang);
    int bytes = doc_tote->Value(sub);
    int reli = doc_tote->Reliability(sub);
    if (bytes == 0) {continue;}                     // Zero bytes

    bool is_tier3 = (cld::kIsPackedTop40[plang] == 0);
    if (is_tier3 &&
        (bytes < kGoodFirstT3MinBytes) &&
        (bytes < total_bytes)) {
      reli = 0;                                     // Too-short tier3
    }

    // Reliable percent is stored as reliable score over stored bytecount
    int reliable_percent = reli / bytes;
    if (reliable_percent >= kMinReliableKeepPercent) {continue;}  // Keeper

    // Delete unreliable entry
    doc_tote->SetKey(sub, 0);
    doc_tote->SetValue(sub, 0);
    doc_tote->SetReliability(sub, 0);

    // Show fate of unreliable languages if at least 10 bytes
    if (FLAGS_cld_html /*&& (reliable_percent >= 10)*/ && (bytes >= 10)) {
      fprintf(stderr, "{Unreli %s.%d(%dB)} ",
              ExtLanguageCode(lang), reliable_percent, bytes);
    }
  }

  if (FLAGS_cld_html) {fprintf(stderr, "<br>\n");}
}


// Move less likely byte count to more likely for close pairs of languages
void RefineScoredClosePairs(ToteWithReliability* doc_tote) {
  for (int sub = 0; sub < doc_tote->MaxSize(); ++sub) {
    int close_packedlang = doc_tote->Key(sub);
    int subscr = kClosePair[close_packedlang];
    if (subscr == 0) {continue;}

    // We have a close pair language -- if the other one is also scored and the
    // longword score differs enough, put all our eggs into one basket

    // Nonzero longword score: Go look for the other of this pair
    for (int sub2 = sub + 1; sub2 < doc_tote->MaxSize(); ++sub2) {
      if (kClosePair[doc_tote->Key(sub2)] == subscr) {
        // We have a matching pair
        int close_packedlang2 = doc_tote->Key(sub2);

        // Move all the text bytes from lower byte-count to higher one
        int from_sub, to_sub;
        Language from_lang, to_lang;
        if (doc_tote->Value(sub) < doc_tote->Value(sub2)) {
          from_sub = sub;
          to_sub = sub2;
          from_lang = cld::UnpackLanguage(close_packedlang);
          to_lang = cld::UnpackLanguage(close_packedlang2);
        } else {
          from_sub = sub2;
          to_sub = sub;
          from_lang = cld::UnpackLanguage(close_packedlang2);
          to_lang = cld::UnpackLanguage(close_packedlang);
        }

        // Move all the bytes smaller => larger of the pair
        if (FLAGS_cld_html || FLAGS_dbgscore) {
          // Show fate of closepair language
          int val = doc_tote->Value(from_sub);
          int reli = doc_tote->Reliability(from_sub);
          int reliable_percent = reli / (val ? val : 1);  // avoid zdiv
          fprintf(stderr, "{CloseLangPair: %s.%d%%(%dB) => %s} ",
                  ExtLanguageCode(from_lang),
                  reliable_percent,
                  doc_tote->Value(from_sub),
                  ExtLanguageCode(to_lang));
        }
        int sum = doc_tote->Value(to_sub) + doc_tote->Value(from_sub);
        doc_tote->SetValue(to_sub, sum);
        doc_tote->SetReliability(to_sub, 100 * sum);

        // Delete old entry
        doc_tote->SetKey(from_sub, 0);
        doc_tote->SetValue(from_sub, 0);
        doc_tote->SetReliability(from_sub, 0);

        break;    // Exit inner for sub2 loop
      }
    }     // End for sub2
  }   // End for sub
}


void ApplyLanguageHints(Tote* chunk_tote, int tote_grams,
                        uint8* lang_hint_boost) {
  // Need 8 quad/unigrams to give full hint boost, else derate linearly
  if (tote_grams > 8) {
    tote_grams = 8;
  }
  for (int sub = 0; sub < chunk_tote->MaxSize(); ++sub) {
    // Hint boosts are per packed subscript
    int lang_sub = chunk_tote->Key(sub);
    int new_value = chunk_tote->Value(sub) +
      ((lang_hint_boost[lang_sub] * tote_grams) >> 3);
    chunk_tote->SetValue(sub, new_value);
    if (FLAGS_dbgscore && (lang_hint_boost[lang_sub] > 0)) {
      fprintf(stderr, "[%s+=%d*%d/8] ",
              ExtLanguageCode(cld::UnpackLanguage(lang_sub)),
              lang_hint_boost[lang_sub], tote_grams);
    }
  }
}


void PrintHtmlEscapedText(FILE* f, const char* txt, int len) {
  for (int i = 0; i < len; ++i) {
    char c = txt[i];
    if (c == '<') {
      fprintf(f, "&lt;");
    } else if (c == '>') {
      fprintf(f, "&gt;");
    } else if (c == '&') {
      fprintf(f, "&amp;");
    } else if (c == '\'') {
      fprintf(f, "&apos;");
    } else if (c == '"') {
      fprintf(f, "&quot;");
    } else {
      fprintf(f, "%c", c);
    }
  }
  fprintf(f, "<br>\n");
}


// Add one chunk's score to running document score
// If the top language is UNKNOWN_LANGUAGE, score nothing. This is used to
// positively identify text to be ignored, such as link farms.
// Sort before scoring and reinit afterward
//
// src and srclen are just for debug output
void ScoreChunkIntoDoc(const char* src, int srclen, int advance_by,
                       UnicodeLScript lscript,
                       Tote* chunk_tote,
                       ToteWithReliability* doc_tote,
                       int tote_grams,
                       uint8* lang_hint_boost) {
  // Apply hints before sorting
  if (lang_hint_boost) {
    ApplyLanguageHints(chunk_tote, tote_grams, lang_hint_boost);
  }

  // Sort to get top two languages
  chunk_tote->Sort(2);
  Language cur_lang = cld::UnpackLanguage(chunk_tote->Key(0));

  // Return if empty
  if (cur_lang < 0) {
    chunk_tote->Reinit();
    return;
  }

  bool cur_unreliable = false;

  // Reliability is a function of mean script score per KB of text
  int len = chunk_tote->GetByteCount();
  int reliability = cld::GetReliability((len * 2) / advance_by,
                                        lscript,
                                        chunk_tote);
  cur_unreliable = (reliability < cld::kMinReliable);

  // If tote_grams=0, always reliable
  // If tote_grams=1, always unreliable
  if (tote_grams == 0) {
    reliability = 100;
    cur_unreliable = false;
  } else if (tote_grams == 1) {
    reliability = 0;
    cur_unreliable = true;
  }

#if 0
  // TEMP
  if (FLAGS_cld_html) {
    if (reliability >= kMinReliableKeepPercent) {
      fprintf(stderr, "R%d%% ", reliability);
    } else {
      fprintf(stderr, "--R%d%% ", reliability);
    }
  }
#endif

  // Track the sequence of language fragments [result currently unused]
  ////if (reliability >= kMinReliableSeq) {
  ////  doc_tote->AddSeq(chunk_tote->Key(0));
  ////}

  if (cur_unreliable && (chunk_tote->Key(1) != 0)) {
    // Unreliable and two top contenders, split byte count 5/8 - 3/8
    int top_len = ((len * 5) + 4) >> 3;
    int second_len = len - top_len;

    doc_tote->Add(chunk_tote->Key(0),
                  top_len, chunk_tote->Value(0), reliability);
    doc_tote->Add(chunk_tote->Key(1),
                  second_len, chunk_tote->Value(1), reliability);
    if (FLAGS_dbgscore) {
      fprintf(stderr, "{+%s.%d.%dR(%dB) +%s.%d.%dR(%dB)} ",
              ExtLanguageCode(cld::UnpackLanguage(chunk_tote->Key(0))),
              chunk_tote->Value(0),
              reliability,
              top_len,
              ExtLanguageCode(cld::UnpackLanguage(chunk_tote->Key(1))),
              chunk_tote->Value(1),
              reliability,
              second_len);
    }
  } else {
    // Reliable or single contender
    doc_tote->Add(chunk_tote->Key(0),
                  len, chunk_tote->Value(0), reliability);
    if (FLAGS_dbgscore) {
      fprintf(stderr, "{+%s.%d.%dR(%dB)} ",
              ExtLanguageCode(cld::UnpackLanguage(chunk_tote->Key(0))),
              chunk_tote->Value(0),
              reliability,
              len);
    }
  }

  if (FLAGS_cld_html) {
    if (cur_lang < 0) {cur_lang = UNKNOWN_LANGUAGE;}
    cld::PrintLang(stderr, chunk_tote,
              cur_lang, cur_unreliable,
              prior_lang, prior_unreliable);
    prior_lang = cur_lang;
    prior_unreliable = cur_unreliable;

    string temp(src, srclen);
    if (temp[0] == '=') {
      // Rewrite =ScriptX= or =SwitchX= as =Xxxx= for script code Xxxx
      temp = "=Buffered_";
      temp.append(UnicodeLScriptCode(lscript));
      temp.append("=");
    }
    cld::PrintText(stderr, cur_lang, temp);
  }

  chunk_tote->Reinit();
}


void PrintTopLang(Language top_lang) {
  if ((top_lang == prior_lang) && (top_lang != UNKNOWN_LANGUAGE)) {
    fprintf(stderr, "[] ");
  } else {
    fprintf(stderr, "[%s] ", ExtLanguageName(top_lang));
    prior_lang = top_lang;
  }
}

void PrintTopLangSpeculative(Language top_lang) {
  fprintf(stderr, "<span style=\"color:#%06X;\">", 0xa0a0a0);
  if ((top_lang == prior_lang) && (top_lang != UNKNOWN_LANGUAGE)) {
    fprintf(stderr, "[] ");
  } else {
    fprintf(stderr, "[%s] ", ExtLanguageName(top_lang));
    prior_lang = top_lang;
  }
  fprintf(stderr, "</span>\n");
}


// Add one chunk's score to running document score
// Convenience function with constant src text
void ScoreChunkIntoDoc2(const char* src, int advance_by,
                       UnicodeLScript lscript,
                       Tote* chunk_tote,
                       ToteWithReliability* doc_tote,
                       int tote_grams,
                       uint8* lang_hint_boost) {
  int srclen = static_cast<int>(strlen(src));
  ScoreChunkIntoDoc(src, srclen, advance_by, lscript, chunk_tote,
                    doc_tote, tote_grams, lang_hint_boost);
}


// Score one scriptspan using the only language for that script
void ScoreNilgrams(getone::LangSpan* scriptspan, int lang,
                  ToteWithReliability* doc_tote,
                  uint8* lang_hint_boost,
                  int flags, Language plus_one) {
  // For debugging only. Not thread-safe
  prior_lang = UNKNOWN_LANGUAGE;
  prior_unreliable = false;

  const char* src = scriptspan->text;
  int len = scriptspan->text_bytes;

  Tote chunk_tote;
  // Score 1000 for 1000 bytes
  chunk_tote.AddGram();
  chunk_tote.Add(lang, scriptspan->text_bytes);
  chunk_tote.AddBytes(scriptspan->text_bytes);
  int advance_by = 2;
  int tote_grams = 0;   // Indicates fully reliable
  ScoreChunkIntoDoc(src, len, advance_by,
                    scriptspan->script, &chunk_tote,
                    doc_tote, tote_grams, lang_hint_boost);
}

// Score one scriptspan using unigrams
// Updates tote_grams
void ScoreUnigrams(getone::LangSpan* scriptspan,
                      int* tote_grams, int gram_limit,
                      Tote* chunk_tote,
                      ToteWithReliability* doc_tote,
                      uint8* lang_hint_boost,
                      int advance_by, int flags,
                   int* initial_word_span, Language plus_one) {
  // chunk_tote may have partial sum coming in
  const char* src = scriptspan->text;
  const char* srclimit = src + scriptspan->text_bytes;

  // For debugging only. Not thread-safe
  prior_lang = UNKNOWN_LANGUAGE;
  prior_unreliable = false;

  // Break text up into multiple chunks and score each
  while (src < srclimit) {
    // Updates tote_grams
    int len = cld::DoUniScoreV3(&compact_lang_det_generated_ctjkvz_b1_obj,
                                 src, srclimit - src, advance_by,
                                 tote_grams, gram_limit, chunk_tote);
    if (FlagUseWords(flags) || (*initial_word_span > 0)) {
      // Use bigram scoring in addition to quadgrams
      cld::DoBigramScoreV3(&kCjkBiTable_obj,
                           src, len, chunk_tote);
    }
    chunk_tote->AddBytes(len);
    *initial_word_span -= len;

    if (*tote_grams >= gram_limit) {
      // Add this chunk to doc totals
      // Remove all but top40 if asked
      if (FlagTop40(flags)) {
        cld::DemoteNotTop40(chunk_tote, cld::PackLanguage(plus_one));
      }

      // Sort, accumulate into doc total, reinit
      ScoreChunkIntoDoc(src, len, advance_by,
                        scriptspan->script, chunk_tote,
                        doc_tote, *tote_grams, lang_hint_boost);
      *tote_grams = 0;
    } else {
      if (FLAGS_cld_html) {
        string temp(src, len);
        Language top_lang = cld::UnpackLanguage(chunk_tote->CurrentTopKey());
        PrintTopLangSpeculative(top_lang);
        cld::PrintText(stderr, top_lang, temp);
      }
    }
    src += len;
  }
  // chunk_tote may have partial sum going out
}

// Back up one UTF-8 character
const uint8* BackOneUTF8(const uint8* p) {
  const uint8* retval = p - 1;
  if ((*retval & 0xc0) == 0x80) {--retval;}
  if ((*retval & 0xc0) == 0x80) {--retval;}
  if ((*retval & 0xc0) == 0x80) {--retval;}
  return retval;
}


// Score one scriptspan using quadgrams
// Incoming chunk_tote may have partial accumulation
void ScoreQuadgrams(getone::LangSpan* scriptspan,
                       int* tote_grams, int gram_limit,
                       Tote* chunk_tote,
                       ToteWithReliability* doc_tote,
                       uint8* lang_hint_boost,
                       int advance_by, int flags,
                       int* initial_word_span, Language plus_one) {
  // chunk_tote may have partial sum coming in
  const char* src = scriptspan->text;
  const char* srclimit = src + scriptspan->text_bytes;
  const char* lastscored_src = src;

  // For debugging only. Not thread-safe
  prior_lang = UNKNOWN_LANGUAGE;
  prior_unreliable = false;

  // Break text up into multiple chunks and score each
  while (src < srclimit) {
    // Updates tote_grams
    int len = cld::DoQuadScoreV3(&kQuadTable_obj,
                                 src, srclimit - src, advance_by,
                                 tote_grams, gram_limit, chunk_tote);
    if (FlagUseWords(flags) || (*initial_word_span > 0)) {
      // Use word scoring in addition to quadgrams
      cld::DoOctaScoreV3(&kLongWord8Table_obj,
                         src, len, chunk_tote);
    }
    chunk_tote->AddBytes(len);
    *initial_word_span -= len;

    if (*tote_grams >= gram_limit) {
      // Remove all but top40 if asked
      if (FlagTop40(flags)) {
        cld::DemoteNotTop40(chunk_tote, cld::PackLanguage(plus_one));
      }

      // Sort, accumulate into doc total, reinit
      ScoreChunkIntoDoc(src, len, advance_by,
                        scriptspan->script, chunk_tote,
                        doc_tote, *tote_grams, lang_hint_boost);
      lastscored_src = src + len;
      *tote_grams = 0;
    } else {
      if (FLAGS_cld_html) {
        string temp(src, len);
        Language top_lang = cld::UnpackLanguage(chunk_tote->CurrentTopKey());
        PrintTopLangSpeculative(top_lang);
        cld::PrintText(stderr, top_lang, temp);
      }
    }
    src += len;
  }
}



void PrintLangs(FILE* f, const Language* language3, const int* percent3,
                const int* text_bytes, const bool* is_reliable) {
  fprintf(f, "<br>&nbsp;&nbsp;Initial_Languages ");
  if (language3[0] != UNKNOWN_LANGUAGE) {
    fprintf(f, "%s%s(%d%%)  ",
            ExtLanguageName(language3[0]),
            *is_reliable ? "" : "*",
            percent3[0]);
  }
  if (language3[1] != UNKNOWN_LANGUAGE) {
    fprintf(f, "%s(%d%%)  ", ExtLanguageName(language3[1]), percent3[1]);
  }
  if (language3[2] != UNKNOWN_LANGUAGE) {
    fprintf(f, "%s(%d%%)  ", ExtLanguageName(language3[2]), percent3[2]);
  }
  fprintf(f, "%d bytes \n", *text_bytes);

  fprintf(f, "<br>\n");
}


// Start the tote with a count of one for the default language for script
void InitScriptToteLang(Tote* script_tote, UnicodeLScript lscript) {
  Language defaultlang = cld::kDefaultLanguagePerLScript[lscript];
  script_tote->Add(cld::PackLanguage(defaultlang), 1);
  script_tote->AddBytes(1);
#if 0
  if (FLAGS_cld_html) {
    cld::PrintLang(stderr, script_tote,
              defaultlang, false,
              UNKNOWN_LANGUAGE, false);
    prior_lang = cur_lang;
    string temp("+1");
    cld::PrintText(stderr, defaultlang, temp);
  }
#endif
}

static const char* const kToteName[4] =
  {"=Latn=", "=Hani=", "=Script2=", "=Script3="};
static const char* const kToteSwitch[4] =
  {"=Latn=", "=Hani=", "=Switch2=", "=Switch3="};



// Upper to lower, keep digits, everything else to minus '-' (2d)
static const char kCharsetToLowerTbl[256] = {
  0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d, 0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
  0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d, 0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
  0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d, 0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37, 0x38,0x39,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,

  0x2d,0x61,0x62,0x63,0x64,0x65,0x66,0x67, 0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77, 0x78,0x79,0x7a,0x2d,0x2d,0x2d,0x2d,0x2d,
  0x2d,0x61,0x62,0x63,0x64,0x65,0x66,0x67, 0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77, 0x78,0x79,0x7a,0x2d,0x2d,0x2d,0x2d,0x2d,

  0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d, 0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
  0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d, 0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
  0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d, 0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
  0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d, 0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,

  0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d, 0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
  0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d, 0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
  0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d, 0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
  0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d, 0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
};


static const char kIsAlpha[256] = {
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,0,0,0,0,0,
  0,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,0,0,0,0,0,

  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
};

static const char kIsDigit[256] = {
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1, 1,1,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,

  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
};

// Normalize ASCII string to first 4 alphabetic/digit chars
// Letters are forced to lowercase ASCII
// Used to normalize TLD values
void MakeChar4(const char* str, char* norm) {
  memcpy(norm, "____", 4);     // four underscores
  int l_ptr = 0;
  for (int i = 0; i < strlen(str); ++i) {
    uint8 uc = static_cast<uint8>(str[i]);
    if (kIsAlpha[uc] | kIsDigit[uc]) {
      if (l_ptr < 4) {                  // Else ignore
        norm[l_ptr] = kCharsetToLowerTbl[uc];
        l_ptr++;
      }
    }
  }
}

// Find subscript of matching key in first 4 bytes of sorted hint array, or -1
static int HintBinaryLookup4(const HintEntry* hintprobs, int hintprobssize,
                     const char* norm_key) {
  // Key is always in range [lo..hi)
  int lo = 0;
  int hi = hintprobssize;
  while (lo < hi) {
    int mid = (lo + hi) >> 1;
    int comp = memcmp(&hintprobs[mid].key[0], norm_key, 4);
    if (comp < 0) {
      lo = mid + 1;
    } else if (comp > 0) {
      hi = mid;
    } else {
      return mid;
    }
  }
  return -1;
}


// Increment the initial probabilities based on a per-TLD probs entry
void ApplyTLDHint(uint8* lang_hint_boost, const char* tld_hint) {
  if (FLAGS_dbgscore) {
    fprintf(stderr, "TLD hint %s\n", tld_hint);
  }
  char normalized_tld[8];
  MakeChar4(tld_hint, normalized_tld);
  int n = HintBinaryLookup4(kTLDHintProbs, kTLDHintProbsSize,
                           normalized_tld);
  // TLD is four bytes, probability entry is 4 bytes
  if (n >= 0) {
    uint32 probs = kTLDHintProbs[n].probs;

    uint8 prob123 = (probs >> 0) & 0xff;
    const uint8* prob123_entry = cld::LgProb2TblEntry(prob123);
    uint8 top1 = (probs >> 8) & 0xff;
    if (top1 > 0) {lang_hint_boost[top1] += cld::LgProb3(prob123_entry, 0);}
    uint8 top2 = (probs >> 16) & 0xff;
    if (top2 > 0) {lang_hint_boost[top2] += cld::LgProb3(prob123_entry, 1);}
    uint8 top3 = (probs >> 24) & 0xff;
    if (top3 > 0) {lang_hint_boost[top3] += cld::LgProb3(prob123_entry, 2);}
  }
}


// Increment the initial probabilities based on a per-encoding probs entry
void ApplyEncodingHint(uint8* lang_hint_boost, int encoding_hint) {
  if (FLAGS_dbgscore) {
    Encoding tempenc = static_cast<Encoding>(encoding_hint);
    fprintf(stderr, "ENC hint %s\n", EncodingName(tempenc));
  }
  if (encoding_hint < ISO_8859_1) {return;}
  if (encoding_hint >= NUM_ENCODINGS) {return;}
  uint32 probs = kEncodingHintProbs[encoding_hint];

  uint8 prob123 = (probs >> 0) & 0xff;
  const uint8* prob123_entry = cld::LgProb2TblEntry(prob123);
  uint8 top1 = (probs >> 8) & 0xff;
  if (top1 > 0) {lang_hint_boost[top1] += cld::LgProb3(prob123_entry, 0);}
  uint8 top2 = (probs >> 16) & 0xff;
  if (top2 > 0) {lang_hint_boost[top2] += cld::LgProb3(prob123_entry, 1);}
  uint8 top3 = (probs >> 24) & 0xff;
  if (top3 > 0) {lang_hint_boost[top3] += cld::LgProb3(prob123_entry, 2);}
}


// Increment the initial probability for given language by fixed amount
// Does not recognize extended languages as hints
void ApplyLanguageHint(uint8* lang_hint_boost, Language language_hint) {
  if (FLAGS_dbgscore) {
    fprintf(stderr, "LANG hint %s\n", ExtLanguageName(language_hint));
  }
  if (language_hint < ENGLISH) {return;}
  if (language_hint >= NUM_LANGUAGES) {return;}
  uint32 probs = kLanguageHintProbs[language_hint];

  uint8 prob123 = (probs >> 0) & 0xff;
  const uint8* prob123_entry = cld::LgProb2TblEntry(prob123);
  uint8 top1 = (probs >> 8) & 0xff;
  if (top1 > 0) {lang_hint_boost[top1] += cld::LgProb3(prob123_entry, 0);}
  uint8 top2 = (probs >> 16) & 0xff;
  if (top2 > 0) {lang_hint_boost[top2] += cld::LgProb3(prob123_entry, 1);}
  uint8 top3 = (probs >> 24) & 0xff;
  if (top3 > 0) {lang_hint_boost[top3] += cld::LgProb3(prob123_entry, 2);}
}

// Extract return values before fixups
void ExtractLangEtc(ToteWithReliability* doc_tote, int total_text_bytes,
                    int* reliable_percent3, Language* language3, int* percent3,
                    double*  normalized_score3,
                    int* text_bytes, bool* is_reliable) {
  reliable_percent3[0] = 0;
  reliable_percent3[1] = 0;
  reliable_percent3[2] = 0;
  language3[0] = UNKNOWN_LANGUAGE;
  language3[1] = UNKNOWN_LANGUAGE;
  language3[2] = UNKNOWN_LANGUAGE;
  percent3[0] = 100;
  percent3[1] = 0;
  percent3[2] = 0;
  normalized_score3[0] = 0.0;
  normalized_score3[1] = 0.0;
  normalized_score3[2] = 0.0;

  *text_bytes = total_text_bytes;
  *is_reliable = false;

  int bytecount1 = total_text_bytes;
  int bytecount2 = 0;
  int bytecount3 = 0;

  int lang1 = doc_tote->Key(0);
  if (lang1 != 0) {
    // We have a top language
    language3[0] = cld::UnpackLanguage(lang1);
    bytecount1 = doc_tote->Value(0);
    int reli1 = doc_tote->Reliability(0);
    reliable_percent3[0] = reli1 / (bytecount1 ? bytecount1 : 1);  // avoid zdiv
    normalized_score3[0] = cld::GetNormalizedScore(language3[0],
                                                  ULScript_Common,
                                                  bytecount1,
                                                  doc_tote->Score(0));
  }

  int lang2 = doc_tote->Key(1);
  if (lang2 != 0) {
    language3[1] = cld::UnpackLanguage(lang2);
    bytecount2 = doc_tote->Value(1);
    int reli2 = doc_tote->Reliability(1);
    reliable_percent3[1] = reli2 / (bytecount2 ? bytecount2 : 1);  // avoid zdiv
    normalized_score3[1] = cld::GetNormalizedScore(language3[1],
                                                  ULScript_Common,
                                                  bytecount2,
                                                  doc_tote->Score(1));
  }

  int lang3 = doc_tote->Key(2);
  if (lang3 != 0) {
    language3[2] = cld::UnpackLanguage(lang3);
    bytecount3 = doc_tote->Value(2);
    int reli3 = doc_tote->Reliability(2);
    reliable_percent3[2] = reli3 / (bytecount3 ? bytecount3 : 1);  // avoid zdiv
    normalized_score3[2] = cld::GetNormalizedScore(language3[2],
                                                  ULScript_Common,
                                                  bytecount3,
                                                  doc_tote->Score(2));
  }

  // Increase total bytes to sum (top 3) if low for some reason
  int total_bytecount12 = bytecount1 + bytecount2;
  int total_bytecount123 = total_bytecount12 + bytecount3;
  if (total_text_bytes < total_bytecount123) {
    total_text_bytes = total_bytecount123;
    *text_bytes = total_text_bytes;
  }

  // Sum minus previous % gives better roundoff behavior than bytecount/total
  int total_text_bytes_div = cld::maxint(1, total_text_bytes);    // Avoid zdiv
  percent3[0] = (bytecount1 * 100) / total_text_bytes_div;
  percent3[1] = (total_bytecount12 * 100) / total_text_bytes_div;
  percent3[2] = (total_bytecount123 * 100) / total_text_bytes_div;
  percent3[2] -= percent3[1];
  percent3[1] -= percent3[0];

  // Roundoff, say 96% 1.6% 1.4%, will produce non-obvious 96% 1% 2%
  // Fix this explicitly
  if (percent3[1] < percent3[2]) {
    ++percent3[1];
    --percent3[2];
  }
  if (percent3[0] < percent3[1]) {
    ++percent3[0];
    --percent3[1];
  }

  *text_bytes = total_text_bytes;

  if (lang1 != 0) {
    // We have a top language
    // Its reliability is overal result reliability
    int bytecount = doc_tote->Value(0);
    int reli = doc_tote->Reliability(0);
    int reliable_percent = reli / (bytecount ? bytecount : 1);  // avoid zdiv
    *is_reliable = reliable_percent >= cld::kMinReliable;
  } else {
    // No top language at all. This can happen with zero text or 100% Klingon
    // if extended=false. Just return all UNKNOWN_LANGUAGE, reliable.
    *is_reliable = true;
  }
}

bool IsFIGS(Language lang) {
  if (lang == FRENCH) {return true;}
  if (lang == ITALIAN) {return true;}
  if (lang == GERMAN) {return true;}
  if (lang == SPANISH) {return true;}
  return false;
}

bool IsEFIGS(Language lang) {
  if (lang == ENGLISH) {return true;}
  if (lang == FRENCH) {return true;}
  if (lang == ITALIAN) {return true;}
  if (lang == GERMAN) {return true;}
  if (lang == SPANISH) {return true;}
  return false;
}

static const int kNonEnBoilerplateMinPercent = 17;    // <this => no second
static const int kNonFIGSBoilerplateMinPercent = 20;  // <this => no second
static const int kGoodFirstMinPercent = 26;           // <this => UNK
static const int kGoodFirstReliableMinPercent = 51;   // <this => unreli
static const int kIgnoreMaxPercent = 95;              // >this => unreli
static const int kKeepMinPercent = 2;                 // <this => unreli

// For Tier3 languages, require more bytes of text to override
// the first-place language
static const int kGoodSecondT1T2MinBytes = 15;        // <this => no second
static const int kGoodSecondT3MinBytes = 128;         // <this => no second
                                                      //

// Calculate a single summary language for the document, and its reliability.
// Returns language3[0] or language3[1] or ENGLISH or UNKNOWN_LANGUAGE
// This is the heart of matching human-rater perception.
// reliable_percent3[] is currently unused
//
// Do not return Tier3 second language unless there are at least 128 bytes
void CalcSummaryLang(ToteWithReliability* doc_tote, int total_text_bytes,
                     const int* reliable_percent3,
                     const Language* language3,
                     const int* percent3,
                     Language* summary_lang, bool* is_reliable) {
  // Vector of active languages; changes if we delete some
  int slot_count = 3;
  int active_slot[3] = {0, 1, 2};

  int ignore_percent = 0;
  int return_percent = percent3[0];   // Default to top lang
  *summary_lang = language3[0];
  *is_reliable = true;
  if (percent3[0] < kKeepMinPercent) {*is_reliable = false;}

  // If any of top 3 is IGNORE, remove it and increment ignore_percent
  for (int i = 0; i < 3; ++i) {
    if (language3[i] == TG_UNKNOWN_LANGUAGE) {
      ignore_percent += percent3[i];
      // Move the rest up, levaing input vectors unchanged
      for (int j=i+1; j < 3; ++j) {
        active_slot[j - 1] = active_slot[j];
      }
      -- slot_count;
      // Logically remove Ignore from percentage-text calculation
      // (extra 1 in 101 avoids zdiv, biases slightly small)
      return_percent = (percent3[0] * 100) / (101 - ignore_percent);
      *summary_lang = language3[active_slot[0]];
      if (percent3[active_slot[0]] < kKeepMinPercent) {*is_reliable = false;}
    }
  }


  // If English and X, where X (not UNK) is big enough,
  // assume the English is boilerplate and return X.
  // Logically remove English from percentage-text calculation
  int second_bytes = (total_text_bytes * percent3[active_slot[1]]) / 100;
  // Require more bytes of text for Tier3 languages
  int minbytesneeded = kGoodSecondT1T2MinBytes;
  int plang_second =  cld::PackLanguage(language3[active_slot[1]]);
  bool is_tier3 = (cld::kIsPackedTop40[plang_second] == 0);
  if (is_tier3) {
    minbytesneeded = kGoodSecondT3MinBytes;
  }

  if ((language3[active_slot[0]] == ENGLISH) &&
      (language3[active_slot[1]] != ENGLISH) &&
      (language3[active_slot[1]] != UNKNOWN_LANGUAGE) &&
      (percent3[active_slot[1]] >= kNonEnBoilerplateMinPercent) &&
      (second_bytes >= minbytesneeded)) {
    ignore_percent += percent3[active_slot[0]];
    return_percent = (percent3[active_slot[1]] * 100) / (101 - ignore_percent);
    *summary_lang = language3[active_slot[1]];
    if (percent3[active_slot[1]] < kKeepMinPercent) {*is_reliable = false;}

  // Else If FIGS and X, where X (not UNK, EFIGS) is big enough,
  // assume the FIGS is boilerplate and return X.
  // Logically remove FIGS from percentage-text calculation
  } else if (IsFIGS(language3[active_slot[0]]) &&
             !IsEFIGS(language3[active_slot[1]]) &&
             (language3[active_slot[1]] != UNKNOWN_LANGUAGE) &&
             (percent3[active_slot[1]] >= kNonFIGSBoilerplateMinPercent) &&
             (second_bytes >= minbytesneeded)) {
    ignore_percent += percent3[active_slot[0]];
    return_percent = (percent3[active_slot[1]] * 100) / (101 - ignore_percent);
    *summary_lang = language3[active_slot[1]];
    if (percent3[active_slot[1]] < kKeepMinPercent) {*is_reliable = false;}

  // Else we are returning the first language, but want to improve its
  // return_percent if the second language should be ignored
  } else  if ((language3[active_slot[1]] == ENGLISH) &&
              (language3[active_slot[0]] != ENGLISH)) {
    ignore_percent += percent3[active_slot[1]];
    return_percent = (percent3[active_slot[0]] * 100) / (101 - ignore_percent);
  } else  if (IsFIGS(language3[active_slot[1]]) &&
              !IsEFIGS(language3[active_slot[0]])) {
    ignore_percent += percent3[active_slot[1]];
    return_percent = (percent3[active_slot[0]] * 100) / (101 - ignore_percent);
  }

  // If return percent is too small (too many languages), return UNKNOWN
  if ((return_percent < kGoodFirstMinPercent)) {
    *summary_lang = UNKNOWN_LANGUAGE;
    *is_reliable = false;
  }

  // If return percent is small, return language but set unreliable.
  if ((return_percent < kGoodFirstReliableMinPercent)) {
    *is_reliable = false;
  }

  // If ignore percent is too large, set unreliable.
  if ((ignore_percent > kIgnoreMaxPercent)) {
    *is_reliable = false;
  }

  // If we removed all the active languages, return UNKNOWN
  if (slot_count == 0) {
    *summary_lang = UNKNOWN_LANGUAGE;
    *is_reliable = false;
  }
}



// Result vector must be exactly three items
Language CompactLangDetImpl::DetectLanguageSummaryV25(
                        const char* buffer,
                        int buffer_length,
                        bool is_plain_text,
                        const char* tld_hint,       // "id" boosts Indonesian
                        int encoding_hint,          // SJS boosts Japanese
                        Language language_hint,     // ITALIAN boosts it
                        bool allow_extended_lang,
                        int flags,
                        Language plus_one,
                        Language* language3,
                        int* percent3,
                        double* normalized_score3,
                        int* text_bytes,
                        bool* is_reliable) {
  language3[0] = UNKNOWN_LANGUAGE;
  language3[1] = UNKNOWN_LANGUAGE;
  language3[2] = UNKNOWN_LANGUAGE;
  percent3[0] = 100;
  percent3[1] = 0;
  percent3[2] = 0;
  normalized_score3[0] = 0.0;
  normalized_score3[1] = 0.0;
  normalized_score3[2] = 0.0;
  *text_bytes = 0;
  *is_reliable = false;

  // Document totals
  ToteWithReliability doc_tote;   // Reliability = 0..100

  // Vector of packed per-language boosts (just one filled in from hints)
  uint8 lang_hint_boost[EXT_NUM_LANGUAGES + 1];
  memset(lang_hint_boost, 0, sizeof(lang_hint_boost));

  // Apply hints,if any
  if ((tld_hint != NULL) && (tld_hint[0] != '\0')) {
    ApplyTLDHint(lang_hint_boost, tld_hint);
  }
  if (encoding_hint != UNKNOWN_ENCODING) {
    ApplyEncodingHint(lang_hint_boost, encoding_hint);
  }
  if (language_hint != UNKNOWN_LANGUAGE) {
    ApplyLanguageHint(lang_hint_boost, language_hint);
  }


  // Four individual script totals, Latin, Han, other2, other3
  int next_other_tote = 2;
  int tote_num = 0;

  // Four totes for up to four different scripts pending at once
  Tote totes[4];                  // [0] Latn  [1] Hani  [2] other  [3] other
  bool tote_seen[4] = {false, false, false, false};
  int tote_grams[4] = {0, 0, 0, 0};     // Number in partial chunk
  UnicodeLScript tote_script[4] =
    {ULScript_Latin, ULScript_HanCJK, ULScript_Common, ULScript_Common};

  // Loop through text spans in a single script
  ScriptScanner ss(buffer, buffer_length, is_plain_text);
  getone::LangSpan scriptspan;

  scriptspan.text = NULL;
  scriptspan.text_bytes = 0;
  scriptspan.offset = 0;
  scriptspan.script = ULScript_Common;
  scriptspan.lang = UNKNOWN_LANGUAGE;

  int total_text_bytes = 0;
  int textlimit = FLAGS_cld_textlimit << 10;    // in KB
  if (textlimit == 0) {textlimit = 0x7fffffff;}

  int advance_by = 2;                   // Advance 2 bytes
  int advance_limit = textlimit >> 3;   // For first 1/8 of max document

  int initial_word_span = kDefaultWordSpan;
  if (FLAGS_cld_forcewords) {
    initial_word_span = kReallyBigWordSpan;
  }

  // Pick up chunk sizes
  // Smoothwidth is units of quadgrams, about 2.5 chars (unigrams) each
  // Sanity check -- force into a reasonable range
  int chunksizequads = FLAGS_cld_smoothwidth;
  chunksizequads = cld::minint(cld::maxint(chunksizequads, kMinChunkSizeQuads),
                               kMaxChunkSizeQuads);
  int chunksizeunis = (chunksizequads * 5) >> 1;

  // Varying short-span limit doesn't work well -- skips too much beyond 20KB
  // int spantooshortlimit = advance_by * FLAGS_cld_smoothwidth;
  int spantooshortlimit = kShortSpanThresh;

  // For debugging only. Not thread-safe
  prior_lang = UNKNOWN_LANGUAGE;
  prior_unreliable = false;

  // Allocate full-document prediction table for finding repeating words
  int hash = 0;
  int* predict_tbl = new int[kPredictionTableSize];
  if (FlagRepeats(flags)) {
    memset(predict_tbl, 0, kPredictionTableSize * sizeof(predict_tbl[0]));
  }

  // Loop through scriptspans accumulating number of text bytes in each language
  while (ss.GetOneScriptSpanLower(&scriptspan)) {
    UnicodeLScript lscript = scriptspan.script;

    // Echo text if asked to
    if (FLAGS_cld_echotext) {
      PrintHtmlEscapedText(stderr, scriptspan.text, scriptspan.text_bytes);
    }

    // Squeeze out big chunks of text span if asked to
    if (FlagSqueeze(flags)) {
      // Remove repetitive or mostly-spaces chunks
      int newlen;
      int chunksize = 0;    // Use the default
      newlen = CheapSqueezeInplace(scriptspan.text, scriptspan.text_bytes,
                                   chunksize);
      scriptspan.text_bytes = newlen;
    } else {
      // Check now and then to see if we should be squeezing
      if ((total_text_bytes >= kCheapSqueezeTestThresh) &&
          !FlagFinish(flags) &&
          ((getone::kMaxScriptBuffer >> 1) < scriptspan.text_bytes) &&
          CheapSqueezeTriggerTest(scriptspan.text,
                                    scriptspan.text_bytes,
                                    kCheapSqueezeTestLen)) {
        // Recursive call with big-chunk squeezing set
        if (FLAGS_cld_html || FLAGS_dbgscore) {
          fprintf(stderr,
                  "<br>---text_bytes[%d] Recursive(Squeeze)---<br><br>\n",
                  total_text_bytes);
        }
        // Deallocate full-document prediction table
        delete[] predict_tbl;

        return DetectLanguageSummaryV25(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          tld_hint,               // "id" boosts Indonesian
                          encoding_hint,          // SJS boosts Japanese
                          language_hint,          // ITALIAN boosts it
                          allow_extended_lang,
                          flags | kCLDFlagSqueeze,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          text_bytes,
                          is_reliable);
      }
    }

    // Remove repetitive words if asked to
    if (FlagRepeats(flags)) {
      // Remove repetitive words
      int newlen;
      newlen = CheapRepWordsInplace(scriptspan.text, scriptspan.text_bytes,
                                    &hash, predict_tbl);
      scriptspan.text_bytes = newlen;
    }

    // The real scoring
    // Accumulate directly into the document total, or accmulate in one of four
    // chunk totals. The purpose of the multiple chunk totals is to piece
    // together short choppy pieces of text in alternating scripts. One total is
    // dedicated to Latin text, one to Han text, and the other two are dynamicly
    // assigned.
    Language onlylang = cld::kOnlyLanguagePerLScript[lscript];

    if (onlylang != UNKNOWN_LANGUAGE) {
      // This entire script run is in a single language.
      ScoreNilgrams(&scriptspan, cld::PackLanguage(onlylang), &doc_tote,
                   lang_hint_boost, flags, plus_one);
    } else if (cld::kScoreUniPerLScript[lscript] != 0) {
      // This entire script run's languages can be distinguished by uni-grams
      // Accumulate in hani_tote
      tote_num = 1;
      if (!tote_seen[tote_num]) {
        tote_seen[tote_num] = true;
        // Default language gets 1 byte
        total_text_bytes += 1;
        InitScriptToteLang(&totes[tote_num], lscript);
      }
      ScoreUnigrams(&scriptspan, &tote_grams[tote_num], chunksizeunis,
                       &totes[tote_num],
                       &doc_tote, lang_hint_boost,
                       advance_by, flags, &initial_word_span, plus_one);
    } else {
      // This entire script-run's languages can be distinguished by quad-grams
      // Accumulate in latn_tote or script0/1_tote
      tote_num = -1;
      for (int t = 0; t < 4; ++t) {
        if (lscript == tote_script[t]) {
          tote_num = t;
          break;
        }
      }
      if (tote_num < 0) {
        // Need to allocate other0/1
        tote_num = next_other_tote;
        next_other_tote ^= 1;     // Round-robin
        if (tote_seen[tote_num]) {
          // Flush previous
          ScoreChunkIntoDoc2(kToteSwitch[tote_num], advance_by,
                             tote_script[tote_num], &totes[tote_num],
                             &doc_tote, tote_grams[tote_num], lang_hint_boost);
          totes[tote_num].Reinit();
        }
        tote_script[tote_num] = lscript;
      }

      if (!tote_seen[tote_num]) {
        tote_seen[tote_num] = true;
        // Default language gets 1 byte
        total_text_bytes += 1;
        InitScriptToteLang(&totes[tote_num], lscript);
      }

      // The actual accumulation, possibly with word scoring also
      ScoreQuadgrams(&scriptspan, &tote_grams[tote_num],
                        chunksizequads,
                        &totes[tote_num],
                        &doc_tote, lang_hint_boost,
                        advance_by, flags, &initial_word_span, plus_one);
    }

    total_text_bytes += scriptspan.text_bytes;

    // For long documents, do less-dense samples the further along we go.
    // This is to keep speed sublinear in document size.
    if (total_text_bytes > advance_limit) {
      if (total_text_bytes > textlimit) {
        // Don't look at rest of doc
        if (FLAGS_cld_html || FLAGS_dbgscore) {
          fprintf(stderr, "<br>---text_bytes[%d] textlimit %d reached---<br>",
                  total_text_bytes, textlimit);
        }
        break;
      }
      advance_by <<= 1;         // Double advance bytes
      advance_limit <<= 1;      // Double limit until next change
      spantooshortlimit <<= 1;  // Double short-span size
      if (FLAGS_cld_html || FLAGS_dbgscore) {
        fprintf(stderr, "<br>---text_bytes[%d] advance_by doubled to %d---<br>",
                total_text_bytes, advance_by);
      }
    }
  }     // End while (ss.GetOneScriptSpanLower())

  // Deallocate full-document prediction table
  delete[] predict_tbl;

  // Flush pending totals
  for (int tote_num = 0; tote_num < 4; ++tote_num) {
    if (tote_seen[tote_num]) {
      ScoreChunkIntoDoc2(kToteName[tote_num], advance_by,
                         tote_script[tote_num], &totes[tote_num], &doc_tote,
                         tote_grams[tote_num], lang_hint_boost);
    }
  }

  // If extended langauges are disallowed, remove them here
  if (!allow_extended_lang) {
    RemoveExtendedLanguages(&doc_tote);
  }

  // Force close pairs to one or the other
  RefineScoredClosePairs(&doc_tote);


  // Calculate return results
  // Find top three byte counts in tote heap
  int reliable_percent3[3];


  // Cannot use Add, etc. after sorting
  doc_tote.Sort(3);

  ExtractLangEtc(&doc_tote, total_text_bytes,
                 reliable_percent3, language3, percent3, normalized_score3,
                 text_bytes, is_reliable);

  bool have_good_answer = false;
  if (FlagFinish(flags)) {
    // Force a result
    have_good_answer = true;
  } else if (total_text_bytes <= kShortTextThresh) {
    // Don't recurse on short text -- we already did word scores
    have_good_answer = true;
  } else if (*is_reliable &&
             (percent3[0] >= kGoodLang1Percent)) {
    have_good_answer = true;
  } else if (*is_reliable &&
             ((percent3[0] + percent3[1]) >= kGoodLang1and2Percent)) {
    have_good_answer = true;
  }


  if (have_good_answer) {
    // This is the real, non-recursive return

    // Move bytes for unreliable langs to another lang or UNKNOWN
    RemoveUnreliableLanguages(&doc_tote);

    // Redo the result extraction after the removal above
    doc_tote.Sort(3);
    ExtractLangEtc(&doc_tote, total_text_bytes,
                   reliable_percent3, language3, percent3, normalized_score3,
                   text_bytes, is_reliable);

#if 0
    // OLD code, replaced by CalcSummaryLang
    //
    // Suppress ignore-me text, TG_UNKNOWN_LANGUAGE if 2nd or 3rd language
    // Force it to English if first language
    if (language3[2] == TG_UNKNOWN_LANGUAGE) {
      reliable_percent3[2] = 0;
      language3[2] = UNKNOWN_LANGUAGE;
      percent3[2] = 0;
    } else if (language3[1] == TG_UNKNOWN_LANGUAGE) {
      // Move up lower language
      reliable_percent3[1] = reliable_percent3[2];
      language3[1] = language3[2];
      percent3[1] = percent3[2];
      reliable_percent3[2] = 0;
      language3[2] = UNKNOWN_LANGUAGE;
      percent3[2] = 0;
    } else if (language3[0] == TG_UNKNOWN_LANGUAGE) {
      language3[0] = ENGLISH;
    }

    if (language3[0] == UNKNOWN_LANGUAGE) {
      // Last-ditch test for some result, but it is UNKNOWN_LANGUAGE
      // Force it to English (should not happen)
      language3[0] = ENGLISH;
      percent3[0] = 100;
      *is_reliable = true;
    }
#endif


#if 0
    // Scaffolding to reveal subset sequence lang distribution across doc text
    // Track the sequence of language fragments [result currently unused]
    if (FLAGS_cld_html) {
      static const int kMaxSubsetSeq = 12;
      uint8 subseq[kMaxSubsetSeq];
      doc_tote.ExtractSeq(kMaxSubsetSeq, subseq);

      fprintf(stderr, "<br>\nSubset Sequence[%d]: ", kMaxSubsetSeq);
      for (int i = 0; i < kMaxSubsetSeq; ++i) {
        fprintf(stderr, "%s ", ExtLanguageCode(cld::UnpackLanguage(subseq[i])));
        if ((i % 4) == 3) {fprintf(stderr, "&nbsp; ");}
      }
      fprintf(stderr, "&nbsp;&nbsp; ");

      for (int i = 0; i < 3; ++i) {
        if (language3[i] != UNKNOWN_LANGUAGE) {
          fprintf(stderr, "%s.%d(%d%%) ",
                  ExtLanguageCode(language3[i]),
                  reliable_percent3[i],
                  percent3[i]);
        }
      }

      fprintf(stderr, "%d B ", total_text_bytes);
      fprintf(stderr, "<br>\n");
    }
    // End Scaffolding to reveal subset sequence lang distribution
#endif

    Language summary_lang;
    CalcSummaryLang(&doc_tote, total_text_bytes,
                    reliable_percent3, language3, percent3,
                    &summary_lang, is_reliable);

    if (FLAGS_cld_html) {
      for (int i = 0; i < 3; ++i) {
        if (language3[i] != UNKNOWN_LANGUAGE) {
          fprintf(stderr, "%s.%d(%d%%) ",
                  ExtLanguageCode(language3[i]),
                  reliable_percent3[i],
                  percent3[i]);
        }
      }

      fprintf(stderr, "%d B ", total_text_bytes);
      fprintf(stderr, "= %s%c ",
              ExtLanguageName(summary_lang), is_reliable ? ' ' : '*');
      fprintf(stderr, "<br>\n");
    }

    return summary_lang;
  }

  // Not a good answer -- do recursive call to refine
  if (FLAGS_cld_html || FLAGS_dbgscore) {
    // This is what we hope to improve on in the recursive call, if any
    PrintLangs(stderr, language3, percent3, text_bytes, is_reliable);
  }

  // For restriction to Top40 + one, the one is 1st/2nd lang that is not Top40
  // For this purpose, we treate "Ignore" as top40
  Language new_plus_one = UNKNOWN_LANGUAGE;
  if (cld::kIsPackedTop40[cld::PackLanguage(language3[0])] == 0) {
    new_plus_one = language3[0];
  } else if (cld::kIsPackedTop40[cld::PackLanguage(language3[1])] == 0) {
    new_plus_one = language3[1];
  }

  if (total_text_bytes < kShortTextThresh) {
      // Short text: Recursive call with top40 and short set
      if (FLAGS_cld_html || FLAGS_dbgscore) {
        fprintf(stderr, "&nbsp;&nbsp;---text_bytes[%d] "
                "Recursive(Top40/Rep/Short/Words)---<br><br>\n",
                total_text_bytes);
      }
      return DetectLanguageSummaryV25(
                        buffer,
                        buffer_length,
                        is_plain_text,
                        tld_hint,               // "id" boosts Indonesian
                        encoding_hint,          // SJS boosts Japanese
                        language_hint,          // ITALIAN boosts it
                        allow_extended_lang,
                        flags | kCLDFlagTop40 | kCLDFlagRepeats |
                          kCLDFlagShort | kCLDFlagUseWords | kCLDFlagFinish,
                        new_plus_one,
                        language3,
                        percent3,
                        normalized_score3,
                        text_bytes,
                        is_reliable);
  }

  // Longer text: Recursive call with top40 set
  if (FLAGS_cld_html || FLAGS_dbgscore) {
    fprintf(stderr,
            "&nbsp;&nbsp;---text_bytes[%d] Recursive(Top40/Rep)---<br><br>\n",
            total_text_bytes);
  }
  return DetectLanguageSummaryV25(
                        buffer,
                        buffer_length,
                        is_plain_text,
                        tld_hint,               // "id" boosts Indonesian
                        encoding_hint,          // SJS boosts Japanese
                        language_hint,          // ITALIAN boosts it
                        allow_extended_lang,
                        flags | kCLDFlagTop40 | kCLDFlagRepeats |
                          kCLDFlagFinish,
                        new_plus_one,
                        language3,
                        percent3,
                        normalized_score3,
                        text_bytes,
                        is_reliable);
}   // End CompactLangDetImpl::DetectLanguageSummaryV25
