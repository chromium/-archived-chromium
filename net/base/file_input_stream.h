// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// This file defines FileInputStream, a basic interface for reading files
// synchronously or asynchronously with support for seeking to an offset.

#ifndef NET_BASE_FILE_INPUT_STREAM_H_
#define NET_BASE_FILE_INPUT_STREAM_H_

#include "net/base/completion_callback.h"

#if defined(OS_WIN)
typedef void* HANDLE;
#endif

namespace net {

// TODO(darin): Move this to a more generic location.
enum Whence {
  FROM_BEGIN,
  FROM_CURRENT,
  FROM_END
};

class FileInputStream {
 public:
  FileInputStream();
  ~FileInputStream();

  // Call this method to close the FileInputStream.  It is OK to call Close
  // multiple times.  Redundant calls are ignored.
  void Close();

  // Call this method to open the FileInputStream.  The remaining methods
  // cannot be used unless this method returns OK.  If the file cannot be
  // opened then an error code is returned.
  //
  // NOTE: The file input stream is opened with non-exclusive access to the
  // underlying file.
  //
  int Open(const std::wstring& path, bool asynchronous_mode);

  // Returns true if Open succeeded and Close has not been called.
  bool IsOpen() const;

  // Adjust the position from where data is read.  Upon success, the stream
  // position relative to the start of the file is returned.  Otherwise, an
  // error code is returned.  It is not valid to call Seek while a Read call
  // has a pending completion.
  int64 Seek(Whence whence, int64 offset);

  // Returns the number of bytes available to read from the current stream
  // position until the end of the file.  Otherwise, an error code is returned.
  int64 Available();

  // Call this method to read data from the current stream position.  Up to
  // buf_len bytes will be copied into buf.  (In other words, partial reads are
  // allowed.)  Returns the number of bytes copied, 0 if at end-of-file, or an
  // error code if the operation could not be performed.
  //
  // If opened with |asynchronous_mode| set to true, then a non-null callback
  // must be passed to this method.  In asynchronous mode, if the read could
  // not complete synchronously, then ERR_IO_PENDING is returned, and the
  // callback will be notified on the current thread (via the MessageLoop) when
  // the read has completed.
  // 
  // In the case of an asychronous read, the memory pointed to by |buf| must
  // remain valid until the callback is notified.  However, it is valid to
  // destroy or close the file stream while there is an asynchronous read in
  // progress.  That will cancel the read and allow the buffer to be freed.
  // 
  int Read(char* buf, int buf_len, CompletionCallback* callback);

 private:
  class AsyncContext;
  friend class AsyncContext;

  // This member is used to support asynchronous reads.  It is non-null when
  // the FileInputStream was opened with asynchronous_mode set to true.
  scoped_ptr<AsyncContext> async_context_;

#if defined(OS_WIN)
  HANDLE handle_;
#endif

  DISALLOW_COPY_AND_ASSIGN(FileInputStream);
};

}  // namespace net

#endif  // NET_BASE_FILE_INPUT_STREAM_H_
