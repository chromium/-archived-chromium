// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_POLICY_H_
#define CHROME_BROWSER_SSL_SSL_POLICY_H_

#include "base/singleton.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_manager.h"

// SSLPolicy
//
// This class is responsible for making the security decisions that concern the
// SSL trust indicators.  It relies on the SSLManager to actually enact the
// decisions it reaches.
//
class SSLPolicy : public SSLManager::Delegate,
                  public SSLBlockingPage::Delegate {
 public:
  // Factory method to get the default policy.
  static SSLPolicy* GetDefaultPolicy();

  // SSLManager::Delegate methods.
  virtual void OnCertError(SSLCertErrorHandler* handler);
  virtual void OnMixedContent(SSLMixedContentHandler* handler);
  virtual void OnRequestStarted(SSLRequestInfo* info);
  virtual void UpdateEntry(SSLManager* manager, NavigationEntry* entry);

  // This method is static because it is called from both the UI and the IO
  // threads.
  static bool IsMixedContent(const GURL& url,
                             ResourceType::Type resource_type,
                             FilterPolicy::Type filter_policy,
                             const std::string& frame_origin);

  // SSLBlockingPage::Delegate methods.
  virtual SSLErrorInfo GetSSLErrorInfo(SSLCertErrorHandler* handler);
  virtual void OnDenyCertificate(SSLCertErrorHandler* handler);
  virtual void OnAllowCertificate(SSLCertErrorHandler* handler);

 private:
  // Construct via |GetDefaultPolicy|.
  SSLPolicy();
  friend struct DefaultSingletonTraits<SSLPolicy>;

  // Helper method for derived classes handling certificate errors that can be
  // overridden by the user.
  // Show a blocking page and let the user continue or cancel the request.
  void OnOverridableCertError(SSLCertErrorHandler* handler);

  // Helper method for derived classes handling fatal certificate errors.
  // Cancel the request and show an error page.
  void OnFatalCertError(SSLCertErrorHandler* handler);

  DISALLOW_COPY_AND_ASSIGN(SSLPolicy);
};

#endif  // CHROME_BROWSER_SSL_SSL_POLICY_H_
