// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const FilePath::CharType kTempCustomDictionaryFile[] =
    FILE_PATH_LITERAL("temp_custom_dictionary.txt");
}  // namespace

class SpellCheckTest : public testing::Test {
 private:
  MessageLoop message_loop_;
};

// Represents a special initialization function used only for the unit tests
// in this file.
extern void InitHunspellWithFiles(FILE* file_aff_hunspell,
                                  FILE* file_dic_hunspell);

// Operates unit tests for the webkit_glue::SpellCheckWord() function
// with the US English dictionary.
// The unit tests in this function consist of:
//   * Tests for the function with empty strings;
//   * Tests for the function with a valid English word;
//   * Tests for the function with a valid non-English word;
//   * Tests for the function with a valid English word with a preceding
//     space character;
//   * Tests for the function with a valid English word with a preceding
//     non-English word;
//   * Tests for the function with a valid English word with a following
//     space character;
//   * Tests for the function with a valid English word with a following
//     non-English word;
//   * Tests for the function with two valid English words concatenated
//     with space characters or non-English words;
//   * Tests for the function with an invalid English word;
//   * Tests for the function with an invalid English word with a preceding
//     space character;
//   * Tests for the function with an invalid English word with a preceding
//     non-English word;
//   * Tests for the function with2 an invalid English word with a following
//     space character;
//   * Tests for the function with an invalid English word with a following
//     non-English word, and;
//   * Tests for the function with two invalid English words concatenated
//     with space characters or non-English words.
// A test with a "[ROBUSTNESS]" mark shows it is a robustness test and it uses
// grammartically incorrect string.
// TODO(hbono): Please feel free to add more tests.
TEST_F(SpellCheckTest, SpellCheckStrings_EN_US) {
  static const struct {
    // A string to be tested.
    const wchar_t* input;
    // An expected result for this test case.
    //   * true: the input string does not have any invalid words.
    //   * false: the input string has one or more invalid words.
    bool expected_result;
    // The position and the length of the first invalid word.
    int misspelling_start;
    int misspelling_length;
  } kTestCases[] = {
    // Empty strings.
    {NULL, true, 0, 0},
    {L"", true, 0, 0},
    {L" ", true, 0, 0},
    {L"\xA0", true, 0, 0},
    {L"\x3000", true, 0, 0},

    // A valid English word "hello".
    {L"hello", true, 0, 0},
    // A valid Chinese word (meaning "hello") consisiting of two CJKV
    // ideographs
    {L"\x4F60\x597D", true, 0, 0},
    // A valid Korean word (meaning "hello") consisting of five hangul
    // syllables
    {L"\xC548\xB155\xD558\xC138\xC694", true, 0, 0},
    // A valid Japanese word (meaning "hello") consisting of five Hiragana
    // letters
    {L"\x3053\x3093\x306B\x3061\x306F", true, 0, 0},
    // A valid Hindi word (meaning ?) consisting of six Devanagari letters
    // (This word is copied from "http://b/issue?id=857583".)
    {L"\x0930\x093E\x091C\x0927\x093E\x0928", true, 0, 0},
    // A valid English word "affix" using a Latin ligature 'ffi'
    {L"a\xFB03x", true, 0, 0},
    // A valid English word "hello" (fullwidth version)
    {L"\xFF28\xFF45\xFF4C\xFF4C\xFF4F", true, 0, 0},
    // Two valid Greek words (meaning "hello") consisting of seven Greek
    // letters
    {L"\x03B3\x03B5\x03B9\x03AC" L" " L"\x03C3\x03BF\x03C5", true, 0, 0},
    // A valid Russian word (meainng "hello") consisting of twelve Cyrillic
    // letters
    {L"\x0437\x0434\x0440\x0430\x0432\x0441"
     L"\x0442\x0432\x0443\x0439\x0442\x0435", true, 0, 0},
    // A valid English contraction
    {L"isn't", true, 0, 0},
    // A valid English word enclosed with underscores.
    {L"_hello_", true, 0, 0},

    // A valid English word with a preceding whitespace
    {L" " L"hello", true, 0, 0},
    // A valid English word with a preceding no-break space
    {L"\xA0" L"hello", true, 0, 0},
    // A valid English word with a preceding ideographic space
    {L"\x3000" L"hello", true, 0, 0},
    // A valid English word with a preceding Chinese word
    {L"\x4F60\x597D" L"hello", true, 0, 0},
    // [ROBUSTNESS] A valid English word with a preceding Korean word
    {L"\xC548\xB155\xD558\xC138\xC694" L"hello", true, 0, 0},
    // A valid English word with a preceding Japanese word
    {L"\x3053\x3093\x306B\x3061\x306F" L"hello", true, 0, 0},
    // [ROBUSTNESS] A valid English word with a preceding Hindi word
    {L"\x0930\x093E\x091C\x0927\x093E\x0928" L"hello", true, 0, 0},
    // [ROBUSTNESS] A valid English word with two preceding Greek words
    {L"\x03B3\x03B5\x03B9\x03AC" L" " L"\x03C3\x03BF\x03C5"
     L"hello", true, 0, 0},
    // [ROBUSTNESS] A valid English word with a preceding Russian word
    {L"\x0437\x0434\x0440\x0430\x0432\x0441"
     L"\x0442\x0432\x0443\x0439\x0442\x0435" L"hello", true, 0, 0},

    // A valid English word with a following whitespace
    {L"hello" L" ", true, 0, 0},
    // A valid English word with a following no-break space
    {L"hello" L"\xA0", true, 0, 0},
    // A valid English word with a following ideographic space
    {L"hello" L"\x3000", true, 0, 0},
    // A valid English word with a following Chinese word
    {L"hello" L"\x4F60\x597D", true, 0, 0},
    // [ROBUSTNESS] A valid English word with a following Korean word
    {L"hello" L"\xC548\xB155\xD558\xC138\xC694", true, 0, 0},
    // A valid English word with a following Japanese word
    {L"hello" L"\x3053\x3093\x306B\x3061\x306F", true, 0, 0},
    // [ROBUSTNESS] A valid English word with a following Hindi word
    {L"hello" L"\x0930\x093E\x091C\x0927\x093E\x0928", true, 0, 0},
    // [ROBUSTNESS] A valid English word with two following Greek words
    {L"hello"
     L"\x03B3\x03B5\x03B9\x03AC" L" " L"\x03C3\x03BF\x03C5", true, 0, 0},
    // [ROBUSTNESS] A valid English word with a following Russian word
    {L"hello" L"\x0437\x0434\x0440\x0430\x0432\x0441"
     L"\x0442\x0432\x0443\x0439\x0442\x0435", true, 0, 0},

    // Two valid English words concatenated with a whitespace
    {L"hello" L" " L"hello", true, 0, 0},
    // Two valid English words concatenated with a no-break space
    {L"hello" L"\xA0" L"hello", true, 0, 0},
    // Two valid English words concatenated with an ideographic space
    {L"hello" L"\x3000" L"hello", true, 0, 0},
    // Two valid English words concatenated with a Chinese word
    {L"hello" L"\x4F60\x597D" L"hello", true, 0, 0},
    // [ROBUSTNESS] Two valid English words concatenated with a Korean word
    {L"hello" L"\xC548\xB155\xD558\xC138\xC694" L"hello", true, 0, 0},
    // Two valid English words concatenated with a Japanese word
    {L"hello" L"\x3053\x3093\x306B\x3061\x306F" L"hello", true, 0, 0},
    // [ROBUSTNESS] Two valid English words concatenated with a Hindi word
    {L"hello" L"\x0930\x093E\x091C\x0927\x093E\x0928" L"hello" , true, 0, 0},
    // [ROBUSTNESS] Two valid English words concatenated with two Greek words
    {L"hello" L"\x03B3\x03B5\x03B9\x03AC" L" " L"\x03C3\x03BF\x03C5"
     L"hello", true, 0, 0},
    // [ROBUSTNESS] Two valid English words concatenated with a Russian word
    {L"hello" L"\x0437\x0434\x0440\x0430\x0432\x0441"
     L"\x0442\x0432\x0443\x0439\x0442\x0435" L"hello", true, 0, 0},
    // [ROBUSTNESS] Two valid English words concatenated with a contraction
    // character.
    {L"hello:hello", true, 0, 0},

    // An invalid English word
    {L"ifmmp", false, 0, 5},
    // An invalid English word "bffly" containing a Latin ligature 'ffl'
    {L"b\xFB04y", false, 0, 3},
    // An invalid English word "ifmmp" (fullwidth version)
    {L"\xFF29\xFF46\xFF4D\xFF4D\xFF50", false, 0, 5},
    // An invalid English contraction
    {L"jtm'u", false, 0, 5},
    // An invalid English word enclosed with underscores.
    {L"_ifmmp_", false, 1, 5},

    // An invalid English word with a preceding whitespace
    {L" " L"ifmmp", false, 1, 5},
    // An invalid English word with a preceding no-break space
    {L"\xA0" L"ifmmp", false, 1, 5},
    // An invalid English word with a preceding ideographic space
    {L"\x3000" L"ifmmp", false, 1, 5},
    // An invalid English word with a preceding Chinese word
    {L"\x4F60\x597D" L"ifmmp", false, 2, 5},
    // [ROBUSTNESS] An invalid English word with a preceding Korean word
    {L"\xC548\xB155\xD558\xC138\xC694" L"ifmmp", false, 5, 5},
    // An invalid English word with a preceding Japanese word
    {L"\x3053\x3093\x306B\x3061\x306F" L"ifmmp", false, 5, 5},
    // [ROBUSTNESS] An invalid English word with a preceding Hindi word
    {L"\x0930\x093E\x091C\x0927\x093E\x0928" L"ifmmp", false, 6, 5},
    // [ROBUSTNESS] An invalid English word with two preceding Greek words
    {L"\x03B3\x03B5\x03B9\x03AC" L" " L"\x03C3\x03BF\x03C5"
     L"ifmmp", false, 8, 5},
    // [ROBUSTNESS] An invalid English word with a preceding Russian word
    {L"\x0437\x0434\x0440\x0430\x0432\x0441"
     L"\x0442\x0432\x0443\x0439\x0442\x0435" L"ifmmp", false, 12, 5},

    // An invalid English word with a following whitespace
    {L"ifmmp" L" ", false, 0, 5},
    // An invalid English word with a following no-break space
    {L"ifmmp" L"\xA0", false, 0, 5},
    // An invalid English word with a following ideographic space
    {L"ifmmp" L"\x3000", false, 0, 5},
    // An invalid English word with a following Chinese word
    {L"ifmmp" L"\x4F60\x597D", false, 0, 5},
    // [ROBUSTNESS] An invalid English word with a following Korean word
    {L"ifmmp" L"\xC548\xB155\xD558\xC138\xC694", false, 0, 5},
    // An invalid English word with a following Japanese word
    {L"ifmmp" L"\x3053\x3093\x306B\x3061\x306F", false, 0, 5},
    // [ROBUSTNESS] An invalid English word with a following Hindi word
    {L"ifmmp" L"\x0930\x093E\x091C\x0927\x093E\x0928", false, 0, 5},
    // [ROBUSTNESS] An invalid English word with two following Greek words
    {L"ifmmp"
     L"\x03B3\x03B5\x03B9\x03AC" L" " L"\x03C3\x03BF\x03C5", false, 0, 5},
    // [ROBUSTNESS] An invalid English word with a following Russian word
    {L"ifmmp" L"\x0437\x0434\x0440\x0430\x0432\x0441"
     L"\x0442\x0432\x0443\x0439\x0442\x0435", false, 0, 5},

    // Two invalid English words concatenated with a whitespace
    {L"ifmmp" L" " L"ifmmp", false, 0, 5},
    // Two invalid English words concatenated with a no-break space
    {L"ifmmp" L"\xA0" L"ifmmp", false, 0, 5},
    // Two invalid English words concatenated with an ideographic space
    {L"ifmmp" L"\x3000" L"ifmmp", false, 0, 5},
    // Two invalid English words concatenated with a Chinese word
    {L"ifmmp" L"\x4F60\x597D" L"ifmmp", false, 0, 5},
    // [ROBUSTNESS] Two invalid English words concatenated with a Korean word
    {L"ifmmp" L"\xC548\xB155\xD558\xC138\xC694" L"ifmmp", false, 0, 5},
    // Two invalid English words concatenated with a Japanese word
    {L"ifmmp" L"\x3053\x3093\x306B\x3061\x306F" L"ifmmp", false, 0, 5},
    // [ROBUSTNESS] Two invalid English words concatenated with a Hindi word
    {L"ifmmp" L"\x0930\x093E\x091C\x0927\x093E\x0928" L"ifmmp" , false, 0, 5},
    // [ROBUSTNESS] Two invalid English words concatenated with two Greek words
    {L"ifmmp" L"\x03B3\x03B5\x03B9\x03AC" L" " L"\x03C3\x03BF\x03C5"
     L"ifmmp", false, 0, 5},
    // [ROBUSTNESS] Two invalid English words concatenated with a Russian word
    {L"ifmmp" L"\x0437\x0434\x0440\x0430\x0432\x0441"
     L"\x0442\x0432\x0443\x0439\x0442\x0435" L"ifmmp", false, 0, 5},
    // [ROBUSTNESS] Two invalid English words concatenated with a contraction
    // character.
    {L"ifmmp:ifmmp", false, 0, 11},
  };

  FilePath hunspell_directory;
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP_DICTIONARIES,
                               &hunspell_directory));

  scoped_refptr<SpellChecker> spell_checker(new SpellChecker(
      hunspell_directory, L"en-US", NULL, FilePath()));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    size_t input_length = 0;
    if (kTestCases[i].input != NULL) {
      input_length = wcslen(kTestCases[i].input);
    }
    int misspelling_start;
    int misspelling_length;
    bool result = spell_checker->SpellCheckWord(kTestCases[i].input,
                                                static_cast<int>(input_length),
                                                &misspelling_start,
                                                &misspelling_length, NULL);

    EXPECT_EQ(kTestCases[i].expected_result, result);
    EXPECT_EQ(kTestCases[i].misspelling_start, misspelling_start);
    EXPECT_EQ(kTestCases[i].misspelling_length, misspelling_length);
  }
}


