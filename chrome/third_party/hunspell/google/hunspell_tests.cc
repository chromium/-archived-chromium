// Copyright 2008 Google Inc. All Rights Reserved.

#include <string>
#include <vector>

#include "base/path_service.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/third_party/hunspell/google/bdict_reader.h"
#include "chrome/third_party/hunspell/google/bdict_writer.h"
#include "chrome/third_party/hunspell/src/hunspell/hunspell.hxx"
#include "chrome/tools/convert_dict/aff_reader.h"
#include "chrome/tools/convert_dict/dic_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

// Note that this test can be run using Hunspell without our modifications so
// it can be compared to with. It keys off of HUNSPELL_CHROME_CLIENT just like
// the Hunspell library does.

namespace {

class HunspellTest : public testing::Test {
 public:

  void RunTest(const char* test_base_name) const;

 private:
  void SetUp() {
    PathService::Get(base::DIR_SOURCE_ROOT, &test_dir_);
    file_util::AppendToPath(&test_dir_, L"chrome");
    file_util::AppendToPath(&test_dir_, L"third_party");
    file_util::AppendToPath(&test_dir_, L"hunspell");
    file_util::AppendToPath(&test_dir_, L"tests");
  }
  void TearDown() {

  }

  std::wstring test_dir_;
};

// Converts all the lines in the given file to separate strings, and returns
// them in a vector.
std::vector<std::string> ReadFileLines(const std::wstring& file_name) {
  std::string file;
  if (!file_util::ReadFileToString(file_name, &file))
    return std::vector<std::string>();

  std::vector<std::string> list;
  SplitString(file, '\n', &list);

  // Erase any empty lines.
  for (size_t i = 0; i < list.size(); i++) {
    if (list[i].empty()) {
      list.erase(list.begin() + i);
      i--;
    }
  }
  return list;
}

void HunspellTest::RunTest(const char* test_base_name) const {
  std::wstring test_base = test_dir_;
  file_util::AppendToPath(&test_base, UTF8ToWide(test_base_name));

  std::string aff_name = WideToUTF8(test_base) + ".aff";
  std::string dic_name = WideToUTF8(test_base) + ".dic";

#ifdef HUNSPELL_CHROME_CLIENT
  // Read the .aff file for the test.
  convert_dict::AffReader aff_reader(aff_name.c_str());
  ASSERT_TRUE(aff_reader.Read()) <<
      "Unable to read " << test_base_name << ".aff";

  // Read the .dic file for the test.
  convert_dict::DicReader dic_reader(dic_name.c_str());
  ASSERT_TRUE(dic_reader.Read(&aff_reader)) <<
      "Unable to read " << test_base_name << ".dic";

  // Set up the writer.
  hunspell::BDictWriter writer;
  writer.SetComment(aff_reader.comments());
  writer.SetAffixRules(aff_reader.affix_rules());
  writer.SetAffixGroups(aff_reader.GetAffixGroups());
  writer.SetReplacements(aff_reader.replacements());
  writer.SetOtherCommands(aff_reader.other_commands());

  // We add a special word "abracadabra" to each dictionary. This is because our
  // dictionary format can't handle having one word in it (it gets confused when
  // the root node is also a leaf), and some of the test dictionaries have only
  // one word.
  convert_dict::DicReader::WordList word_list = dic_reader.words();
  std::vector<int> affix_ids;
  affix_ids.push_back(0);
  word_list.push_back(std::make_pair("abracadabra", affix_ids));
  writer.SetWords(word_list);

  // Actually generate the bdic data.
  std::string serialized = writer.GetBDict();
  ASSERT_FALSE(serialized.empty());

  // Create a hunspell object to read that data.
  Hunspell hunspell(reinterpret_cast<const unsigned char*>(serialized.data()),
                    serialized.size());
#else
  // Use "regular" Hunspell.
  FILE* aff_file = file_util::OpenFile(aff_name, "r");
  FILE* dic_file = file_util::OpenFile(dic_name, "r");
  EXPECT_TRUE(aff_file && dic_file);
  
  Hunspell hunspell(aff_file, dic_file);
#endif

  // Read the good words file and check it.
  std::vector<std::string> good_words = ReadFileLines(test_base + L".good");
  for (size_t i = 0; i < good_words.size(); i++) {
#ifdef HUNSPELL_CHROME_CLIENT
    std::string cur_word;
    EXPECT_TRUE(aff_reader.EncodingToUTF8(good_words[i], &cur_word));
#else
    std::string cur_word = good_words[i];
#endif
    EXPECT_TRUE(hunspell.spell(cur_word.c_str())) <<
        "On test \"" << test_base_name << "\" the good word \"" <<
        good_words[i] << "\" was reported as spelled incorrectly.";
  }

  // Read the wrong words file and check it.
  std::vector<std::string> wrong_words = ReadFileLines(test_base + L".wrong");
  for (size_t i = 0; i < wrong_words.size(); i++) {
#ifdef HUNSPELL_CHROME_CLIENT
    std::string cur_word;
    EXPECT_TRUE(aff_reader.EncodingToUTF8(wrong_words[i], &cur_word));
#else
    std::string cur_word = wrong_words[i];
#endif
    EXPECT_FALSE(hunspell.spell(cur_word.c_str())) <<
        "On test \"" << test_base_name << "\" the wrong word \"" <<
        cur_word << "\" was reported as spelled correctly.";
  }
}

}  // namespace

