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
//
// MimeSnifferProxy wraps an URLRequest to use mime_util's MIME
// sniffer to better report the content's MIME type.
// It only supports a subset of the URLRequest API, and must be used together
// with an URLRequest.  Their lifetimes should be the same.
//
// To use it, create a normal URLRequest and initialize it appropriately,
// then insert a MimeSnifferProxy between your object and the URLRequest:
//   ms_.reset(new MimeSnifferProxy(url_request, this));
// It then proxies URLRequest delegate callbacks (from URLRequest back into
// your object) appropriately.
//
// For the other direction of calls (from your object to URLRequest), be sure
// to use two MimeSniffed functions in place of the URLRequest functions:
// 1) ms_->Read() -- just like URLRequest::Read()
// 2) ms_->mime_type() -- returns the sniffed mime type of the data;
//    valid after OnResponseStarted() is called.

#include "net/url_request/url_request.h"

class MimeSnifferProxy : public URLRequest::Delegate {
 public:
  // The constructor inserts this MimeSnifferProxy in between the URLRequest
  // and the URLRequest::Delegate, so that the URLRequest's delegate callbacks
  // first go through the MimeSnifferProxy.
  MimeSnifferProxy(URLRequest* request, URLRequest::Delegate* delegate);

  // URLRequest::Delegate implementation.
  // These first two functions are handled specially:
  virtual void OnResponseStarted(URLRequest* request);
  virtual void OnReadCompleted(URLRequest* request, int bytes_read);
  // The remaining three just proxy directly to the delegate:
  virtual void OnReceivedRedirect(URLRequest* request,
                                  const GURL& new_url) {
    delegate_->OnReceivedRedirect(request, new_url);
  }
  virtual void OnAuthRequired(URLRequest* request,
                              net::AuthChallengeInfo* auth_info) {
    delegate_->OnAuthRequired(request, auth_info);
  }
  virtual void OnSSLCertificateError(URLRequest* request,
                                     int cert_error,
                                     X509Certificate* cert) {
    delegate_->OnSSLCertificateError(request, cert_error, cert);
  }

  // Wrapper around URLRequest::Read.
  bool Read(char* buf, int max_bytes, int *bytes_read);

  // Return the sniffed mime type of the request.  Valid after
  // OnResponseStarted() has been called on the delegate.
  const std::string& mime_type() const { return mime_type_; }

 private:
  // The request underneath us.
  URLRequest* request_;
  // The delegate above us, that we're proxying the request to.
  URLRequest::Delegate* delegate_;

  // The (sniffed, if necessary) request mime type.
  std::string mime_type_;

  // Whether we're sniffing this request.
  bool sniff_content_;
  // Whether we've encountered an error on our initial Read().
  bool error_;

  // A buffer for the first bit of the request.
  char buf_[1024];
  // The number of bytes we've read into the buffer.
  int bytes_read_;
};
