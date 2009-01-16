// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_POLICY_H__
#define CHROME_BROWSER_SSL_POLICY_H__

#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_manager.h"

// The basic SSLPolicy.  This class contains default implementations of all
// the SSLPolicy entry points.  It is expected that subclasses will override
// most of these methods to implement policy specific to certain errors or
// situations.
class SSLPolicy : public SSLManager::Delegate,
                  public SSLBlockingPage::Delegate {
 public:
  // Factory method to get the default policy.
  //
  // SSLPolicy is not meant to be instantiated itself.  Only subclasses should
  // be instantiated.  The default policy has more complex behavior than a
  // direct instance of SSLPolicy.
  static SSLPolicy* GetDefaultPolicy();

  // SSLManager::Delegate methods.
  virtual void OnCertError(const GURL& main_frame_url,
                           SSLManager::CertError* error);
  virtual void OnMixedContent(
      NavigationController* navigation_controller,
      const GURL& main_frame_url,
      SSLManager::MixedContentHandler* mixed_content_handler) {
    // So far only the default policy is expected to receive mixed-content
    // calls.
    NOTREACHED();
  }

  virtual void OnRequestStarted(SSLManager* manager,
                                const GURL& url,
                                ResourceType::Type resource_type,
                                int ssl_cert_id,
                                int ssl_cert_status);
  virtual SecurityStyle GetDefaultStyle(const GURL& url);

  // SSLBlockingPage::Delegate methods.
  virtual SSLErrorInfo GetSSLErrorInfo(SSLManager::CertError* error);
  virtual void OnDenyCertificate(SSLManager::CertError* error);
  virtual void OnAllowCertificate(SSLManager::CertError* error);

 protected:
  // Allow our subclasses to construct us.
  SSLPolicy();

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
  DISALLOW_EVIL_CONSTRUCTORS(SSLPolicy);
};

#endif  // CHROME_BROWSER_SSL_POLICY_H__

