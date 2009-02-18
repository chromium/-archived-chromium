// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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

#ifndef NET_URL_REQUEST_MIME_SNIFFER_PROXY_H_
#define NET_URL_REQUEST_MIME_SNIFFER_PROXY_H_

#include "net/url_request/url_request.h"

namespace net {
class IOBuffer;
}

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
                                     net::X509Certificate* cert) {
    delegate_->OnSSLCertificateError(request, cert_error, cert);
  }

  // Wrapper around URLRequest::Read.
  bool Read(net::IOBuffer* buf, int max_bytes, int *bytes_read);

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
  scoped_refptr<net::IOBuffer> buf_;
  // The number of bytes we've read into the buffer.
  int bytes_read_;
};

#endif  // NET_URL_REQUEST_MIME_SNIFFER_PROXY_H_
