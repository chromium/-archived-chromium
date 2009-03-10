// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/convert_dict/aff_reader.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/tools/convert_dict/hunspell_reader.h"

namespace convert_dict {

namespace {

// Returns true if the given line begins with the given case-sensitive
// NULL-terminated ASCII string.
bool StringBeginsWith(const std::string& str, const char* with) {
  size_t cur = 0;
  while (cur < str.size() && with[cur] != 0) {
    if (str[cur] != with[cur])
      return false;
    cur++;
  }
  return with[cur] == 0;
}

// Collapses runs of spaces to only one space.
void CollapseDuplicateSpaces(std::string* str) {
  int prev_space = false;
  for (size_t i = 0; i < str->length(); i++) {
    if ((*str)[i] == ' ') {
      if (prev_space) {
        str->erase(str->begin() + i);
        i--;
      }
      prev_space = true;
    } else {
      prev_space = false;
    }
  }
}

}  // namespace

AffReader::AffReader(const std::string& filename) {
  file_ = file_util::OpenFile(filename, "r");

  // Default to Latin1 in case the file doesn't specify it.
  encoding_ = "ISO8859-1";
}

AffReader::~AffReader() {
  if (file_)
    file_util::CloseFile(file_);
}

bool AffReader::Read() {
  if (!file_)
    return false;

  // TODO(brettw) handle byte order mark.

  bool got_command = false;
  bool got_first_af = false;
  bool got_first_rep = false;

  has_indexed_affixes_ = false;

  while (!feof(file_)) {
    std::string line = ReadLine(file_);

    // Save comment lines before any commands.
    if (!got_command && !line.empty() && line[0] == '#') {
      intro_comment_.append(line);
      intro_comment_.push_back('\n');
      continue;
    }

    StripComment(&line);
    if (line.empty())
      continue;
    got_command = true;

    if (StringBeginsWith(line, "SET ")) {
      // Character set encoding.
      encoding_ = line.substr(4);
      TrimLine(&encoding_);
    } else if (StringBeginsWith(line, "AF ")) {
      // Affix. The first one is the number of ones following which we don't
      // bother with.
      has_indexed_affixes_ = true;
      if (got_first_af) {
        std::string group(line.substr(3));
        AddAffixGroup(&group);
      } else {
        got_first_af = true;
      }
    } else if (StringBeginsWith(line, "SFX ") ||
               StringBeginsWith(line, "PFX ")) {
      AddAffix(&line);
    } else if (StringBeginsWith(line, "REP ")) {
      // The first rep line is the number of ones following which we don't
      // bother with.
      if (got_first_rep) {
        std::string replacement(line.substr(4));
        AddReplacement(&replacement);
      } else {
        got_first_rep = true;
      }
    } else if (StringBeginsWith(line, "TRY ") ||
               StringBeginsWith(line, "MAP ")) {
      HandleEncodedCommand(line);
    } else if (StringBeginsWith(line, "IGNORE ")) {
      printf("We don't support the IGNORE command yet. This would change how "
        "we would insert things in our lookup table.\n");
      exit(1);
    } else if (StringBeginsWith(line, "COMPLEXPREFIXES ")) {
      printf("We don't support the COMPLEXPREFIXES command yet. This would "
        "mean we have to insert words backwords as well (I think)\n");
      exit(1);
    } else {
      // All other commands get stored in the other commands list.
      HandleRawCommand(line);
    }
  }

  return true;
}

bool AffReader::EncodingToUTF8(const std::string& encoded,
                               std::string* utf8) const {
  std::wstring wide_word;
  if (!CodepageToWide(encoded, encoding(),
                      OnStringUtilConversionError::FAIL, &wide_word))
    return false;
  *utf8 = WideToUTF8(wide_word);
  return true;
}

int AffReader::GetAFIndexForAFString(const std::string& af_string) {
  std::map<std::string, int>::iterator found = affix_groups_.find(af_string);
  if (found != affix_groups_.end())
    return found->second;
  std::string my_string(af_string);
  return AddAffixGroup(&my_string);
}

// We convert the data from our map to an indexed list, and also prefix each
// line with "AF" for the parser to read later.
std::vector<std::string> AffReader::GetAffixGroups() const {
  int max_id = 0;
  for (std::map<std::string, int>::const_iterator i = affix_groups_.begin();
       i != affix_groups_.end(); ++i) {
    if (i->second > max_id)
      max_id = i->second;
  }

  std::vector<std::string> ret;

  ret.resize(max_id);
  for (std::map<std::string, int>::const_iterator i = affix_groups_.begin();
       i != affix_groups_.end(); ++i) {
    // Convert the indices into 1-based.
    ret[i->second - 1] = std::string("AF ") + i->first;
  }

  return ret;
}

int AffReader::AddAffixGroup(std::string* rule) {
  TrimLine(rule);

  // We use the 1-based index of the rule. This matches the way Hunspell
  // refers to the numbers.
  int affix_id = static_cast<int>(affix_groups_.size()) + 1;
  affix_groups_.insert(std::make_pair(*rule, affix_id));
  return affix_id;
}

void AffReader::AddAffix(std::string* rule) {
  TrimLine(rule);
  CollapseDuplicateSpaces(rule);

  // These lines have two forms:
  //   AFX D Y 4       <- First line, lists how many affixes for "D" there are.
  //   AFX D   0 d e   <- Following lines.
  // We want to ensure the two last groups on the last line are encoded in
  // UTF-8, and we want to make sure that the affix identifier "D" is *not*
  // encoded, since that's basically an 8-bit identifier.

  // Count to the third space. Everything after that will be re-encoded. This
  // will re-encode the number on the first line, but that will be a NOP. If
  // there are not that many groups, we won't reencode it, but pass it through.
  int found_spaces = 0;
  std::string token;
  for (size_t i = 0; i < rule->length(); i++) {
    if ((*rule)[i] == ' ') {
      found_spaces++;
      if (found_spaces == 3) {
        size_t part_start = i;
        std::string part;
        if (token[0] != 'Y' && token[0] != 'N') {
          // This token represents a stripping prefix or suffix, which is
          // either a length or a string to be replaced.
          // We also reencode them to UTF-8.
          part_start = i - token.length();
        }
        part = rule->substr(part_start);  // From here to end.

        size_t slash_index = part.find('/');
        if (slash_index != std::string::npos && !has_indexed_affixes()) {
          // This can also have a rule string associated with it following a
          // slash. For example:
          //    PFX P   0 foo/Y  .
          // The "Y" is a flag. For example, the aff file might have a line:
          //    COMPOUNDFLAG Y
          // so that means that this prefix would be a compound one.
          //
          // It expects these rules to use the same alias rules as the .dic
          // file. We've forced it to use aliases, which is a numberical index
          // instead of these character flags, and this needs to be consistent.

          std::string before_flags = part.substr(0, slash_index + 1);

          // After the slash are both the flags, then whitespace, then the part
          // that tells us what to strip.
          std::vector<std::string> after_slash;
          SplitString(part.substr(slash_index + 1), ' ', &after_slash);
          if (after_slash.size() < 2) {
            // Note that we may get a third term here which is the
            // morphological description of this rule. This happens in the tests
            // only, so we can just ignore it.
            printf("ERROR: Didn't get enough after the slash\n");
            return;
          }

          part = StringPrintf("%s%d %s", before_flags.c_str(),
                  GetAFIndexForAFString(after_slash[0]),
                  after_slash[1].c_str());
        }

        // Reencode from here
        std::string reencoded;
        if (!EncodingToUTF8(part, &reencoded))
          break;

        *rule = rule->substr(0, part_start) + reencoded;
        break;
      }
      token.clear();
    } else {
      token.push_back((*rule)[i]);
    }
  }

  affix_rules_.push_back(*rule);
}

void AffReader::AddReplacement(std::string* rule) {
  TrimLine(rule);

  std::string utf8rule;
  if (!EncodingToUTF8(*rule, &utf8rule))
    return;

  std::vector<std::string> split;
  SplitString(utf8rule, ' ', &split);

  // There should be two parts.
  if (split.size() != 2)
    return;

  // Underscores are used to represent spaces
  // (since the line is parsed on spaces).
  std::replace(split[0].begin(), split[0].end(), '_', ' ');
  std::replace(split[1].begin(), split[1].end(), '_', ' ');

  replacements_.push_back(std::make_pair(split[0], split[1]));
}

void AffReader::HandleRawCommand(const std::string& line) {
  other_commands_.push_back(line);
}

void AffReader::HandleEncodedCommand(const std::string& line) {
  std::string utf8;
  if (EncodingToUTF8(line, &utf8))
    other_commands_.push_back(utf8);
}

}  // namespace convert_dict