TEST_F(SpellCheckTest, SpellCheckSuggestions_EN_US) {
  static const struct {
    // A string to be tested.
    const wchar_t* input;
    // An expected result for this test case.
    //   * true: the input string does not have any invalid words.
    //   * false: the input string has one or more invalid words.
    bool expected_result;
    // The position and the length of the first invalid word.
    int misspelling_start;
    int misspelling_length;

    // A suggested word that should occur.
    const wchar_t* suggested_word;
  } kTestCases[] = {    // A valid English word with a preceding whitespace
    {L"ello", false, 0, 0, L"hello"},
    {L"ello", false, 0, 0, L"cello"},
    {L"wate", false, 0, 0, L"water"},
    {L"wate", false, 0, 0, L"waste"},
    {L"wate", false, 0, 0, L"sate"},
    {L"wate", false, 0, 0, L"rate"},
    {L"jum", false, 0, 0, L"jump"},
    {L"jum", false, 0, 0, L"rum"},
    {L"jum", false, 0, 0, L"sum"},
    {L"jum", false, 0, 0, L"tum"},
    // TODO (Sidchat): add many more examples.
  };

  FilePath hunspell_directory;
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP_DICTIONARIES,
                               &hunspell_directory));

  scoped_refptr<SpellChecker> spell_checker(new SpellChecker(
      hunspell_directory, L"en-US", NULL, FilePath()));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    std::vector<std::wstring> suggestions;
    size_t input_length = 0;
    if (kTestCases[i].input != NULL) {
      input_length = wcslen(kTestCases[i].input);
    }
    int misspelling_start;
    int misspelling_length;
    bool result = spell_checker->SpellCheckWord(kTestCases[i].input,
                                                static_cast<int>(input_length),
                                                &misspelling_start,
                                                &misspelling_length,
                                                &suggestions);

    // Check for spelling.
    EXPECT_EQ(kTestCases[i].expected_result, result);

    // Check if the suggested words occur.
    bool suggested_word_is_present = false;
    for (int j=0; j < static_cast<int>(suggestions.size()); j++) {
      if (suggestions.at(j).compare(kTestCases[i].suggested_word) == 0) {
        suggested_word_is_present = true;
        break;
      }
    }

    EXPECT_TRUE(suggested_word_is_present);
  }
}

