/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the tests of class TemporaryFile.

#include <stdio.h>

#include "base/file_util.h"
#include "tests/common/win/testing_common.h"
#include "utils/cross/temporary_file.h"

namespace o3d {

class TemporaryFileTest : public testing::Test {
};

TEST_F(TemporaryFileTest, BasicConstruction) {
  TemporaryFile temporary_file;
  EXPECT_TRUE(temporary_file.path().empty());
}

TEST_F(TemporaryFileTest, BasicFunction) {
  FilePath path;
  file_util::CreateTemporaryFileName(&path);
  EXPECT_TRUE(file_util::PathExists(path));
  {
    TemporaryFile temporary_file(path);
    EXPECT_TRUE(file_util::PathExists(path));
    EXPECT_EQ(path.value(), temporary_file.path().value());
    EXPECT_TRUE(file_util::PathExists(temporary_file.path()));
  }
  EXPECT_FALSE(file_util::PathExists(path));
}

TEST_F(TemporaryFileTest, Reset) {
  FilePath path;
  FilePath path1;
  file_util::CreateTemporaryFileName(&path);
  file_util::CreateTemporaryFileName(&path1);
  EXPECT_TRUE(file_util::PathExists(path));
  EXPECT_TRUE(file_util::PathExists(path1));
  {
    TemporaryFile temporary_file(path1);
    EXPECT_EQ(path1.value(), temporary_file.path().value());
    EXPECT_TRUE(file_util::PathExists(path1));
    EXPECT_TRUE(file_util::PathExists(temporary_file.path()));
    temporary_file.Reset(path);
    EXPECT_TRUE(file_util::PathExists(path));
    EXPECT_EQ(path.value(), temporary_file.path().value());
    EXPECT_FALSE(file_util::PathExists(path1));
    EXPECT_TRUE(file_util::PathExists(path));
    EXPECT_TRUE(file_util::PathExists(temporary_file.path()));
  }
  EXPECT_FALSE(file_util::PathExists(path));
  EXPECT_FALSE(file_util::PathExists(path1));
}

TEST_F(TemporaryFileTest, Release) {
  FilePath path;
  file_util::CreateTemporaryFileName(&path);
  EXPECT_TRUE(file_util::PathExists(path));
  {
    TemporaryFile temporary_file(path);
    EXPECT_TRUE(file_util::PathExists(path));
    EXPECT_TRUE(file_util::PathExists(temporary_file.path()));
    temporary_file.Release();
  }
  EXPECT_TRUE(file_util::PathExists(path));
}

TEST_F(TemporaryFileTest, Create) {
  FilePath test_path;
  {
    TemporaryFile temporary_file;
    TemporaryFile::Create(&temporary_file);
    EXPECT_FALSE(temporary_file.path().empty());
    EXPECT_TRUE(file_util::PathExists(temporary_file.path()));
    EXPECT_TRUE(file_util::PathIsWritable(temporary_file.path()));
    test_path = temporary_file.path();
  }
  EXPECT_FALSE(file_util::PathExists(test_path));
}

}  // namespace o3d
