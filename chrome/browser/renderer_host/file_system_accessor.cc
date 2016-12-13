// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/file_system_accessor.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"

FileSystemAccessor::FileSystemAccessor(void* param, FileSizeCallback* callback)
    : param_(param), callback_(callback) {
  caller_loop_ = MessageLoop::current();
}

FileSystemAccessor::~FileSystemAccessor() {
}

void FileSystemAccessor::RequestFileSize(const FilePath& path,
                                         void* param,
                                         FileSizeCallback* callback) {
  // Getting file size could take long time if it lives on a network share,
  // so run it on FILE thread.
  ChromeThread::GetMessageLoop(ChromeThread::FILE)->PostTask(
      FROM_HERE,
      NewRunnableMethod(new FileSystemAccessor(param, callback),
                        &FileSystemAccessor::GetFileSize, path));
}

void FileSystemAccessor::GetFileSize(const FilePath& path) {
  int64 result;
  // Set result to -1 if failed to get file size.
  if (!file_util::GetFileSize(path, &result))
    result = -1;

  // Pass the result back to the caller thread.
  caller_loop_->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &FileSystemAccessor::GetFileSizeCompleted, result));
}

void FileSystemAccessor::GetFileSizeCompleted(int64 result) {
  callback_->Run(result, param_);
}
