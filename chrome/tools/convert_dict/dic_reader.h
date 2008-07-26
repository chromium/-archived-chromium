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

  // Contains all words and their corresponding affix index.
  WordList words_;
};

}  // namespace convert_dict

#endif  // CHROME_TOOLS_CONVERT_DICT_DIC_READER_H__
