// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// If linux ever gains a platform specific spellchecker, it will be
// implemented here.

#include "chrome/browser/spellchecker_common.h"

namespace SpellCheckerPlatform {

bool SpellCheckerAvailable() {
  // As of Summer 2009, there is no commonly accepted platform spellchecker
  // for linux, so we'll return false here.
  return false;
}

// The following methods are just stubs to keep the linker happy.
bool PlatformSupportsLanguage(const Language& current_language) {
  return false;
}

void Init() {
}

void SetLanguage(const Language& lang_to_set) {
}

bool CheckSpelling(const std::string& word_to_check) {
  return false;
}

void FillSuggestionList(const std::string& wrong_word,
                        std::vector<std::wstring>* optional_suggestions) {
}

void AddWord(const std::wstring& word) {
}

void RemoveWord(const std::wstring& word) {
}

}  // namespace SpellCheckerPlatform
