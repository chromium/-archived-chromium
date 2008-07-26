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

#ifndef CHROME_TOOLS_CONVERT_DICT_AFF_READER_H__
#define CHROME_TOOLS_CONVERT_DICT_AFF_READER_H__

#include <map>
#include <stdio.h>
#include <string>
#include <vector>

namespace convert_dict {

class AffReader {
 public:
  AffReader(const std::string& filename);
  ~AffReader();

  bool Read();

  // Returns whether this file uses indexed affixes, or, on false, whether the
  // rule string will be specified literally in the .dic file. This must be
  // called after Read().
  bool has_indexed_affixes() const { return has_indexed_affixes_; }

  // Returns a string representing the encoding of the dictionary. This will
  // default to ISO-8859-1 if the .aff file does not specify it.
  const char* encoding() const { return encoding_.c_str(); }

  // Converts the given string from the file encoding to UTF-8, returning true
  // on success.
  bool EncodingToUTF8(const std::string& encoded, std::string* utf8) const;

  // Adds a new affix string, returning the index. If it already exists, returns
  // the index of the existing one. This is used to convert .dic files which
  // list the
  // You must not call this until after Read();
  int GetAFIndexForAFString(const std::string& af_string);

  // Getters for the computed data.
  const std::string& comments() const { return intro_comment_; }
  const std::vector<std::string>& affix_rules() const { return affix_rules_; }
  const std::vector< std::pair<std::string, std::string> >&
      replacements() const {
    return replacements_;
  }
  const std::vector<std::string>& other_commands() const {
    return other_commands_;
  }

  // Returns the affix groups ("AF" lines) for this file. The indices into this
  // are 1-based, but we don't use the 0th item, so lookups will have to
  // subtract one to get the index. This is how hunspell stores this data.
  std::vector<std::string> GetAffixGroups() const;

 private:
  // Command-specific handlers. These are given the string folling the
  // command. The input rule may be modified arbitrarily by the function.
  int AddAffixGroup(std::string* rule);  // Returns the new affix group ID.
  void AddAffix(std::string* rule);  // SFX/PFX
  void AddReplacement(std::string* rule);
  //void HandleFlag(std::string* rule);

  // Used to handle "other" commands. The "raw" just saves the line as-is.
  // The "encoded" version converts the line to UTF-8 and saves it.
  void HandleRawCommand(const std::string& line);
  void HandleEncodedCommand(const std::string& line);

  FILE* file_;

  // Comments from the beginning of the file. This is everything before the
  // first command. We want to store this since it often contains the copyright
  // information.
  std::string intro_comment_;

  // Encoding of the source words.
  std::string encoding_;

  // Affix rules. These are populated by "AF" commands. The .dic file can refer
  // to these by index. They are indexed by their string value (the list of
  // characters representing rules), and map to the numeric affix IDs.
  //
  // These can also be added using GetAFIndexForAFString.
  std::map<std::string, int> affix_groups_;

  // True when the affixes were specified in the .aff file using indices. The
  // dictionary reader uses this to see how it should treat the stuff after the
  // word on each line.
  bool has_indexed_affixes_;

  // SFX and PFX commands. This is a list of each of those lines in the order
  // they appear in the file. They have been re-encoded.
  std::vector<std::string> affix_rules_;

  // Replacement commands. The first string is a possible input, and the second
  // is the replacment.
  std::vector< std::pair<std::string, std::string> > replacements_;

  // All other commands.
  std::vector<std::string> other_commands_;
};

}  // namespace convert_dict

#endif  // CHROME_TOOLS_CONVERT_DICT_AFF_READER_H__
