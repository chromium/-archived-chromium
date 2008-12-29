// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_CONVERT_DICT_DIC_READER_H__
#define CHROME_TOOLS_CONVERT_DICT_DIC_READER_H__

#include <stdio.h>
#include <map>
#include <string>
#include <vector>

namespace convert_dict {

class AffReader;

// Reads Hunspell .dic files.
class DicReader {
 public:
  // Associated with each word is a list of affix group IDs. This will typically
  // be only one long, but may be more if there are multiple groups of
  // independent affix rules for a base word.
  typedef std::pair<std::string, std::vector<int> > WordEntry;
  typedef std::vector<WordEntry> WordList;

  DicReader(const std::string& filename);
  ~DicReader();

  // Non-numeric affixes will be added to the given AffReader and converted into
  // indices.
  bool Read(AffReader* aff_reader);

  // Returns the words read by Read(). These will be in order.
  const WordList& words() const { return words_; }

 private:
  FILE* file_;
  FILE* additional_words_file_;

  // Contains all words and their corresponding affix index.
  WordList words_;
};

}  // namespace convert_dict

#endif  // CHROME_TOOLS_CONVERT_DICT_DIC_READER_H__

