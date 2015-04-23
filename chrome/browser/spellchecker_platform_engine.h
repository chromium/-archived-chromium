// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface that any platform-specific spellchecker
// needs to implement in order to be used by the browser.

#ifndef CHROME_BROWSER_SPELLCHECKER_PLATFORM_ENGINE_H_
#define CHROME_BROWSER_SPELLCHECKER_PLATFORM_ENGINE_H_

#include <string>
#include <vector>

#include "chrome/browser/spellchecker_common.h"

namespace SpellCheckerPlatform {
// Returns true if there is an platform-specific spellchecker.
bool SpellCheckerAvailable();
// Do any initialization needed for spellchecker.
void Init();
// TODO(pwicks): should we add a companion to this, TearDown or something?

// Translates the codes used by chrome to the language codes used by os x
// and checks the given language agains the languages that the current system
// supports. If the platform-specific spellchecker supports the language,
// then returns true, otherwise false.
bool PlatformSupportsLanguage(const std::string& current_language);
// Sets the language for the platform-specific spellchecker.
void SetLanguage(const std::string& lang_to_set);
// Checks the spelling of the given string, using the platform-specific
// spellchecker. Returns true if the word is spelled correctly.
bool CheckSpelling(const std::string& word_to_check);
// Fills the given vector |optional_suggestions| with a number (up to
// kMaxSuggestions, which is defined in spellchecker_common.h) of suggestions
// for the string |wrong_word|.
void FillSuggestionList(const std::string& wrong_word,
                        std::vector<std::wstring>* optional_suggestions);
// Adds the given word to the platform dictionary.
void AddWord(const std::wstring& word);
// Remove a given word from the platform dictionary.
void RemoveWord(const std::wstring& word);
}

#endif  // CHROME_BROWSER_SPELLCHECKER_PLATFORM_ENGINE_H_
