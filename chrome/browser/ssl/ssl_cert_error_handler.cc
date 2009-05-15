// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_cert_error_handler.h"

#include "chrome/browser/ssl/ssl_manager.h"
#include "chrome/browser/ssl/ssl_policy.h"

SSLCertErrorHandler::SSLCertErrorHandler(
    ResourceDispatcherHost* rdh,
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

void SSLCertErrorHandler::OnDispatchFailed() {
  CancelRequest();
}

void SSLCertErrorHandler::OnDispatched() {
  manager_->policy()->OnCertError(this);
}
