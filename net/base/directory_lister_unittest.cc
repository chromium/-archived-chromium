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

#include "base/message_loop.h"
#include "base/path_service.h"
#include "net/base/directory_lister.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class DirectoryListerTest : public testing::Test {
  };
}

class DirectoryListerDelegate : public DirectoryLister::Delegate {
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
  scoped_refptr<DirectoryLister> lister =
      new DirectoryLister(windows_path, &delegate);

  lister->Start();

  MessageLoop::current()->Run();

  EXPECT_EQ(delegate.error(), ERROR_SUCCESS);
}

TEST(DirectoryListerTest, CancelTest) {
  std::wstring windows_path;
  ASSERT_TRUE(PathService::Get(base::DIR_WINDOWS, &windows_path));

  DirectoryListerDelegate delegate;
  scoped_refptr<DirectoryLister> lister =
      new DirectoryLister(windows_path, &delegate);

  lister->Start();
  lister->Cancel();

  MessageLoop::current()->Run();

  EXPECT_EQ(delegate.error(), ERROR_OPERATION_ABORTED);
  EXPECT_EQ(lister->was_canceled(), true);
}
