// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "net/base/directory_lister.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class DirectoryListerTest : public testing::Test {};
}

class ListerDelegate : public net::DirectoryLister::DirectoryListerDelegate {
 public:
  ListerDelegate() : error_(-1) {
  }
  void OnListFile(const file_util::FileEnumerator::FindInfo& data) {
  }
  void OnListDone(int error) {
    error_ = error;
    MessageLoop::current()->Quit();
  }
  int error() const { return error_; }
 private:
  int error_;
};

TEST(DirectoryListerTest, BigDirTest) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &path));

  ListerDelegate delegate;
  scoped_refptr<net::DirectoryLister> lister =
      new net::DirectoryLister(path, &delegate);

  lister->Start();

  MessageLoop::current()->Run();

  EXPECT_EQ(delegate.error(), net::OK);
}

TEST(DirectoryListerTest, CancelTest) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &path));

  ListerDelegate delegate;
  scoped_refptr<net::DirectoryLister> lister =
      new net::DirectoryLister(path, &delegate);

  lister->Start();
  lister->Cancel();

  MessageLoop::current()->Run();

  EXPECT_EQ(delegate.error(), net::ERR_ABORTED);
}
