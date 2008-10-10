// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/basictypes.h"
#include "base/logging.h"
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
#endif
  EXPECT_TRUE(cl.command_line_string().empty());
  EXPECT_TRUE(cl.program().empty());
  EXPECT_EQ(0U, cl.GetLooseValueCount());
}

// Test static functions for appending switches to a command line.
// TODO(pinkerton): non-windows platforms don't have the requisite ctor here, so
// we need something that tests AppendSwitches in another way (if even desired).
#if defined(OS_WIN)
TEST(CommandLineTest, AppendSwitches) {
  std::wstring cl_string = L"Program";
  std::wstring switch1 = L"switch1";
  std::wstring switch2 = L"switch2";
  std::wstring value = L"value";
  std::wstring switch3 = L"switch3";
  std::wstring value3 = L"a value with spaces";
  std::wstring switch4 = L"switch4";
  std::wstring value4 = L"\"a value with quotes\"";

  CommandLine::AppendSwitch(&cl_string, switch1);
  CommandLine::AppendSwitchWithValue(&cl_string, switch2, value);
  CommandLine::AppendSwitchWithValue(&cl_string, switch3, value3);
  CommandLine::AppendSwitchWithValue(&cl_string, switch4, value4);
  CommandLine cl(cl_string);

  EXPECT_TRUE(cl.HasSwitch(switch1));
  EXPECT_TRUE(cl.HasSwitch(switch2));
  EXPECT_EQ(value, cl.GetSwitchValue(switch2));
  EXPECT_TRUE(cl.HasSwitch(switch3));
  EXPECT_EQ(value3, cl.GetSwitchValue(switch3));
  EXPECT_TRUE(cl.HasSwitch(switch4));
  EXPECT_EQ(value4.substr(1, value4.length() - 2), cl.GetSwitchValue(switch4));
}
#endif
