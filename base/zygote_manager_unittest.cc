// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/zygote_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

#include "testing/gtest/include/gtest/gtest.h"

using file_util::Delete;
using file_util::WriteFile;
using file_util::ReadFileToString;
using file_util::GetCurrentDirectory;

#if defined(OS_LINUX)
// ZygoteManager is only used on Linux at the moment

typedef testing::Test ZygoteManagerTest;

TEST_F(ZygoteManagerTest, Ping) {
  base::ZygoteManager zm;

  scoped_ptr< std::vector<std::string> > new_argv;
  new_argv.reset(zm.Start());
  EXPECT_TRUE(new_argv.get() == NULL);
  // Measure round trip time
  base::TimeDelta delta;
  EXPECT_EQ(zm.Ping(&delta), true);
  EXPECT_LT(delta.InMilliseconds(), 5000);
}

TEST_F(ZygoteManagerTest, SpawnChild) {
  base::ZygoteManager zm;
  const int kDummyChildExitCode = 39;

  scoped_ptr< std::vector<std::string> > new_argv;
  new_argv.reset(zm.Start());
  if (new_argv.get() == NULL) {
    // original process
    // Launch a child process
    std::vector<std::string> myargs;
    myargs.push_back(std::string("0thArg"));
    myargs.push_back(std::string("1stArg"));
    base::file_handle_mapping_vector no_files;
    pid_t child = zm.LongFork(myargs, no_files);
    EXPECT_NE(child, -1);
    EXPECT_NE(child, 0);
    LOG(INFO) << "child pid " << child;

    // ZygoteManager doesn't support waiting for exit status
    int status;
    int err = HANDLE_EINTR(waitpid(child, &status, 0));
    EXPECT_EQ(-1, err);
    EXPECT_EQ(ECHILD, errno);
  } else {
    LOG(INFO) << "Hello from child!";
    // child process
    std::string arg0(new_argv.get()->at(0));
    std::string arg1(new_argv.get()->at(1));
    EXPECT_EQ(arg0, std::string("0thArg"));
    EXPECT_EQ(arg1, std::string("1stArg"));
    exit(kDummyChildExitCode);
  }
}

TEST_F(ZygoteManagerTest, MapFile) {
  base::ZygoteManager zm;
  const int kDummyChildExitCode = 39;
  const int kSpecialFDSlot = 5;

  scoped_ptr< std::vector<std::string> > new_argv;
  new_argv.reset(zm.Start());
  if (new_argv.get() == NULL) {
    // original process
    // Launch a child process
    std::vector<std::string> myargs;
    myargs.push_back(std::string("0thArg"));
    myargs.push_back(std::string("1stArg"));
    base::file_handle_mapping_vector fds_to_map;
    int fd = open("/tmp/zygote_manager_unittest.tmp", O_CREAT|O_RDWR|O_TRUNC, 0644);
    fds_to_map.push_back(std::pair<int, int>(fd, kSpecialFDSlot));
    pid_t child = zm.LongFork(myargs, fds_to_map);
    EXPECT_NE(child, -1);
    EXPECT_NE(child, 0);
    // FIXME: really wait for child
    sleep(3);
    char buf[100];

    // Expect fd to be seeked to end, so reading without seeking should fail
    off_t loc = lseek(fd, 0L, SEEK_CUR);
    EXPECT_EQ(3, loc);

    memset(buf, 0, sizeof(buf));
    int nread = read(fd, buf, 5);
    EXPECT_EQ(0, nread);

    // Try again from beginning
    lseek(fd, 0L, SEEK_SET);
    memset(buf, 0, sizeof(buf));
    nread = read(fd, buf, 2);
    EXPECT_EQ(2, nread);
    EXPECT_EQ(strncmp(buf, "hi", 2), 0);
    close(fd);
  } else {
    // child process
    // Write three bytes; this happens to seek the file descriptor to the end.
    int nwritten = write(kSpecialFDSlot, "hi\n", 3);
    EXPECT_EQ(3, nwritten);
    exit(kDummyChildExitCode);
  }
}

