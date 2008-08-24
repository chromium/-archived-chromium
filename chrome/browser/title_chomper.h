// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TITLE_CHOMPER_H__
#define CHROME_BROWSER_TITLE_CHOMPER_H__

#include <string>
#include <vector>

class TitleChomper {
 public:
  TitleChomper();

  void AddTitle(const std::wstring& title);

  void ChompTitles(std::vector<std::wstring>* chomped_titles);

 private:
  void GenerateChompedTitle(const std::wstring& title,
                            std::wstring* chomped_title);

 private:
  std::vector<std::wstring> titles_;
  std::vector<std::wstring> last_words_;
  std::wstring last_title_;
};

#endif  // #ifndef CHROME_BROWSER_TITLE_CHOMPER_H__

