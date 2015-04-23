// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_TRANSACTION_H_
#define NET_FTP_FTP_TRANSACTION_H_

#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/load_states.h"

namespace net {

class FtpRequestInfo;
class FtpResponseInfo;

// Represents a single FTP transaction.
class FtpTransaction {
 public:
  // Stops any pending IO and destroys the transaction object.
  virtual ~FtpTransaction() {}

  // Starts the FTP transaction (i.e., sends the FTP request).
  //
  // Returns OK if the transaction could be started synchronously, which means
  // that the request was served from the cache (only supported for directory
  // listings).  ERR_IO_PENDING is returned to indicate that the
  // CompletionCallback will be notified once response info is available or if
  // an IO error occurs.  Any other return value indicates that the transaction
  // could not be started.
  //
  // Regardless of the return value, the caller is expected to keep the
  // request_info object alive until Destroy is called on the transaction.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  //
  virtual int Start(const FtpRequestInfo* request_info,
                    CompletionCallback* callback) = 0;

  // Restarts the FTP transaction with authentication credentials.
  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              CompletionCallback* callback) = 0;

  // Once response info is available for the transaction, response data may be
  // read by calling this method.
  //
  // Response data is copied into the given buffer and the number of bytes
  // copied is returned.  ERR_IO_PENDING is returned if response data is not
  // yet available.  The CompletionCallback is notified when the data copy
  // completes, and it is passed the number of bytes that were successfully
  // copied.  Or, if a read error occurs, the CompletionCallback is notified of
  // the error.  Any other negative return value indicates that the transaction
  // could not be read.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  //
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   CompletionCallback* callback) = 0;

  // Returns the response info for this transaction or NULL if the response
  // info is not available.
  virtual const FtpResponseInfo* GetResponseInfo() const = 0;

  // Returns the load state for this transaction.
  virtual LoadState GetLoadState() const = 0;

  // Returns the upload progress in bytes.  If there is no upload data,
  // zero will be returned.
  virtual uint64 GetUploadProgress() const = 0;
};

}  // namespace net

#endif  // NET_FTP_FTP_TRANSACTION_H_