// This test Adds words to the SpellChecker and veifies that it remembers them.
TEST_F(SpellCheckTest, DISABLED_SpellCheckAddToDictionary_EN_US) {
  static const struct {
    // A string to be added to SpellChecker.
    const wchar_t* word_to_add;
  } kTestCases[] = {  // word to be added to SpellChecker
    {L"Googley"},
    {L"Googleplex"},
    {L"Googler"},
  };

  FilePath hunspell_directory;
  FilePath custom_dictionary_file(kTempCustomDictionaryFile);
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP_DICTIONARIES,
                               &hunspell_directory));

  scoped_refptr<SpellChecker> spell_checker(new SpellChecker(
      hunspell_directory, L"en-US", NULL, custom_dictionary_file));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    // Add the word to spellchecker.
    spell_checker->AddWord(std::wstring(kTestCases[i].word_to_add));

    // Now check whether it is added to Spellchecker.
    std::vector<std::wstring> suggestions;
    size_t input_length = 0;
    if (kTestCases[i].word_to_add != NULL) {
      input_length = wcslen(kTestCases[i].word_to_add);
    }
    int misspelling_start;
    int misspelling_length;
    bool result = spell_checker->SpellCheckWord(kTestCases[i].word_to_add,
                                                static_cast<int>(input_length),
                                                &misspelling_start,
                                                &misspelling_length,
                                                &suggestions);

    // Check for spelling.
    EXPECT_TRUE(result);
  }

  // Now initialize another spellchecker to see that AddToWord is permanent.
  scoped_refptr<SpellChecker> spell_checker_new(new SpellChecker(
      hunspell_directory, L"en-US", NULL, custom_dictionary_file));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    // Now check whether it is added to Spellchecker.
    std::vector<std::wstring> suggestions;
    size_t input_length = 0;
    if (kTestCases[i].word_to_add != NULL) {
      input_length = wcslen(kTestCases[i].word_to_add);
    }
    int misspelling_start;
    int misspelling_length;
    bool result = spell_checker_new->SpellCheckWord(
        kTestCases[i].word_to_add,
        static_cast<int>(input_length),
        &misspelling_start,
        &misspelling_length,
        &suggestions);

    // Check for spelling.
    EXPECT_TRUE(result);
  }

  // Remove the temp custom dictionary file.
  file_util::Delete(custom_dictionary_file, false);
}

