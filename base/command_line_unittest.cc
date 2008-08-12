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
                 L"\"in the time of submarines...\"");
#elif defined(OS_POSIX)
  char* argv[] = {"program", "--foo=", "-bAr", 
                  "-Spaetzel=pierogi", "-Baz", "flim",
                  "--other-switches=--dog=canine --cat=feline",
                  "-spaetzle=Crepe", "-=loosevalue", "flan",
                  "--input-translation=45--output-rotation",
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

  EXPECT_EQ(static_cast<size_t>(3), cl.GetLooseValueCount());

  CommandLine::LooseValueIterator iter = cl.GetLooseValuesBegin();
  EXPECT_EQ(L"flim", *iter);
  ++iter;
  EXPECT_EQ(L"flan", *iter);
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
  EXPECT_EQ(static_cast<size_t>(0), cl.GetLooseValueCount());
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
  EXPECT_TRUE(cl.HasSwitch(switch2));
  EXPECT_EQ(value4.substr(1, value4.length() - 2), cl.GetSwitchValue(switch4));
}
#endif
