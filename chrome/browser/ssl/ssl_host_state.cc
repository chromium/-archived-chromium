// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_host_state.h"

#include "base/logging.h"

SSLHostState::SSLHostState() {
}

SSLHostState::~SSLHostState() {
}

void SSLHostState::DenyCertForHost(net::X509Certificate* cert,
                                   const std::string& host) {
  DCHECK(CalledOnValidThread());

  // Remember that we don't like this cert for this host.
  cert_policy_for_host_[host].Deny(cert);
}

void SSLHostState::AllowCertForHost(net::X509Certificate* cert,
                                    const std::string& host) {
  DCHECK(CalledOnValidThread());

  // Remember that we do like this cert for this host.
  cert_policy_for_host_[host].Allow(cert);
}

net::X509Certificate::Policy::Judgment SSLHostState::QueryPolicy(
    net::X509Certificate* cert, const std::string& host) {
  DCHECK(CalledOnValidThread());

  return cert_policy_for_host_[host].Check(cert);
}

bool SSLHostState::CanShowInsecureContent(const GURL& url) {
  DCHECK(CalledOnValidThread());

  return (can_show_insecure_content_for_host_.find(url.host()) !=
      can_show_insecure_content_for_host_.end());
}

void SSLHostState::AllowShowInsecureContentForURL(const GURL& url) {
  DCHECK(CalledOnValidThread());

  can_show_insecure_content_for_host_.insert(url.host());
}