// SpellChecker should suggest custome words for misspelled words.
TEST_F(SpellCheckTest, DISABLED_SpellCheckSuggestionsAddToDictionary_EN_US) {
  static const struct {
    // A string to be added to SpellChecker.
    const wchar_t* word_to_add;
  } kTestCases[] = {  // word to be added to SpellChecker
    {L"Googley"},
    {L"Googleplex"},
    {L"Googler"},
  };

  FilePath hunspell_directory;
  FilePath custom_dictionary_file(kTempCustomDictionaryFile);
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP_DICTIONARIES,
                               &hunspell_directory));

  scoped_refptr<SpellChecker> spell_checker(new SpellChecker(
      hunspell_directory, L"en-US", NULL, custom_dictionary_file));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    // Add the word to spellchecker.
    spell_checker->AddWord(std::wstring(kTestCases[i].word_to_add));
  }

  // Now check to see whether the custom words are suggested for
  // misspelled but similar words.
  static const struct {
    // A string to be tested.
    const wchar_t* input;
    // An expected result for this test case.
    //   * true: the input string does not have any invalid words.
    //   * false: the input string has one or more invalid words.
    bool expected_result;
    // The position and the length of the first invalid word.
    int misspelling_start;
    int misspelling_length;

    // A suggested word that should occur.
    const wchar_t* suggested_word;
  } kTestCasesToBeTested[] = {
    {L"oogley", false, 0, 0, L"Googley"},
    {L"oogler", false, 0, 0, L"Googler"},
    {L"oogleplex", false, 0, 0, L"Googleplex"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCasesToBeTested); ++i) {
    std::vector<std::wstring> suggestions;
    size_t input_length = 0;
    if (kTestCasesToBeTested[i].input != NULL) {
      input_length = wcslen(kTestCasesToBeTested[i].input);
    }
    int misspelling_start;
    int misspelling_length;
    bool result = spell_checker->SpellCheckWord(kTestCasesToBeTested[i].input,
                                                static_cast<int>(input_length),
                                                &misspelling_start,
                                                &misspelling_length,
                                                &suggestions);

    // Check for spelling.
    EXPECT_EQ(result, kTestCasesToBeTested[i].expected_result);

    // Check if the suggested words occur.
    bool suggested_word_is_present = false;
    for (int j=0; j < static_cast<int>(suggestions.size()); j++) {
      if (suggestions.at(j).compare(kTestCasesToBeTested[i].suggested_word) ==
                                    0) {
        suggested_word_is_present = true;
        break;
      }
    }

    EXPECT_TRUE(suggested_word_is_present);
  }

  // Remove the temp custom dictionary file.
  file_util::Delete(custom_dictionary_file, false);
}
