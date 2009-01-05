// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// This macro helps avoid wrapped lines in the test structs.
#define FPL(x) FILE_PATH_LITERAL(x)

struct UnaryTestData {
  const FilePath::CharType* input;
  const FilePath::CharType* expected;
};

struct UnaryBooleanTestData {
  const FilePath::CharType* input;
  bool expected;
};

struct BinaryTestData {
  const FilePath::CharType* inputs[2];
  const FilePath::CharType* expected;
};

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest
class FilePathTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();
  }
  virtual void TearDown() {
    PlatformTest::TearDown();
  }
};

TEST_F(FilePathTest, DirName) {
  const struct UnaryTestData cases[] = {
    { FPL(""),              FPL(".") },
    { FPL("aa"),            FPL(".") },
    { FPL("/aa/bb"),        FPL("/aa") },
    { FPL("/aa/bb/"),       FPL("/aa") },
    { FPL("/aa/bb//"),      FPL("/aa") },
    { FPL("/aa/bb/ccc"),    FPL("/aa/bb") },
    { FPL("/aa"),           FPL("/") },
    { FPL("/aa/"),          FPL("/") },
    { FPL("/"),             FPL("/") },
    { FPL("//"),            FPL("//") },
    { FPL("///"),           FPL("/") },
    { FPL("aa/"),           FPL(".") },
    { FPL("aa/bb"),         FPL("aa") },
    { FPL("aa/bb/"),        FPL("aa") },
    { FPL("aa/bb//"),       FPL("aa") },
    { FPL("aa//bb//"),      FPL("aa") },
    { FPL("aa//bb/"),       FPL("aa") },
    { FPL("aa//bb"),        FPL("aa") },
    { FPL("//aa/bb"),       FPL("//aa") },
    { FPL("//aa/"),         FPL("//") },
    { FPL("//aa"),          FPL("//") },
    { FPL("0:"),            FPL(".") },
    { FPL("@:"),            FPL(".") },
    { FPL("[:"),            FPL(".") },
    { FPL("`:"),            FPL(".") },
    { FPL("{:"),            FPL(".") },
    { FPL("\xB3:"),         FPL(".") },
    { FPL("\xC5:"),         FPL(".") },
#if defined(OS_WIN)
    { FPL("\x0143:"),       FPL(".") },
#endif  // OS_WIN
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("c:"),            FPL("c:") },
    { FPL("C:"),            FPL("C:") },
    { FPL("A:"),            FPL("A:") },
    { FPL("Z:"),            FPL("Z:") },
    { FPL("a:"),            FPL("a:") },
    { FPL("z:"),            FPL("z:") },
    { FPL("c:aa"),          FPL("c:") },
    { FPL("c:/"),           FPL("c:/") },
    { FPL("c://"),          FPL("c://") },
    { FPL("c:///"),         FPL("c:/") },
    { FPL("c:/aa"),         FPL("c:/") },
    { FPL("c:/aa/"),        FPL("c:/") },
    { FPL("c:/aa/bb"),      FPL("c:/aa") },
    { FPL("c:aa/bb"),       FPL("c:aa") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { FPL("\\aa\\bb"),      FPL("\\aa") },
    { FPL("\\aa\\bb\\"),    FPL("\\aa") },
    { FPL("\\aa\\bb\\\\"),  FPL("\\aa") },
    { FPL("\\aa\\bb\\ccc"), FPL("\\aa\\bb") },
    { FPL("\\aa"),          FPL("\\") },
    { FPL("\\aa\\"),        FPL("\\") },
    { FPL("\\"),            FPL("\\") },
    { FPL("\\\\"),          FPL("\\\\") },
    { FPL("\\\\\\"),        FPL("\\") },
    { FPL("aa\\"),          FPL(".") },
    { FPL("aa\\bb"),        FPL("aa") },
    { FPL("aa\\bb\\"),      FPL("aa") },
    { FPL("aa\\bb\\\\"),    FPL("aa") },
    { FPL("aa\\\\bb\\\\"),  FPL("aa") },
    { FPL("aa\\\\bb\\"),    FPL("aa") },
    { FPL("aa\\\\bb"),      FPL("aa") },
    { FPL("\\\\aa\\bb"),    FPL("\\\\aa") },
    { FPL("\\\\aa\\"),      FPL("\\\\") },
    { FPL("\\\\aa"),        FPL("\\\\") },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("c:\\"),          FPL("c:\\") },
    { FPL("c:\\\\"),        FPL("c:\\\\") },
    { FPL("c:\\\\\\"),      FPL("c:\\") },
    { FPL("c:\\aa"),        FPL("c:\\") },
    { FPL("c:\\aa\\"),      FPL("c:\\") },
    { FPL("c:\\aa\\bb"),    FPL("c:\\aa") },
    { FPL("c:aa\\bb"),      FPL("c:aa") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#endif  // FILE_PATH_USES_WIN_SEPARATORS
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    FilePath observed = input.DirName();
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed.value()) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, BaseName) {
  const struct UnaryTestData cases[] = {
    { FPL(""),              FPL("") },
    { FPL("aa"),            FPL("aa") },
    { FPL("/aa/bb"),        FPL("bb") },
    { FPL("/aa/bb/"),       FPL("bb") },
    { FPL("/aa/bb//"),      FPL("bb") },
    { FPL("/aa/bb/ccc"),    FPL("ccc") },
    { FPL("/aa"),           FPL("aa") },
    { FPL("/"),             FPL("/") },
    { FPL("//"),            FPL("//") },
    { FPL("///"),           FPL("/") },
    { FPL("aa/"),           FPL("aa") },
    { FPL("aa/bb"),         FPL("bb") },
    { FPL("aa/bb/"),        FPL("bb") },
    { FPL("aa/bb//"),       FPL("bb") },
    { FPL("aa//bb//"),      FPL("bb") },
    { FPL("aa//bb/"),       FPL("bb") },
    { FPL("aa//bb"),        FPL("bb") },
    { FPL("//aa/bb"),       FPL("bb") },
    { FPL("//aa/"),         FPL("aa") },
    { FPL("//aa"),          FPL("aa") },
    { FPL("0:"),            FPL("0:") },
    { FPL("@:"),            FPL("@:") },
    { FPL("[:"),            FPL("[:") },
    { FPL("`:"),            FPL("`:") },
    { FPL("{:"),            FPL("{:") },
    { FPL("\xB3:"),         FPL("\xB3:") },
    { FPL("\xC5:"),         FPL("\xC5:") },
#if defined(OS_WIN)
    { FPL("\x0143:"),       FPL("\x0143:") },
#endif  // OS_WIN
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("c:"),            FPL("") },
    { FPL("C:"),            FPL("") },
    { FPL("A:"),            FPL("") },
    { FPL("Z:"),            FPL("") },
    { FPL("a:"),            FPL("") },
    { FPL("z:"),            FPL("") },
    { FPL("c:aa"),          FPL("aa") },
    { FPL("c:/"),           FPL("/") },
    { FPL("c://"),          FPL("//") },
    { FPL("c:///"),         FPL("/") },
    { FPL("c:/aa"),         FPL("aa") },
    { FPL("c:/aa/"),        FPL("aa") },
    { FPL("c:/aa/bb"),      FPL("bb") },
    { FPL("c:aa/bb"),       FPL("bb") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { FPL("\\aa\\bb"),      FPL("bb") },
    { FPL("\\aa\\bb\\"),    FPL("bb") },
    { FPL("\\aa\\bb\\\\"),  FPL("bb") },
    { FPL("\\aa\\bb\\ccc"), FPL("ccc") },
    { FPL("\\aa"),          FPL("aa") },
    { FPL("\\"),            FPL("\\") },
    { FPL("\\\\"),          FPL("\\\\") },
    { FPL("\\\\\\"),        FPL("\\") },
    { FPL("aa\\"),          FPL("aa") },
    { FPL("aa\\bb"),        FPL("bb") },
    { FPL("aa\\bb\\"),      FPL("bb") },
    { FPL("aa\\bb\\\\"),    FPL("bb") },
    { FPL("aa\\\\bb\\\\"),  FPL("bb") },
    { FPL("aa\\\\bb\\"),    FPL("bb") },
    { FPL("aa\\\\bb"),      FPL("bb") },
    { FPL("\\\\aa\\bb"),    FPL("bb") },
    { FPL("\\\\aa\\"),      FPL("aa") },
    { FPL("\\\\aa"),        FPL("aa") },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("c:\\"),          FPL("\\") },
    { FPL("c:\\\\"),        FPL("\\\\") },
    { FPL("c:\\\\\\"),      FPL("\\") },
    { FPL("c:\\aa"),        FPL("aa") },
    { FPL("c:\\aa\\"),      FPL("aa") },
    { FPL("c:\\aa\\bb"),    FPL("bb") },
    { FPL("c:aa\\bb"),      FPL("bb") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#endif  // FILE_PATH_USES_WIN_SEPARATORS
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    FilePath::StringType observed = input.BaseName();
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, Append) {
  const struct BinaryTestData cases[] = {
    { { FPL(""),           FPL("cc") }, FPL("cc") },
    { { FPL("."),          FPL("ff") }, FPL("ff") },
    { { FPL("/"),          FPL("cc") }, FPL("/cc") },
    { { FPL("/aa"),        FPL("") },   FPL("/aa") },
    { { FPL("/aa/"),       FPL("") },   FPL("/aa") },
    { { FPL("//aa"),       FPL("") },   FPL("//aa") },
    { { FPL("//aa/"),      FPL("") },   FPL("//aa") },
    { { FPL("//"),         FPL("aa") }, FPL("//aa") },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { { FPL("c:"),         FPL("a") },  FPL("c:a") },
    { { FPL("c:"),         FPL("") },   FPL("c:") },
    { { FPL("c:/"),        FPL("a") },  FPL("c:/a") },
    { { FPL("c://"),       FPL("a") },  FPL("c://a") },
    { { FPL("c:///"),      FPL("a") },  FPL("c:/a") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    // Append introduces the default separator character, so these test cases
    // need to be defined with different expected results on platforms that use
    // different default separator characters.
    { { FPL("\\"),         FPL("cc") }, FPL("\\cc") },
    { { FPL("\\aa"),       FPL("") },   FPL("\\aa") },
    { { FPL("\\aa\\"),     FPL("") },   FPL("\\aa") },
    { { FPL("\\\\aa"),     FPL("") },   FPL("\\\\aa") },
    { { FPL("\\\\aa\\"),   FPL("") },   FPL("\\\\aa") },
    { { FPL("\\\\"),       FPL("aa") }, FPL("\\\\aa") },
    { { FPL("/aa/bb"),     FPL("cc") }, FPL("/aa/bb\\cc") },
    { { FPL("/aa/bb/"),    FPL("cc") }, FPL("/aa/bb\\cc") },
    { { FPL("aa/bb/"),     FPL("cc") }, FPL("aa/bb\\cc") },
    { { FPL("aa/bb"),      FPL("cc") }, FPL("aa/bb\\cc") },
    { { FPL("a/b"),        FPL("c") },  FPL("a/b\\c") },
    { { FPL("a/b/"),       FPL("c") },  FPL("a/b\\c") },
    { { FPL("//aa"),       FPL("bb") }, FPL("//aa\\bb") },
    { { FPL("//aa/"),      FPL("bb") }, FPL("//aa\\bb") },
    { { FPL("\\aa\\bb"),   FPL("cc") }, FPL("\\aa\\bb\\cc") },
    { { FPL("\\aa\\bb\\"), FPL("cc") }, FPL("\\aa\\bb\\cc") },
    { { FPL("aa\\bb\\"),   FPL("cc") }, FPL("aa\\bb\\cc") },
    { { FPL("aa\\bb"),     FPL("cc") }, FPL("aa\\bb\\cc") },
    { { FPL("a\\b"),       FPL("c") },  FPL("a\\b\\c") },
    { { FPL("a\\b\\"),     FPL("c") },  FPL("a\\b\\c") },
    { { FPL("\\\\aa"),     FPL("bb") }, FPL("\\\\aa\\bb") },
    { { FPL("\\\\aa\\"),   FPL("bb") }, FPL("\\\\aa\\bb") },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { { FPL("c:\\"),       FPL("a") },  FPL("c:\\a") },
    { { FPL("c:\\\\"),     FPL("a") },  FPL("c:\\\\a") },
    { { FPL("c:\\\\\\"),   FPL("a") },  FPL("c:\\a") },
    { { FPL("c:\\"),       FPL("") },   FPL("c:\\") },
    { { FPL("c:\\a"),      FPL("b") },  FPL("c:\\a\\b") },
    { { FPL("c:\\a\\"),    FPL("b") },  FPL("c:\\a\\b") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#else  // FILE_PATH_USES_WIN_SEPARATORS
    { { FPL("/aa/bb"),     FPL("cc") }, FPL("/aa/bb/cc") },
    { { FPL("/aa/bb/"),    FPL("cc") }, FPL("/aa/bb/cc") },
    { { FPL("aa/bb/"),     FPL("cc") }, FPL("aa/bb/cc") },
    { { FPL("aa/bb"),      FPL("cc") }, FPL("aa/bb/cc") },
    { { FPL("a/b"),        FPL("c") },  FPL("a/b/c") },
    { { FPL("a/b/"),       FPL("c") },  FPL("a/b/c") },
    { { FPL("//aa"),       FPL("bb") }, FPL("//aa/bb") },
    { { FPL("//aa/"),      FPL("bb") }, FPL("//aa/bb") },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { { FPL("c:/"),        FPL("a") },  FPL("c:/a") },
    { { FPL("c:/"),        FPL("") },   FPL("c:/") },
    { { FPL("c:/a"),       FPL("b") },  FPL("c:/a/b") },
    { { FPL("c:/a/"),      FPL("b") },  FPL("c:/a/b") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#endif  // FILE_PATH_USES_WIN_SEPARATORS
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath root(cases[i].inputs[0]);
    FilePath::StringType leaf(cases[i].inputs[1]);
    FilePath observed_str = root.Append(leaf);
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed_str.value()) <<
              "i: " << i << ", root: " << root.value() << ", leaf: " << leaf;
    FilePath observed_path = root.Append(FilePath(leaf));
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed_path.value()) <<
              "i: " << i << ", root: " << root.value() << ", leaf: " << leaf;
  }
}

TEST_F(FilePathTest, IsAbsolute) {
  const struct UnaryBooleanTestData cases[] = {
    { FPL(""),       false },
    { FPL("a"),      false },
    { FPL("c:"),     false },
    { FPL("c:a"),    false },
    { FPL("a/b"),    false },
    { FPL("//"),     true },
    { FPL("//a"),    true },
    { FPL("c:a/b"),  false },
    { FPL("?:/a"),   false },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("/"),      false },
    { FPL("/a"),     false },
    { FPL("/."),     false },
    { FPL("/.."),    false },
    { FPL("c:/"),    true },
    { FPL("c:/a"),   true },
    { FPL("c:/."),   true },
    { FPL("c:/.."),  true },
    { FPL("C:/a"),   true },
    { FPL("d:/a"),   true },
#else  // FILE_PATH_USES_DRIVE_LETTERS
    { FPL("/"),      true },
    { FPL("/a"),     true },
    { FPL("/."),     true },
    { FPL("/.."),    true },
    { FPL("c:/"),    false },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { FPL("a\\b"),   false },
    { FPL("\\\\"),   true },
    { FPL("\\\\a"),  true },
    { FPL("a\\b"),   false },
    { FPL("\\\\"),   true },
    { FPL("//a"),    true },
    { FPL("c:a\\b"), false },
    { FPL("?:\\a"),  false },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("\\"),     false },
    { FPL("\\a"),    false },
    { FPL("\\."),    false },
    { FPL("\\.."),   false },
    { FPL("c:\\"),   true },
    { FPL("c:\\"),   true },
    { FPL("c:\\a"),  true },
    { FPL("c:\\."),  true },
    { FPL("c:\\.."), true },
    { FPL("C:\\a"),  true },
    { FPL("d:\\a"),  true },
#else  // FILE_PATH_USES_DRIVE_LETTERS
    { FPL("\\"),     true },
    { FPL("\\a"),    true },
    { FPL("\\."),    true },
    { FPL("\\.."),   true },
    { FPL("c:\\"),   false },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#endif  // FILE_PATH_USES_WIN_SEPARATORS
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    bool observed = input.IsAbsolute();
    EXPECT_EQ(cases[i].expected, observed) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, Contains) {
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &data_dir));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("FilePathTest"));

  // Create a fresh, empty copy of this directory.
  file_util::Delete(data_dir, true);
  file_util::CreateDirectory(data_dir);

  FilePath foo(data_dir.Append(FILE_PATH_LITERAL("foo")));
  FilePath bar(foo.Append(FILE_PATH_LITERAL("bar.txt")));
  FilePath baz(data_dir.Append(FILE_PATH_LITERAL("baz.txt")));
  FilePath foobar(data_dir.Append(FILE_PATH_LITERAL("foobar.txt")));

  // Annoyingly, the directories must actually exist in order for realpath(),
  // which Contains() relies on in posix, to work.
  file_util::CreateDirectory(foo);
  std::string data("hello");
  file_util::WriteFile(bar.ToWStringHack(), data.c_str(), data.length());
  file_util::WriteFile(baz.ToWStringHack(), data.c_str(), data.length());
  file_util::WriteFile(foobar.ToWStringHack(), data.c_str(), data.length());

  EXPECT_TRUE(foo.Contains(bar));
  EXPECT_FALSE(foo.Contains(baz));
  EXPECT_FALSE(foo.Contains(foobar));
  EXPECT_FALSE(foo.Contains(foo));

// Platform-specific concerns
  FilePath foo_caps(data_dir.Append(FILE_PATH_LITERAL("FOO")));
#if defined(OS_WIN)
  EXPECT_TRUE(foo.Contains(foo_caps.Append(FILE_PATH_LITERAL("bar.txt"))));
  EXPECT_TRUE(foo.Contains(
      FilePath(foo.value() + FILE_PATH_LITERAL("/bar.txt"))));
#elif defined(OS_LINUX)
  EXPECT_FALSE(foo.Contains(foo_caps.Append(FILE_PATH_LITERAL("bar.txt"))));
#else
  // We can't really do this test on osx since the case-sensitivity of the
  // filesystem is configurable.
#endif

  // Note: whether 
}
