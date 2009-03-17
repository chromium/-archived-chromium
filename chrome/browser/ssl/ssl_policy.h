// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_POLICY_H_
#define CHROME_BROWSER_SSL_POLICY_H_

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
  virtual void OnCertError(const GURL& main_frame_url,
                           SSLManager::CertError* error);
  virtual void OnMixedContent(
      NavigationController* navigation_controller,
      const GURL& main_frame_url,
      SSLManager::MixedContentHandler* mixed_content_handler);
  virtual void OnRequestStarted(SSLManager* manager,
                                const GURL& url,
                                ResourceType::Type resource_type,
                                int ssl_cert_id,
                                int ssl_cert_status);
  virtual SecurityStyle GetDefaultStyle(const GURL& url);

  // This method is static because it is called from both the UI and the IO
  // threads.
  static bool IsMixedContent(const GURL& url,
                             ResourceType::Type resource_type,
                             const std::string& main_frame_origin);

  // SSLBlockingPage::Delegate methods.
  virtual SSLErrorInfo GetSSLErrorInfo(SSLManager::CertError* error);
  virtual void OnDenyCertificate(SSLManager::CertError* error);
  virtual void OnAllowCertificate(SSLManager::CertError* error);

 protected:
  // Construct via |GetDefaultPolicy|.
  SSLPolicy();
  friend struct DefaultSingletonTraits<SSLPolicy>;

  // Helper method for derived classes handling certificate errors that can be
  // overridden by the user.
  // Show a blocking page and let the user continue or cancel the request.
  void OnOverridableCertError(const GURL& main_frame_url,
                              SSLManager::CertError* error);

  // Helper method for derived classes handling fatal certificate errors.
  // Cancel the request and show an error page.
  void OnFatalCertError(const GURL& main_frame_url,
                        SSLManager::CertError* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(SSLPolicy);
};

#endif  // CHROME_BROWSER_SSL_POLICY_H_
