// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/path_service.h"
#include "net/base/directory_lister.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DirectoryListerTest : public testing::Test {
};

}

class DirectoryListerDelegate : public net::DirectoryLister::Delegate {
 public:
  DirectoryListerDelegate() : error_(-1) {
  }
  void OnListFile(const WIN32_FIND_DATA& data) {
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
  std::wstring windows_path;
  ASSERT_TRUE(PathService::Get(base::DIR_WINDOWS, &windows_path));

  DirectoryListerDelegate delegate;
  scoped_refptr<net::DirectoryLister> lister =
      new net::DirectoryLister(windows_path, &delegate);

  lister->Start();

  MessageLoop::current()->Run();

  EXPECT_EQ(delegate.error(), ERROR_SUCCESS);
}

TEST(DirectoryListerTest, CancelTest) {
  std::wstring windows_path;
  ASSERT_TRUE(PathService::Get(base::DIR_WINDOWS, &windows_path));

  DirectoryListerDelegate delegate;
  scoped_refptr<net::DirectoryLister> lister =
      new net::DirectoryLister(windows_path, &delegate);

  lister->Start();
  lister->Cancel();

  MessageLoop::current()->Run();

  EXPECT_EQ(delegate.error(), ERROR_OPERATION_ABORTED);
  EXPECT_EQ(lister->was_canceled(), true);
}