TEST_F(ZygoteManagerTest, OpenFile) {
  base::ZygoteManager zm;

  scoped_ptr< std::vector<std::string> > new_argv;
  new_argv.reset(zm.Start());
  EXPECT_EQ(NULL, new_argv.get());

  const char kSomeText[] = "foobar\n";

  // Verify that we disallow nonabsolute paths.
  FilePath badfilepath(FilePath::kCurrentDirectory);
  badfilepath = badfilepath.AppendASCII("zygote_manager_test.pak");
  ASSERT_FALSE(badfilepath.IsAbsolute());
  EXPECT_NE(-1, WriteFile(badfilepath, kSomeText, strlen(kSomeText)));
  std::string badfilename = WideToASCII(badfilepath.ToWStringHack());
  int fd = zm.OpenFile(badfilename);
  EXPECT_EQ(-1, fd);
  EXPECT_TRUE(Delete(badfilepath, false));

  // Verify that we disallow non-plain files.
  ASSERT_TRUE(GetCurrentDirectory(&badfilepath));
  badfilepath = badfilepath.AppendASCII("zygote_manager_test.pak");
  std::string badfilenameA = WideToASCII(badfilepath.ToWStringHack());
  EXPECT_EQ(0, mkfifo(badfilenameA.c_str(), 0644));
  badfilename = WideToASCII(badfilepath.ToWStringHack());
  fd = zm.OpenFile(badfilename);
  ASSERT_EQ(-1, fd);
  EXPECT_TRUE(Delete(badfilepath, false));

  // Verify that we disallow files not ending in .pak.
  ASSERT_TRUE(GetCurrentDirectory(&badfilepath));
  badfilepath = badfilepath.AppendASCII("zygote_manager_test.tmp");
  ASSERT_NE(-1, WriteFile(badfilepath, kSomeText, strlen(kSomeText)));
  badfilename = WideToASCII(badfilepath.ToWStringHack());
  fd = zm.OpenFile(badfilename);
  ASSERT_EQ(-1, fd);
  EXPECT_TRUE(Delete(badfilepath, false));

  // Verify that we disallow files in /etc
  badfilepath = FilePath(FILE_PATH_LITERAL("/"));
  badfilepath = badfilepath.AppendASCII("etc");
  badfilepath = badfilepath.AppendASCII("hosts");
  EXPECT_TRUE(badfilepath.IsAbsolute());
  badfilename = WideToASCII(badfilepath.ToWStringHack());
  fd = zm.OpenFile(badfilename);
  ASSERT_EQ(-1, fd);

  // Verify that we disallow files in /dev
  badfilepath = FilePath(FILE_PATH_LITERAL("/"));
  badfilepath = badfilepath.AppendASCII("dev");
  badfilepath = badfilepath.AppendASCII("tty");
  EXPECT_TRUE(badfilepath.IsAbsolute());
  badfilename = WideToASCII(badfilepath.ToWStringHack());
  fd = zm.OpenFile(badfilename);
  ASSERT_EQ(-1, fd);

  // Verify that we allow absolute paths with filename ending in .pak,
  // and that we can open them a second time even if they were
  // deleted after we opened them the first time.
  // Because of our restrictive filename checks, can't put
  // test file in /tmp, so put it in current directory.
  FilePath goodfilepath;
  ASSERT_TRUE(GetCurrentDirectory(&goodfilepath));
  goodfilepath = goodfilepath.AppendASCII("zygote_manager_test.pak");
  ASSERT_NE(-1, WriteFile(goodfilepath, kSomeText, strlen(kSomeText)));
  std::string goodfilename = WideToASCII(goodfilepath.ToWStringHack());
  for (int i = 0; i < 2; i++) {
    fd = zm.OpenFile(goodfilename);
    ASSERT_NE(-1, fd);
    char buf[sizeof(kSomeText)];
    // Can't use read because it depends on file position.
    // (In practice these files are mmapped.)
    int nread = pread(fd, buf, strlen(kSomeText), 0);
    ASSERT_EQ(strlen(kSomeText), static_cast<size_t>(nread));
    EXPECT_EQ(0, strncmp(buf, kSomeText, strlen(kSomeText)));
    EXPECT_EQ(0, close(fd));

    // oddly, our Delete returns true for nonexistant files.
    EXPECT_EQ(true, Delete(goodfilepath, false));
  }
}

TEST_F(ZygoteManagerTest, ChildOpenFile) {
  base::ZygoteManager zm;
  const int kDummyChildExitCode = 39;

  const char kSomeText[] = "foobar\n";

  FilePath resultfilepath(FILE_PATH_LITERAL("/tmp"));
  resultfilepath = resultfilepath.AppendASCII("zygote_manager_test_result.tmp");
  EXPECT_EQ(true, Delete(resultfilepath, false));

  scoped_ptr< std::vector<std::string> > new_argv;
  new_argv.reset(zm.Start());
  if (new_argv.get() == NULL) {
    // original process
    // Launch a child process
    std::vector<std::string> myargs;
    base::file_handle_mapping_vector no_files;
    pid_t child = zm.LongFork(myargs, no_files);
    EXPECT_NE(child, -1);
    EXPECT_NE(child, 0);
    LOG(INFO) << "child pid " << child;

    // Wait for resultfile to be created
    std::string result;
    int nloops = 0;
    while (!ReadFileToString(resultfilepath, &result)) {
      sleep(1);
      ++nloops;
      ASSERT_NE(10, nloops);
    }
    ASSERT_EQ(result, std::string(kSomeText));

    EXPECT_EQ(true, Delete(resultfilepath, false));

  } else {
    LOG(INFO) << "Hello from child!";

    FilePath goodfilepath;
    ASSERT_TRUE(GetCurrentDirectory(&goodfilepath));
    goodfilepath = goodfilepath.AppendASCII("zygote_manager_test.pak");
    ASSERT_NE(-1, WriteFile(goodfilepath, kSomeText, strlen(kSomeText)));
    std::string goodfilename = WideToASCII(goodfilepath.ToWStringHack());
    int fd = zm.OpenFile(goodfilename);
    EXPECT_EQ(true, Delete(goodfilepath, false));
    ASSERT_NE(-1, fd);
    char buf[sizeof(kSomeText)];
    // Can't use read because it depends on file position.
    // (In practice these files are mmapped.)
    int nread = pread(fd, buf, strlen(kSomeText), 0);
    ASSERT_EQ(strlen(kSomeText), static_cast<size_t>(nread));
    EXPECT_EQ(0, strncmp(buf, kSomeText, strlen(kSomeText)));
    EXPECT_EQ(0, close(fd));

    ASSERT_NE(-1, WriteFile(resultfilepath, kSomeText, strlen(kSomeText)));

    exit(kDummyChildExitCode);
  }
}

#endif
