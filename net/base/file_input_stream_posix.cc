// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/base/file_input_stream.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

// FileInputStream::AsyncContext ----------------------------------------------

class FileInputStream::AsyncContext {
};

// FileInputStream ------------------------------------------------------------

FileInputStream::FileInputStream() {
}

FileInputStream::~FileInputStream() {
  Close();
}

void FileInputStream::Close() {
}

int FileInputStream::Open(const std::wstring& path, bool asynchronous_mode) {
  NOTIMPLEMENTED();
  return ERR_FAILED;
}

bool FileInputStream::IsOpen() const {
  NOTIMPLEMENTED();
  return false;
}

int64 FileInputStream::Seek(Whence whence, int64 offset) {
  NOTIMPLEMENTED();
  return ERR_FAILED;
}

int64 FileInputStream::Available() {
  NOTIMPLEMENTED();
  return ERR_FAILED;
}

int FileInputStream::Read(
    char* buf, int buf_len, CompletionCallback* callback) {
  NOTIMPLEMENTED();
  return ERR_FAILED;
}

}  // namespace net
