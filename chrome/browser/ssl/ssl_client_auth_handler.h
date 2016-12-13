// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H
#define CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "net/base/ssl_cert_request_info.h"

namespace net {
class X509Certificate;
}
class MessageLoop;
class URLRequest;

// This class handles the approval and selection of a certificate for SSL client
// authentication by the user.
// It is self-owned and deletes itself when the UI reports the user selection or
// when the URLRequest is cancelled.
class SSLClientAuthHandler : public base::RefCounted<SSLClientAuthHandler> {
 public:
  SSLClientAuthHandler(URLRequest* request,
                       net::SSLCertRequestInfo* cert_request_info,
                       MessageLoop* io_loop,
                       MessageLoop* ui_loop);
  ~SSLClientAuthHandler();

  // Asks the user to select a certificate and resumes the URL request with that
  // certificate.
  // Should only be called on the IO thread.
  void SelectCertificate();

  // Invoked when the request associated with this handler is cancelled.
  // Should only be called on the IO thread.
  void OnRequestCancelled();

 private:
  // Asks the user for a cert.
  // Called on the UI thread.
  void DoSelectCertificate();

  // Notifies that the user has selected a cert.
  // Called on the IO thread.
  void CertificateSelected(net::X509Certificate* cert);

  // The URLRequest that triggered this client auth.
  URLRequest* request_;

  // The certs to choose from.
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

  MessageLoop* io_loop_;
  MessageLoop* ui_loop_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientAuthHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H
