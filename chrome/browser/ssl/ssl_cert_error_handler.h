// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CERT_ERROR_HANDLER_H_
#define CHROME_BROWSER_SSL_SSL_CERT_ERROR_HANDLER_H_

#include "chrome/browser/ssl/ssl_error_handler.h"
#include "chrome/browser/ssl/ssl_manager.h"
#include "net/base/ssl_info.h"
#include "net/base/x509_certificate.h"

// A CertError represents an error that occurred with the certificate in an
// SSL session.  A CertError object exists both on the IO thread and on the UI
// thread and allows us to cancel/continue a request it is associated with.
class SSLCertErrorHandler : public SSLErrorHandler {
 public:
  // Construct on the IO thread.
  // We mark this method as private because it is tricky to correctly
  // construct a CertError object.
  SSLCertErrorHandler(ResourceDispatcherHost* rdh,
                      URLRequest* request,
                      ResourceType::Type resource_type,
                      const std::string& frame_origin,
                      const std::string& main_frame_origin,
                      int cert_error,
                      net::X509Certificate* cert,
                      MessageLoop* ui_loop)
      : SSLErrorHandler(rdh, request, resource_type, frame_origin,
                        main_frame_origin, ui_loop),
        cert_error_(cert_error) {
    DCHECK(request == resource_dispatcher_host_->GetURLRequest(request_id_));

    // We cannot use the request->ssl_info(), it's not been initialized yet, so
    // we have to set the fields manually.
    ssl_info_.cert = cert;
    ssl_info_.SetCertError(cert_error);
  }

  virtual SSLCertErrorHandler* AsSSLCertErrorHandler() { return this; }

  // These accessors are available on either thread
  const net::SSLInfo& ssl_info() const { return ssl_info_; }
  int cert_error() const { return cert_error_; }

 protected:
  // SSLErrorHandler methods
  virtual void OnDispatchFailed() { CancelRequest(); }
  virtual void OnDispatched() { manager_->OnCertError(this); }

 private:
  // These read-only members may be accessed on any thread.
  net::SSLInfo ssl_info_;
  const int cert_error_;  // The error we represent.

  DISALLOW_COPY_AND_ASSIGN(SSLCertErrorHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_CERT_ERROR_HANDLER_H_
