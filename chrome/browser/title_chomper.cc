// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/title_chomper.h"

#include "base/logging.h"
#include "base/word_iterator.h"

TitleChomper::TitleChomper() {
}

void TitleChomper::AddTitle(const std::wstring& title) {
  titles_.push_back(title);
}

void TitleChomper::ChompTitles(std::vector<std::wstring>* chomped_titles) {
  std::vector<std::wstring>::iterator title;
  for (title = titles_.begin(); title != titles_.end(); ++title) {
    std::wstring chomped;
    GenerateChompedTitle(*title, &chomped);
    chomped_titles->push_back(chomped);
  }
}

void TitleChomper::GenerateChompedTitle(const std::wstring& title,
                                        std::wstring* chomped_title) {
  // We don't chomp identical titles, since they would chomp to nothing!
  if (title == last_title_) {
    *chomped_title = title;
    last_words_.clear();
    return;
  }
  last_title_ = title;

  // TODO(beng): fix locale
  WordIterator iter(title, WordIterator::BREAK_WORD);
  if (!iter.Init())
    return;

  int chomp_point = 0;
  size_t count = 0;

  std::vector<std::wstring> words;

  bool record_next_point = false;
  bool found_chomp_point = false;

  while (iter.Advance()) {
    if (iter.IsWord()) {
      const std::wstring fragment = iter.GetWord();
      words.push_back(fragment);

      size_t last_words_size = last_words_.size();
      bool word_mismatch =
        (count < last_words_size && last_words_.at(count) != fragment) ||
        (last_words_size > 0 && count >= last_words_size);
      if (!found_chomp_point && word_mismatch) {
        // Need to wait until the next word point so that we skip any spaces or
        // punctuation at the start of the string.
        record_next_point = true;
      }
      ++count;
    }
    if (!found_chomp_point && record_next_point) {
      chomp_point = iter.prev();
      found_chomp_point = true;
    }
  }
  last_words_ = words;
  chomped_title->assign(title.substr(chomp_point));
}

