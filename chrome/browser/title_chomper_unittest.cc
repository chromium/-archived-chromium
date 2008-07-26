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

#include "chrome/browser/title_chomper.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

class TitleChomperTest : public testing::Test {
};

TEST_F(TitleChomperTest, BasicCase) {
  TitleChomper chomper;
  chomper.AddTitle(L"A");
  chomper.AddTitle(L"A B");
  chomper.AddTitle(L"A B C");
  chomper.AddTitle(L"A B C D");

  std::vector<std::wstring> chomped_titles;
  chomper.ChompTitles(&chomped_titles);

  EXPECT_EQ(L"A", chomped_titles.at(0));
  EXPECT_EQ(L"B", chomped_titles.at(1));
  EXPECT_EQ(L"C", chomped_titles.at(2));
  EXPECT_EQ(L"D", chomped_titles.at(3));
}

TEST_F(TitleChomperTest, LongerTitleBasicCase) {
  TitleChomper chomper;
  chomper.AddTitle(L"A Q");
  chomper.AddTitle(L"A B Q");
  chomper.AddTitle(L"A B C Q");
  chomper.AddTitle(L"A B C D Q");

  std::vector<std::wstring> chomped_titles;
  chomper.ChompTitles(&chomped_titles);

  EXPECT_EQ(L"A Q", chomped_titles.at(0));
  EXPECT_EQ(L"B Q", chomped_titles.at(1));
  EXPECT_EQ(L"C Q", chomped_titles.at(2));
  EXPECT_EQ(L"D Q", chomped_titles.at(3));
}

TEST_F(TitleChomperTest, PunctuationCase) {
  TitleChomper chomper;
  // There should be able to be wacky punctuation and it should still work
  chomper.AddTitle(L"A, Q.");
  chomper.AddTitle(L"A, B:- Q.");
  chomper.AddTitle(L"A, B:- C; Q");
  chomper.AddTitle(L"A B. C D; Q");

  std::vector<std::wstring> chomped_titles;
  chomper.ChompTitles(&chomped_titles);

  EXPECT_EQ(L"A, Q.", chomped_titles.at(0));
  EXPECT_EQ(L"B:- Q.", chomped_titles.at(1));
  EXPECT_EQ(L"C; Q", chomped_titles.at(2));
  EXPECT_EQ(L"D; Q", chomped_titles.at(3));
}

TEST_F(TitleChomperTest, IdentiticalTitleCase) {
  TitleChomper chomper;
  chomper.AddTitle(L"A Q");
  chomper.AddTitle(L"A Q");
  chomper.AddTitle(L"A B C Q");
  chomper.AddTitle(L"A B C Q");
  chomper.AddTitle(L"A B C Q");

  std::vector<std::wstring> chomped_titles;
  chomper.ChompTitles(&chomped_titles);

  EXPECT_EQ(L"A Q", chomped_titles.at(0));
  EXPECT_EQ(L"A Q", chomped_titles.at(1));
  EXPECT_EQ(L"A B C Q", chomped_titles.at(2));
  EXPECT_EQ(L"A B C Q", chomped_titles.at(3));
  EXPECT_EQ(L"A B C Q", chomped_titles.at(4));
}

TEST_F(TitleChomperTest, CraigslistCase) {
  TitleChomper chomper;
  chomper.AddTitle(L"craigslist: san francisco bay area classifieds for jobs, apartments, personals, for sale, services, community, and events");
  chomper.AddTitle(L"craigslist | cars & trucks");
  chomper.AddTitle(L"s.f. bayarea craigslist >  > cars & trucks: search");
  chomper.AddTitle(L"s.f. bayarea craigslist > peninsula > cars & trucks: search");
  chomper.AddTitle(L"s.f. bayarea craigslist > peninsula > cars & trucks: search for \"tacoma\"");

  std::vector<std::wstring> chomped_titles;
  chomper.ChompTitles(&chomped_titles);

  EXPECT_EQ(L"craigslist: san francisco bay area classifieds for jobs, apartments, personals, for sale, services, community, and events", chomped_titles.at(0));
  EXPECT_EQ(L"cars & trucks", chomped_titles.at(1));
  EXPECT_EQ(L"s.f. bayarea craigslist >  > cars & trucks: search", chomped_titles.at(2));
  EXPECT_EQ(L"peninsula > cars & trucks: search", chomped_titles.at(3));
  EXPECT_EQ(L"for \"tacoma\"", chomped_titles.at(4));
}