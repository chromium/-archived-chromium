// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is for i18n. It contains two enums, namely Language and
// Encoding, where Language is the linguistic convention, and Encoding
// contains information on both language encoding and character set.
//
// The language and encoding are both based on Teragram's conventions,
// except for some common ISO-8859 encodings that are not detected by
// Teragram but might be in the future.
//
// This file also includes functions that do mappings among
// Language/Encoding enums, language/encoding string names (typically
// the output from Language Encoding identifier), and language codes
// (iso 639), and two-letter country codes (iso 3166)
//
// NOTE: Both Language and Encoding enums should always start from
// zero value. This assumption has been made and used.
//

#ifndef I18N_ENCODINGS_LANG_ENC_H__
#define I18N_ENCODINGS_LANG_ENC_H__

#include "third_party/cld/bar/toolbar/cld/i18n/languages/public/languages.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/public/encodings.h"


// EncodingsForLanguage
// --------------------
//
// Given the language, returns a pointer to an array of encodings this
// language supports. Typically, the encs array has at least one
// element: UNKNOWN_ENCODING, which is always the last element of the
// array. The first encoding is the default encoding of the language.
// Return NULL if the input is invalid.
//
// Note: The output encoding array does not include ASCII_7BIT, UTF8
// or UNICODE which are good for all languages. TODO: Find out whether
// it is better to include ASCII_7BIT, UTF8 and UNICODE or leave them
// as special cases.
//
const Encoding* EncodingsForLanguage(Language lang);


// DefaultEncodingForLanguage
// --------------------------
//
// Given the language, returns the default encoding for the language
// via the argument encoding.
//
// The function returns true if the input lang is valid. Otherwise,
// false is returned, and encoding is set to UNKNOWN_ENCODING.
//
bool DefaultEncodingForLanguage(Language lang,
                                Encoding *encoding);

// LanguagesForEncoding
// --------------------
//
// Given the encoding, returns a pointer to an array of languages this
// encoding supports. Typically, the langs array has at least one
// element: UNKNOWN_LANGUAGE, which is always the last element of the
// array. The first language in the array if the most popular
// language for that encoding. NULL is returned if the input is
// invalid.
//
// Note: For ASCII_7BIT, UNICODE and UTF8, only ENGLISH and
// UNKNOWN_LANGUAGE are returned. TODO: Find out whether to return all
// the languages or to treat these two encodings as special cases.
//
// For other known encodings, ENGLISH is always included. This is
// because English (Latin) characters are included in each encoding.
//
const Language* LanguagesForEncoding(Encoding enc);

// DefaultLanguageForEncoding
// --------------------------
//
// Given the encoding, returns the default language for that encoding
// via the argument language.
//
// The function returns true if the input enc is valid. Otherwise,
// false is returned, and language is set to UNKNOWN_LANGUAGE.
//
// Note, this function is more useful for the encodings that have only
// one corresponding language i.e. shift_jis => Japanese. There are
// cases that multiple langauges have the same encoding, for which the
// default language is an arbitrary choice from them.
//
bool DefaultLanguageForEncoding(Encoding enc, Language* language);

//
// IsLangEncCompatible
// -------------------
//
// This function is to determine whether the input language and
// encoding are compatible. For example, FRENCH and LATIN1 are
// compatible, but FRENCH and GB are not.
//
// If either lang or enc is invalid return false.
// If either lang is unknown, return true.
//    (e.g. we can detect a page's encoding as latin1 from metatag info, but
//     cannot derive it language since there are more than one
//     language encoding in Latin1 )
// If language is known, but encoding is unknown, return false.
//    (return true will do us no good since we cannot convert to UTF8 anyway)
// If enc is unicode or utf8, return true.
// Otherwise check if lang is supported by enc and enc supported by
// lang.
//
bool IsLangEncCompatible(Language lang, Encoding enc);

//
// DominantLanguageFromEncoding
// ----------------------------
//
// This function determine if there exists a dominant language for the
// input encoding. For example, the encoding GB has a dominant
// language (Chinese), but Latin1 does not.
//
// The word "dominant" is used here because English characters are
// included in each encoding.
//
// If there is no dominant langauge for the encoding, such as Latin1,
// UNKNOWN_LANGUAGE is returned.
//
Language DominantLanguageFromEncoding(Encoding enc);

