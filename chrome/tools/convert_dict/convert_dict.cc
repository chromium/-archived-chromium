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

// This tool converts Hunspell .aff/.dic pairs to a combined binary dictionary
// format (.bdic). This format is more compact, and can be more efficiently
// read by the client application.
//
// We do this conversion manually before publishing dictionary files. It is not
// part of any build process.
//
// See PrintHelp() below for usage.

#include <stdio.h>

#include "base/icu_util.h"
#include "base/string_util.h"
#include "chrome/third_party/hunspell/google/bdict_reader.h"
#include "chrome/third_party/hunspell/google/bdict_writer.h"
#include "chrome/tools/convert_dict/aff_reader.h"
#include "chrome/tools/convert_dict/dic_reader.h"

namespace {

// Compares the given word list with the serialized trie to make sure they
// are the same.
bool VerifyWords(const convert_dict::DicReader::WordList& org_words,
                 const std::string& serialized) {
  hunspell::BDictReader reader;
  if (!reader.Init(reinterpret_cast<const unsigned char*>(serialized.data()),
                   serialized.size())) {
    printf("BDict is invalid\n");
    return false;
  }
  hunspell::WordIterator iter = reader.GetAllWordIterator();

  int affix_ids[hunspell::BDict::MAX_AFFIXES_PER_WORD];

  static const int buf_size = 128;
  char buf[buf_size];
  for (size_t i = 0; i < org_words.size(); i++) {
    int affix_matches = iter.Advance(buf, buf_size, affix_ids);
    if (affix_matches == 0)
      return false;  // Found the end before we expectd.
    if (org_words[i].first != buf)
      return false;  // Word doesn't match.

    if (affix_matches != static_cast<int>(org_words[i].second.size()))
      return false;  // Different number of affix indices.

    // Check the individual affix indices.
    for (size_t affix_index = 0; affix_index < org_words[i].second.size();
         affix_index++) {
      if (affix_ids[affix_index] != org_words[i].second[affix_index])
        return false;  // Index doesn't match.
    }
  }

  return true;
}

int PrintHelp() {
  printf("Usage: convert_dict <dicfile base name>\n\n");
  printf("Example:\n  convert_dict en-US\nwill read en-US.dic / en-US.aff and\n");
  printf("generate en-US.bdic\n\n");
  return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 2)
    return PrintHelp();

  icu_util::Initialize();

  std::string file_base = argv[1];

  std::string aff_name = file_base + ".aff";
  printf("Reading %s ...\n", aff_name.c_str());
  convert_dict::AffReader aff_reader(aff_name.c_str());
  if (!aff_reader.Read()) {
    printf("Unable to read the aff file.\n");
    return 1;
  }

  std::string dic_name = file_base + ".dic";
  printf("Reading %s ...\n", dic_name.c_str());
  convert_dict::DicReader dic_reader(dic_name.c_str());
  if (!dic_reader.Read(&aff_reader)) {
    printf("Unable to read the dic file.\n");
    return 1;
  }

  hunspell::BDictWriter writer;
  writer.SetComment(aff_reader.comments());
  writer.SetAffixRules(aff_reader.affix_rules());
  writer.SetAffixGroups(aff_reader.GetAffixGroups());
  writer.SetReplacements(aff_reader.replacements());
  writer.SetOtherCommands(aff_reader.other_commands());
  writer.SetWords(dic_reader.words());

  printf("Serializing...\n");
  std::string serialized = writer.GetBDict();

  printf("Verifying...\n");
  if (!VerifyWords(dic_reader.words(), serialized)) {
    printf("ERROR converting, the dictionary does not check out OK.");
    return 1;
  }

  std::string out_name = file_base + ".bdic";
  printf("Writing %s ...\n", out_name.c_str());
  FILE* out_file;
  fopen_s(&out_file, out_name.c_str(), "wb");
  if (!out_file) {
    printf("ERROR writing file\n");
    return 1;
  }
  fwrite(&serialized[0], 1, serialized.size(), out_file);
  fclose(out_file);

  return 0;
}
