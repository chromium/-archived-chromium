// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_host_state.h"

#include "base/logging.h"

namespace {

static const char kDot = '.';

static bool IsIntranetHost(const std::string& host) {
  const size_t dot = host.find(kDot);
  return dot == std::string::npos || dot == host.length() - 1;
}

}  // namespace

SSLHostState::SSLHostState() {
}

SSLHostState::~SSLHostState() {
}

void SSLHostState::MarkHostAsBroken(const std::string& host) {
  DCHECK(CalledOnValidThread());

  broken_hosts_.insert(host);
}

bool SSLHostState::DidMarkHostAsBroken(const std::string& host) {
  DCHECK(CalledOnValidThread());

  // CAs issue certificate for intranet hosts to everyone.  Therefore, we always
  // treat intranet hosts as broken.
  if (IsIntranetHost(host))
    return true;

  return (broken_hosts_.find(host) != broken_hosts_.end());
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

void SSLHostState::AllowMixedContentForHost(const std::string& host) {
  DCHECK(CalledOnValidThread());

  allow_mixed_content_for_host_.insert(host);
}

bool SSLHostState::DidAllowMixedContentForHost(const std::string& host) {
  DCHECK(CalledOnValidThread());

  return (allow_mixed_content_for_host_.find(host) !=
      allow_mixed_content_for_host_.end());
}