TEST_F(HunspellTest, All) {
  RunTest("1592880");
  RunTest("affixes");
  RunTest("alias");
  RunTest("alias2");
  //RunTest("alias3");  // Uses COMPLEXPREFIXES which we don't support.
  RunTest("base");
  RunTest("break");
  //RunTest("checkcompoundcase");  // This one fails for some reason even
                                   // without our modifications.
  RunTest("checkcompoundcase2");
  RunTest("checkcompoundcaseutf");
  RunTest("checkcompounddup");
  RunTest("checkcompoundpattern");
  RunTest("checkcompoundrep");
  RunTest("checkcompoundtriple");
  RunTest("checksharps");
  RunTest("checksharpsutf");
  RunTest("circumfix");
  //RunTest("complexprefixes");
  //RunTest("complexprefixes2");
  //RunTest("complexprefixesutf");
  RunTest("compoundaffix");
  RunTest("compoundaffix2");
  RunTest("compoundaffix3");
  RunTest("compoundflag");
  RunTest("compoundrule");
  RunTest("compoundrule2");
  RunTest("compoundrule3");
  RunTest("compoundrule4");
  RunTest("compoundrule5");
  RunTest("compoundrule6");
  RunTest("conditionalprefix");
  RunTest("flag");
  RunTest("flaglong");
  RunTest("flagnum");
  RunTest("flagutf8");
  RunTest("fogemorpheme");
  RunTest("forbiddenword");
  RunTest("germancompounding");
  RunTest("germancompoundingold");
  RunTest("i35725");
  RunTest("i53643");
  RunTest("i54633");
  RunTest("i54980");
  RunTest("i58202");
  //RunTest("ignore");     // We don't support the "IGNORE" command.
  //RunTest("ignoreutf");
  RunTest("keepcase");
  RunTest("map");
  RunTest("maputf");
  RunTest("needaffix");
  RunTest("needaffix2");
  RunTest("needaffix3");
  RunTest("needaffix4");
  RunTest("needaffix5");
  RunTest("nosuggest");
  RunTest("onlyincompound");
  RunTest("rep");
  RunTest("reputf");
  RunTest("slash");
  RunTest("sug");
  RunTest("utf8");
  RunTest("utf8_bom");
  RunTest("utf8_bom2");
  RunTest("utf8_nonbmp");
  RunTest("utfcompound");
  RunTest("zeroaffix");
}
