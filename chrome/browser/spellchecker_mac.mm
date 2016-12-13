// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the interface defined in spellchecker_platform_engine.h
// for the OS X platform.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/spellchecker_common.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "base/time.h"
#include "base/histogram.h"
#include "base/sys_string_conversions.h"

using base::TimeTicks;
namespace {
// The number of characters in the first part of the language code.
const unsigned int kShortLanguageCodeSize = 2;

// A private utility function to convert hunspell language codes to os x
// language codes.
NSString* ConvertLanguageCodeToMac(const std::string& lang_code) {
  NSString* whole_code = base::SysUTF8ToNSString(lang_code);

  if ([whole_code length] > kShortLanguageCodeSize) {
    NSString* lang_code = [whole_code
                           substringToIndex:kShortLanguageCodeSize];
    // Add 1 here to skip the underscore.
    NSString* region_code = [whole_code
                             substringFromIndex:(kShortLanguageCodeSize + 1)];

    // Check for the special case of en-US and pt-PT, since os x lists these
    // as just en and pt respectively.
    // TODO(pwicks): Find out if there are other special cases for languages
    // not installed on the system by default. Are there others like pt-PT?
    if (([lang_code isEqualToString:@"en"] &&
       [region_code isEqualToString:@"US"]) ||
        ([lang_code isEqualToString:@"pt"] &&
       [region_code isEqualToString:@"PT"])) {
      return lang_code;
    }

    // Otherwise, just build a string that uses an underscore instead of a
    // dash between the language and the region code, since this is the
    // format that os x uses.
    NSString* os_x_language =
        [NSString stringWithFormat:@"%@_%@", lang_code, region_code];
    return os_x_language;
  } else {
    // This is just a language code with the same format as os x
    // language code.
    return whole_code;
  }
}
} // namespace

namespace SpellCheckerPlatform {

bool SpellCheckerAvailable() {
  // If this file was compiled, then we know that we are on OS X 10.5 at least
  // and can safely return true here.
  return true;
}

void Init() {
  // This call must be made before the call to
  // [NSSpellchecker sharedSpellChecker] or it will return nil.
  [NSApplication sharedApplication];
}

bool PlatformSupportsLanguage(const std::string& current_language) {
  // First, convert Language to an osx lang code NSString.
  NSString* mac_lang_code = ConvertLanguageCodeToMac(current_language);

  // Then grab the languages available.
  NSArray* availableLanguages;
  availableLanguages = [[NSSpellChecker sharedSpellChecker]
                        availableLanguages];

  // Return true if the given languange is supported by os x.
  return [availableLanguages containsObject:mac_lang_code];
}

void SetLanguage(const std::string& lang_to_set) {
  NSString* NS_lang_to_set = ConvertLanguageCodeToMac(lang_to_set);
  [[NSSpellChecker sharedSpellChecker] setLanguage:NS_lang_to_set];
}

bool CheckSpelling(const std::string& word_to_check) {
  // [[NSSpellChecker sharedSpellChecker] checkSpellingOfString] returns an
  // NSRange that we can look at to determine if a word is misspelled.
  NSRange spell_range = {0,0};

  // Convert the word to an NSString.
  NSString* NS_word_to_check = base::SysUTF8ToNSString(word_to_check);
  // Check the spelling, starting at the beginning of the word.
  spell_range = [[NSSpellChecker sharedSpellChecker]
                 checkSpellingOfString:NS_word_to_check startingAt:0];

  // If the length of the misspelled word == 0,
  // then there is no misspelled word.
  bool word_correct = (spell_range.length == 0);
  return word_correct;
}

void FillSuggestionList(const std::string& wrong_word,
                              std::vector<std::wstring>* optional_suggestions) {
  NSString* NS_wrong_word = base::SysUTF8ToNSString(wrong_word);
  TimeTicks begin_time = TimeTicks::Now();
  // The suggested words for |wrong_word|.
  NSArray* guesses =
      [[NSSpellChecker sharedSpellChecker] guessesForWord:NS_wrong_word];
  DHISTOGRAM_TIMES("Spellcheck.SuggestTime",
                   TimeTicks::Now() - begin_time);

  for (int i = 0; i < static_cast<int>([guesses count]); i++) {
    if (i < kMaxSuggestions) {
      optional_suggestions->push_back(base::SysNSStringToWide(
                                      [guesses objectAtIndex:i]));
    }
  }
}

void AddWord(const std::wstring& word) {
   NSString* word_to_add = base::SysWideToNSString(word);
  [[NSSpellChecker sharedSpellChecker] learnWord:word_to_add];
}

void RemoveWord(const std::wstring& word) {
  NSString *word_to_remove = base::SysWideToNSString(word);
  [[NSSpellChecker sharedSpellChecker] unlearnWord:word_to_remove];
}
}  // namespace SpellCheckerPlatform

