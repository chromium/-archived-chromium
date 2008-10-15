// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class CommandLineTest : public testing::Test {
  };
};

TEST(CommandLineTest, CommandLineConstructor) {
#ifdef OS_WIN
  CommandLine cl(L"program --foo= -bAr  /Spaetzel=pierogi /Baz flim "
                 L"--other-switches=\"--dog=canine --cat=feline\" "
                 L"-spaetzle=Crepe   -=loosevalue  flan "
                 L"--input-translation=\"45\"--output-rotation "
                 L"-- -- --not-a-switch "
                 L"\"in the time of submarines...\"");
#elif defined(OS_POSIX)
  const char* argv[] = {"program", "--foo=", "-bAr", 
                        "-Spaetzel=pierogi", "-Baz", "flim",
                        "--other-switches=--dog=canine --cat=feline",
                        "-spaetzle=Crepe", "-=loosevalue", "flan",
                        "--input-translation=45--output-rotation",
                        "--", "--", "--not-a-switch",
                        "in the time of submarines..."};
  CommandLine cl(arraysize(argv), argv);
#endif
  EXPECT_FALSE(cl.command_line_string().empty());
  EXPECT_FALSE(cl.HasSwitch(L"cruller"));
  EXPECT_FALSE(cl.HasSwitch(L"flim"));
  EXPECT_FALSE(cl.HasSwitch(L"program"));
  EXPECT_FALSE(cl.HasSwitch(L"dog"));
  EXPECT_FALSE(cl.HasSwitch(L"cat"));
  EXPECT_FALSE(cl.HasSwitch(L"output-rotation"));
  EXPECT_FALSE(cl.HasSwitch(L"not-a-switch"));
  EXPECT_FALSE(cl.HasSwitch(L"--"));

  EXPECT_EQ(L"program", cl.program());

  EXPECT_TRUE(cl.HasSwitch(L"foo"));
  EXPECT_TRUE(cl.HasSwitch(L"bar"));
  EXPECT_TRUE(cl.HasSwitch(L"baz"));
  EXPECT_TRUE(cl.HasSwitch(L"spaetzle"));
  EXPECT_TRUE(cl.HasSwitch(L"SPAETZLE"));
  EXPECT_TRUE(cl.HasSwitch(L"other-switches"));
  EXPECT_TRUE(cl.HasSwitch(L"input-translation"));

  EXPECT_EQ(L"Crepe", cl.GetSwitchValue(L"spaetzle"));
  EXPECT_EQ(L"", cl.GetSwitchValue(L"Foo"));
  EXPECT_EQ(L"", cl.GetSwitchValue(L"bar"));
  EXPECT_EQ(L"", cl.GetSwitchValue(L"cruller"));
  EXPECT_EQ(L"--dog=canine --cat=feline", cl.GetSwitchValue(L"other-switches"));
  EXPECT_EQ(L"45--output-rotation", cl.GetSwitchValue(L"input-translation"));

  EXPECT_EQ(5U, cl.GetLooseValueCount());

  CommandLine::LooseValueIterator iter = cl.GetLooseValuesBegin();
  EXPECT_EQ(L"flim", *iter);
  ++iter;
  EXPECT_EQ(L"flan", *iter);
  ++iter;
  EXPECT_EQ(L"--", *iter);
  ++iter;
  EXPECT_EQ(L"--not-a-switch", *iter);
  ++iter;
  EXPECT_EQ(L"in the time of submarines...", *iter);
  ++iter;
  EXPECT_TRUE(iter == cl.GetLooseValuesEnd());
#if defined(OS_POSIX)
  std::vector<std::string> argvec = cl.argv();

  for (size_t i = 0; i < argvec.size(); i++) {
    EXPECT_EQ(0, argvec[i].compare(argv[i]));
  }
#endif
}

// These test the command line used to invoke the unit test.
TEST(CommandLineTest, DefaultConstructor) {
  CommandLine cl;
  EXPECT_FALSE(cl.command_line_string().empty());
  EXPECT_FALSE(cl.program().empty());
}

// Tests behavior with an empty input string.
TEST(CommandLineTest, EmptyString) {
#if defined(OS_WIN)
  CommandLine cl(L"");
#elif defined(OS_POSIX)
  CommandLine cl(0, NULL);
  EXPECT_TRUE(cl.argv().size() == 0);
#endif
  EXPECT_TRUE(cl.command_line_string().empty());
  EXPECT_TRUE(cl.program().empty());
  EXPECT_EQ(0U, cl.GetLooseValueCount());
}

// Test static functions for appending switches to a command line.
TEST(CommandLineTest, AppendSwitches) {
  std::wstring switch1 = L"switch1";
  std::wstring switch2 = L"switch2";
  std::wstring value = L"value";
  std::wstring switch3 = L"switch3";
  std::wstring value3 = L"a value with spaces";
  std::wstring switch4 = L"switch4";
  std::wstring value4 = L"\"a value with quotes\"";

#if defined(OS_WIN)
  std::wstring cl_string = L"Program";
  CommandLine::AppendSwitch(&cl_string, switch1);
  CommandLine::AppendSwitchWithValue(&cl_string, switch2, value);
  CommandLine::AppendSwitchWithValue(&cl_string, switch3, value3);
  CommandLine::AppendSwitchWithValue(&cl_string, switch4, value4);
  CommandLine cl(cl_string);
#elif defined(OS_POSIX)
  std::vector<std::string> argv;
  argv.push_back(std::string("Program"));
  argv.push_back(WideToUTF8(CommandLine::PrefixedSwitchString(switch1)));
  argv.push_back(WideToUTF8(CommandLine::PrefixedSwitchStringWithValue(
      switch2, value)));
  argv.push_back(WideToUTF8(CommandLine::PrefixedSwitchStringWithValue(
      switch3, value3)));
  argv.push_back(WideToUTF8(CommandLine::PrefixedSwitchStringWithValue(
      switch4, value4.substr(1, value4.length() - 2))));
  CommandLine cl(argv);
#endif

  EXPECT_TRUE(cl.HasSwitch(switch1));
  EXPECT_TRUE(cl.HasSwitch(switch2));
  EXPECT_EQ(value, cl.GetSwitchValue(switch2));
  EXPECT_TRUE(cl.HasSwitch(switch3));
  EXPECT_EQ(value3, cl.GetSwitchValue(switch3));
  EXPECT_TRUE(cl.HasSwitch(switch4));
  EXPECT_EQ(value4.substr(1, value4.length() - 2), cl.GetSwitchValue(switch4));
}

