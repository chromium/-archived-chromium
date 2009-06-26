// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"

namespace {

// We keep the file descriptor for /dev/urandom around so we don't need to
// reopen it (which is expensive), and since we may not even be able to reopen
// it if we are later put in a sandbox. This class wraps the file descriptor so
// we can use LazyInstance to handle opening it on the first access.
class URandomFd {
 public:
  URandomFd() {
    fd_ = open("/dev/urandom", O_RDONLY);
    CHECK(fd_ >= 0) << "Cannot open /dev/urandom: " << errno;
  }

  ~URandomFd() {
    close(fd_);
  }

  int fd() const { return fd_; }

 private:
  int fd_;
};

base::LazyInstance<URandomFd> g_urandom_fd(base::LINKER_INITIALIZED);

}  // namespace

namespace base {

uint64 RandUint64() {
  uint64 number;

  int urandom_fd = g_urandom_fd.Pointer()->fd();
  bool success = file_util::ReadFromFD(urandom_fd,
                                       reinterpret_cast<char*>(&number),
                                       sizeof(number));
  CHECK(success);

  return number;
}

}  // namespace base
