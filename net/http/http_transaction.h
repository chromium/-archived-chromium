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

#ifndef NET_HTTP_HTTP_TRANSACTION_H_
#define NET_HTTP_HTTP_TRANSACTION_H_

#include "net/base/completion_callback.h"
#include "net/base/load_states.h"

namespace net {

class HttpRequestInfo;
class HttpResponseInfo;

// Represents a single HTTP transaction (i.e., a single request/response pair).
// HTTP redirects are not followed and authentication challenges are not
// answered.  Cookies are assumed to be managed by the caller.
class HttpTransaction {
 public:
  // Stops any pending IO and destroys the transaction object.
  virtual void Destroy() = 0;

  // Starts the HTTP transaction (i.e., sends the HTTP request).
  //
  // Returns OK if the transaction could be started synchronously, which means
  // that the request was served from the cache.  ERR_IO_PENDING is returned to
  // indicate that the CompletionCallback will be notified once response info
  // is available or if an IO error occurs.  Any other return value indicates
  // that the transaction could not be started.
  //
  // Regardless of the return value, the caller is expected to keep the
  // request_info object alive until Destroy is called on the transaction.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  //
  virtual int Start(const HttpRequestInfo* request_info,
                    CompletionCallback* callback) = 0;

  // Restarts the HTTP transaction, ignoring the last error.  This call can
  // only be made after a call to Start (or RestartIgnoringLastError) failed.
  // Once Read has been called, this method cannot be called.  This method is
  // used, for example, to continue past various SSL related errors.
  //
  // Not all errors can be ignored using this method.  See error code
  // descriptions for details about errors that can be ignored.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  //
  virtual int RestartIgnoringLastError(CompletionCallback* callback) = 0;

  // Restarts the HTTP transaction with authentication credentials.
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
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback) = 0;

  // Returns the response info for this transaction or NULL if the response
  // info is not available.
  virtual const HttpResponseInfo* GetResponseInfo() const = 0;

  // Returns the load state for this transaction.
  virtual LoadState GetLoadState() const = 0;

  // Returns the upload progress in bytes.  If there is no upload data,
  // zero will be returned.  This does not include the request headers.
  virtual uint64 GetUploadProgress() const = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_H_