// LanguageCode
// ------------------------
// Given the Language and Encoding, return language code with dialects
// (>= 2 letters).  Encoding is necessary to disambiguate between
// Simplified and Traditional Chinese.
//
// See the note on Chinese Language Codes in
// i18n/languages/public/languages.h
// for the details.

const char* LanguageCode(Language lang, Encoding enc);

//
// IsEncodingWithSupportedLanguage()
// ---------------------------------
//
// There are some encoding listed here just because they are commonly
// used.  There is no interface language for them yet. They are not
// detected by Teragram, but can be detected from the meta info of the
// HTML page.
//
// For example, we have list ARABIC_ENCODING but there is no arabic in
// the Language enum. If the user input an Arabic query from Google
// main page, Netscape will just send the raw bytes to GWS, and GWS
// will treat them as Latin1.  Therefore, there is no use to detect
// ARABIC_ENCODING for indexing, since they will never match the
// queries which are treated as Latin1 by GWS. On the contrary, if we
// treat page with ARABIC_ENCODING as UNKNOWN_ENCODING, Google will
// fall them through as Latin1 in indexing time. And there might be a
// match for some ARABIC queries which are also treated as Latin1 by
// GWS. In fact, some people are relying on this feature to do Arabic
// searches.
//
// Thus for these type of encoding, before we have the UI support for
// their language and have a pretty comprehensive language/encoding
// identification quality, it is better to revert them as
// UNKNOWN_ENCODING.
//
// This function checks whether the input encoding is one with
// an interface language.
bool IsEncodingWithSupportedLanguage(Encoding enc);


//
// LangsFromCountryCode and EncFromCountryCode
// -------------------------------------------
//
// These two functions return the possible languages and encodings,
// respectively, according to the input country code, which is a
// 2-letter string. The country code is usually specified in the url
// of a document.
//
//

// LangsFromCountryCode
// --------------------
//
// This function takes a string of arbitrary length. It treats the
// first 2 bytes of the string as the country code, as defined in iso
// 3166-1993 (E).  It returns, via arguments, an array of the
// languages that are popular in that country, roughly in order of
// popularity, together with the size of the array.
//
// This function returns true if we have language information for
// country_code.  Otherwise, it returns false.
//
bool LangsFromCountryCode(const char* country_code,
                          const Language** lang_arry,
                          int* num_langs);


//
// EncFromCountryCode
// ------------------
//
// This function takes a string of arbitrary length. It treats the
// first 2 bytes of that string as the country code, as defined in iso
// 3166-1993 (E). It sets *enc to the encoding that is
// most often used for the languages spoken in that country.
//
// This function returns true if we have encoding information for
// country_code.  Otherwise, it returns false, and *enc is set to
// UNKNOWN_ENCODING.
//
bool EncFromCountryCode(const char* country_code, Encoding* enc);



// VisualType
// ----------
//
// Right-to-left documents may be in logical or visual order. When they
// are in visual order we convert them to logical order before processing.
// This enum lists the types of visual document we can encounter.
// Some, but not all, documents in Hebrew/Arabic/Persian etc. will be visual.
// The other documents in those languages, and all documents in non-RTL
// languages, will be NOT_VISUAL_DOCUMENT.
// See http://wiki/Main/RtlLanguages for details.
enum VisualType {
  NOT_VISUAL_DOCUMENT = 0,
  VISUAL_HEBREW_HTML,  // HTML documents in the legacy visual order.
  CONVERTED_RTL_PDF,   // Converted RTL PDFs, which are always visual.
};

VisualType default_visualtype();

// VisualTypeName
// --------------
//
// Given the visual type, returns a string name useful for debug output.
const char* VisualTypeName(VisualType visualtype);



// InitLangEnc
// -----------
//
// Ensures the LangEnc module has been initialized.  Normally this
// happens during InitGoogle, but this allows access for scripts that
// don't support InitGoogle. InitLangEnc calls InitEncodings (see
// i18n/encodings/public/encodings.h) and also initializes data
// structures used in lang_enc.cc.
//
void InitLangEnc();

#endif  // I18N_ENCODINGS_LANG_ENC_H__
