// Copyright 2008 Google Inc. All Rights Reserved.

#ifndef CHROME_THIRD_PARTY_HUNSPELL_GOOGLE_BDICT_WRITER_H__
#define CHROME_THIRD_PARTY_HUNSPELL_GOOGLE_BDICT_WRITER_H__

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace hunspell {

class DicNode;

// Class for creating a binary dictionary file from data read from Hunspell
// spellchecker files.
class BDictWriter {
 public:
  typedef std::vector< std::pair<std::string, std::vector<int> > > WordList;

  BDictWriter();
  ~BDictWriter();

  // Affix setters.
  void SetComment(const std::string& comment) {
    comment_ = comment;
  }
  void SetAffixRules(const std::vector<std::string>& rules) {
    affix_rules_ = rules;
  }
  void SetAffixGroups(const std::vector<std::string>& groups) {
    affix_groups_ = groups;
  }
  void SetReplacements(
      const std::vector< std::pair<std::string, std::string> >& replacements) {
    replacements_ = replacements;
  }
  void SetOtherCommands(const std::vector<std::string>& commands) {
    other_commands_ = commands;
  }

  // The words must be sorted already.
  void SetWords(const WordList& words);

  // Returns the serialized data for the entire file. You must call all the
  // setters above before this.
  std::string GetBDict() const;

 private:
  // Converts the affix members to a string.
  void SerializeAff(std::string* output) const;

  // Affix members.
  std::string comment_;
  std::vector<std::string> affix_rules_;
  std::vector<std::string> affix_groups_;
  std::vector< std::pair<std::string, std::string> > replacements_;
  std::vector<std::string> other_commands_;

  // Root of the generated trie. Filled by SetWords.
  DicNode* trie_root_;

  DISALLOW_EVIL_CONSTRUCTORS(BDictWriter);
};

}  // namespace hunspell

#endif  // CHROME_THIRD_PARTY_HUNSPELL_GOOGLE_BDICT_WRITER_H__
