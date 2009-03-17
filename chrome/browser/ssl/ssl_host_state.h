// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_HOST_STATE_H_
#define CHROME_BROWSER_SSL_SSL_HOST_STATE_H_

#include <string>
#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/non_thread_safe.h"
#include "googleurl/src/gurl.h"
#include "net/base/x509_certificate.h"

// SSLHostState
//
// The SSLHostState encapulates the host-specific state for SSL errors.  For
// example, SSLHostState remembers whether the user has whitelisted a
// particular broken cert for use with particular host.  We separate this state
// from the SSLManager because this state is shared across many navigation
// controllers.

class SSLHostState : public NonThreadSafe {
 public:
  SSLHostState();
  ~SSLHostState();

  // Records that a host is "broken," that is, the origin for that host has been
  // contaminated with insecure content, either via HTTP or via HTTPS with a
  // bad certificate.
  void MarkHostAsBroken(const std::string& host);

  // Returns whether the specified host was marked as broken.
  bool DidMarkHostAsBroken(const std::string& host);

  // Records that |cert| is permitted to be used for |host| in the future.
  void DenyCertForHost(net::X509Certificate* cert, const std::string& host);

  // Records that |cert| is not permitted to be used for |host| in the future.
  void AllowCertForHost(net::X509Certificate* cert, const std::string& host);

  // Queries whether there is at least one certificate that has been manually
  // allowed for this host.
  bool DidAllowCertForHost(const std::string& host);

  // Queries whether |cert| is allowed or denied for |host|.
  net::X509Certificate::Policy::Judgment QueryPolicy(
      net::X509Certificate* cert, const std::string& host);

  // Allows mixed content to be visible (non filtered).
  void AllowMixedContentForHost(const std::string& host);

  // Returns whether the specified host is allowed to show mixed content.
  bool DidAllowMixedContentForHost(const std::string& host);

 private:
  // Hosts which have been contaminated with unsafe content.
  std::set<std::string> broken_hosts_;

  // Certificate policies for each host.
  std::map<std::string, net::X509Certificate::Policy> cert_policy_for_host_;

  // Hosts for which we are allowed to show mixed content.
  std::set<std::string> allow_mixed_content_for_host_;

  DISALLOW_COPY_AND_ASSIGN(SSLHostState);
};

#endif  // CHROME_BROWSER_SSL_SSL_HOST_STATE_H_
