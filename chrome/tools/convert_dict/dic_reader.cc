// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/tools/convert_dict/dic_reader.h"

#include <algorithm>
#include <set>

#include "base/string_util.h"
#include "chrome/tools/convert_dict/aff_reader.h"
#include "chrome/tools/convert_dict/hunspell_reader.h"

namespace convert_dict {

namespace {

// Maps each unique word to the unique affix group IDs associated with it.
typedef std::map<std::string, std::set<int> > WordSet;

void SplitDicLine(const std::string& line, std::vector<std::string>* output) {
  // We split the line on a slash not preceeded by a backslash. A slash at the
  // beginning of the line is not a separator either.
  size_t slash_index = line.size();
  for (size_t i = 0; i < line.size(); i++) {
    if (line[i] == '/' && i > 0 && line[i - 1] != '\\') {
      slash_index = i;
      break;
    }
  }

  output->clear();

  // Everything before the slash index is the first term. We also need to
  // convert all escaped slashes ("\/" sequences) to regular slashes.
  std::string word = line.substr(0, slash_index);
  ReplaceSubstringsAfterOffset(&word, 0, "\\/", "/");
  output->push_back(word);

  // Everything (if anything) after the slash is the second.
  if (slash_index < line.size() - 1)
    output->push_back(line.substr(slash_index + 1));
}

}  // namespace

DicReader::DicReader(const std::string& filename) {
  fopen_s(&file_, filename.c_str(), "r");
}

DicReader::~DicReader() {
  if (file_)
    fclose(file_);
}

bool DicReader::Read(AffReader* aff_reader) {
  if (!file_)
    return false;

  bool got_count = false;
  int line_number = 0;

  WordSet word_set;
  while (!feof(file_)) {
    std::string line = ReadLine(file_);
    line_number++;
    StripComment(&line);
    if (line.empty())
      continue;

    if (!got_count) {
      // Skip the first nonempty line, this is the line count. We don't bother
      // with it and just read all the lines.
      got_count = true;
      continue;
    }

    std::vector<std::string> split;
    SplitDicLine(line, &split);
    if (split.size() == 0 || split.size() > 2) {
      printf("Line %d has extra slashes in the dic file\n", line_number);
      return false;
    }

    // The first part is the word, the second (optional) part is the affix. We
    // always use UTF-8 as the encoding to simplify life.
    std::string utf8word;
    if (!aff_reader->EncodingToUTF8(split[0], &utf8word)) {
      printf("Unable to convert line %d from %s to UTF-8 in the dic file\n",
             line_number, aff_reader->encoding());
      return false;
    }

    // We always convert the affix to an index. 0 means no affix.
    int affix_index = 0;
    if (split.size() == 2) {
      // Got a rule, which is the stuff after the slash. The line may also have
      // an optional term separated by a tab. This is the morphological
      // description. We don't care about this (it is used in the tests to
      // generate a nice dump), so we remove it.
      size_t split1_tab_offset = split[1].find('\t');
      if (split1_tab_offset != std::string::npos)
        split[1] = split[1].substr(0, split1_tab_offset);

      if (aff_reader->has_indexed_affixes())
        affix_index = atoi(split[1].c_str());
      else
        affix_index = aff_reader->GetAFIndexForAFString(split[1]);
    }

    WordSet::iterator found = word_set.find(utf8word);
    if (found == word_set.end()) {
      std::set<int> affix_vector;
      affix_vector.insert(affix_index);
      word_set.insert(std::make_pair(utf8word, affix_vector));
    } else {
      found->second.insert(affix_index);
    }
  }

  // Make sure the words are sorted, they may be unsorted in the input.
  for (WordSet::iterator word = word_set.begin(); word != word_set.end();
       ++word) {
    std::vector<int> affixes;
    for (std::set<int>::iterator aff = word->second.begin();
         aff != word->second.end(); ++aff)
      affixes.push_back(*aff);

    // Double check that the affixes are sorted. This isn't strictly necessary
    // but it's nice for the file to have a fixed layout.
    std::sort(affixes.begin(), affixes.end());
    words_.push_back(std::make_pair(word->first, affixes));
  }

  // Double-check that the words are sorted.
  std::sort(words_.begin(), words_.end());
  return true;
}

}  // namespace convert_dict
