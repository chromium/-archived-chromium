// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FileSystemAccessor provides functions so consumers can do file access
// asynchronously. It would PostTask to FILE thread to access file info, then on
// completion PostTask back to the caller thread and pass the result back.
// Here is an example on how to use it to get file size:
//   1. Define a callback function so FileSystemAccessor could run it after the
//      task has been completed:
//      void Foo::GetFileSizeCallback(int64 result, void* param) {
//      }
//   2. Call static function FileSystemAccessor::RequestFileSize, provide file
//      path, param (any object you want to pass back to the callback function)
//      and callback:
//      FileSystemAccessor::RequestFileSize(
//          path,
//          param,
//          NewCallback(this, &Foo::GetFileSizeCallback));
//   3. FileSystemAceessor would PostTask to FILE thread to get file size, then
//      on completion it would PostTask back to the current thread and run
//      Foo::GetFileSizeCallback.
//

#ifndef CHROME_BROWSER_RENDERER_HOST_FILE_SYSTEM_ACCESSOR_H_
#define CHROME_BROWSER_RENDERER_HOST_FILE_SYSTEM_ACCESSOR_H_

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/ref_counted.h"
#include "base/task.h"

class MessageLoop;

class FileSystemAccessor
    : public base::RefCountedThreadSafe<FileSystemAccessor> {
 public:
  typedef Callback2<int64, void*>::Type FileSizeCallback;

  virtual ~FileSystemAccessor();

  // Request to get file size.
  //
  // param is an object that is owned by the caller and needs to pass back to
  // the caller by FileSystemAccessor through the callback function.
  // It can be set to NULL if no object needs to pass back.
  //
  // FileSizeCallback function is defined as:
  // void f(int64 result, void* param);
  // Variable result has the file size. If the file does not exist or there is
  // error accessing the file, result is set to -1. If the given path is a
  // directory, result is set to 0.
  static void RequestFileSize(const FilePath& path,
                              void* param,
                              FileSizeCallback* callback);

 private:
  FileSystemAccessor(void* param, FileSizeCallback* callback);

  // Get file size on the worker thread and pass result back to the caller
  // thread.
  void GetFileSize(const FilePath& path);

  // Getting file size completed, callback to reply message.
  void GetFileSizeCompleted(int64 result);

  MessageLoop* caller_loop_;
  void* param_;
  scoped_ptr<FileSizeCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemAccessor);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_FILE_SYSTEM_ACCESSOR_H_
