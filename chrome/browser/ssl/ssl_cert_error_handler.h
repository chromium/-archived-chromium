// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CERT_ERROR_HANDLER_H_
#define CHROME_BROWSER_SSL_SSL_CERT_ERROR_HANDLER_H_

#include <string>

#include "chrome/browser/ssl/ssl_error_handler.h"
#include "net/base/ssl_info.h"
#include "net/base/x509_certificate.h"

// A CertError represents an error that occurred with the certificate in an
// SSL session.  A CertError object exists both on the IO thread and on the UI
// thread and allows us to cancel/continue a request it is associated with.
class SSLCertErrorHandler : public SSLErrorHandler {
 public:
  // Construct on the IO thread.
  SSLCertErrorHandler(ResourceDispatcherHost* rdh,
                      URLRequest* request,
                      ResourceType::Type resource_type,
                      const std::string& frame_origin,
                      const std::string& main_frame_origin,
                      int cert_error,
                      net::X509Certificate* cert,
                      MessageLoop* ui_loop);

  virtual SSLCertErrorHandler* AsSSLCertErrorHandler() { return this; }

  // These accessors are available on either thread
  const net::SSLInfo& ssl_info() const { return ssl_info_; }
  int cert_error() const { return cert_error_; }

 protected:
  // SSLErrorHandler methods
  virtual void OnDispatchFailed();
  virtual void OnDispatched();

 private:
  // These read-only members may be accessed on any thread.
  net::SSLInfo ssl_info_;
  const int cert_error_;  // The error we represent.

  DISALLOW_COPY_AND_ASSIGN(SSLCertErrorHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_CERT_ERROR_HANDLER_H_
