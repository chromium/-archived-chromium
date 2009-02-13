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
// example, SSLHostState rememebers whether the user has whitelisted a
// particular broken cert for use with particular host.  We separate this state
// from the SSLManager because this state is shared across many navigation
// controllers.

class SSLHostState : public NonThreadSafe {
 public:
  SSLHostState();
  ~SSLHostState();

  // Records that |cert| is permitted to be used for |host| in the future.
  void DenyCertForHost(net::X509Certificate* cert, const std::string& host);

  // Records that |cert| is not permitted to be used for |host| in the future.
  void AllowCertForHost(net::X509Certificate* cert, const std::string& host);

  // Queries whether |cert| is allowed or denied for |host|.
  net::X509Certificate::Policy::Judgment QueryPolicy(
      net::X509Certificate* cert, const std::string& host);

  // Allow mixed/unsafe content to be visible (non filtered) for the specified
  // URL.
  // Note that the current implementation allows on a host name basis.
  void AllowShowInsecureContentForURL(const GURL& url);

  // Returns whether the specified URL is allowed to show insecure (mixed or
  // unsafe) content.
  bool CanShowInsecureContent(const GURL& url);

 private:
  // Certificate policies for each host.
  std::map<std::string, net::X509Certificate::Policy> cert_policy_for_host_;

  // Domains for which it is OK to show insecure content.
  std::set<std::string> can_show_insecure_content_for_host_;

  DISALLOW_COPY_AND_ASSIGN(SSLHostState);
};

#endif  // CHROME_BROWSER_SSL_SSL_HOST_STATE_H_
