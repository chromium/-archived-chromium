// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_COMMON_H_
#define CHROME_BROWSER_SPELLCHECKER_COMMON_H_

#include <string>
#include <vector>

// Some constants and typedefs that are common to all spellchecker
// files/classes/backends/platforms/whatever.

typedef std::string Language;
typedef std::vector<Language> Languages;

static const int kMaxSuggestions = 5;  // Max number of dictionary suggestions.

static const int kMaxAutoCorrectWordSize = 8;

#endif  // CHROME_BROWSER_SPELLCHECKER_COMMON_H_

