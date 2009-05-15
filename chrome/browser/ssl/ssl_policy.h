// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_POLICY_H_
#define CHROME_BROWSER_SSL_SSL_POLICY_H_

#include <string>

#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/common/filter_policy.h"
#include "webkit/glue/resource_type.h"

class NavigationEntry;
class SSLCertErrorHandler;
class SSLMixedContentHandler;
class SSLPolicyBackend;
class SSLRequestInfo;

// SSLPolicy
//
// This class is responsible for making the security decisions that concern the
// SSL trust indicators.  It relies on the SSLPolicyBackend to actually enact
// the decisions it reaches.
//
class SSLPolicy : public SSLBlockingPage::Delegate {
 public:
  explicit SSLPolicy(SSLPolicyBackend* backend);

  // An error occurred with the certificate in an SSL connection.
  void OnCertError(SSLCertErrorHandler* handler);

  // A request for a mixed-content resource was made.  Note that the resource
  // request was not started yet and the delegate is responsible for starting
  // it.
  void OnMixedContent(SSLMixedContentHandler* handler);

  // We have started a resource request with the given info.
  void OnRequestStarted(SSLRequestInfo* info);

  // Update the SSL information in |entry| to match the current state.
  void UpdateEntry(NavigationEntry* entry);

  // This method is static because it is called from both the UI and the IO
  // threads.
  static bool IsMixedContent(const GURL& url,
                             ResourceType::Type resource_type,
                             FilterPolicy::Type filter_policy,
                             const std::string& frame_origin);

  SSLPolicyBackend* backend() const { return backend_; }

  // SSLBlockingPage::Delegate methods.
  virtual SSLErrorInfo GetSSLErrorInfo(SSLCertErrorHandler* handler);
  virtual void OnDenyCertificate(SSLCertErrorHandler* handler);
  virtual void OnAllowCertificate(SSLCertErrorHandler* handler);

 private:
  class ShowMixedContentTask;

  // Helper method for derived classes handling certificate errors that can be
  // overridden by the user.
  // Show a blocking page and let the user continue or cancel the request.
  void OnOverridableCertError(SSLCertErrorHandler* handler);

  // Helper method for derived classes handling fatal certificate errors.
  // Cancel the request and show an error page.
  void OnFatalCertError(SSLCertErrorHandler* handler);

  // Show an error page for this certificate error.  This error page does not
  // give the user the opportunity to ingore the error.
  void ShowErrorPage(SSLCertErrorHandler* handler);

  // Add a warning about mixed content to the JavaScript console.  This warning
  // helps web developers track down and eliminate mixed content on their site.
  void AddMixedContentWarningToConsole(SSLMixedContentHandler* handler);

  // If the security style of |entry| has not been initialized, then initialize
  // it with the default style for its URL.
  void InitializeEntryIfNeeded(NavigationEntry* entry);

  // Mark |origin| as containing insecure content in the process with ID |pid|.
  void MarkOriginAsBroken(const std::string& origin, int pid);

  // Allow |origin| to include mixed content.  This stops us from showing an
  // infobar warning after the user as approved mixed content.
  void AllowMixedContentForOrigin(const std::string& origin);

  // Called after we've decided that |info| represents a request for mixed
  // content.  Updates our internal state to reflect that we've loaded |info|.
  void UpdateStateForMixedContent(SSLRequestInfo* info);

  // Called after we've decided that |info| represents a request for unsafe
  // content.  Updates our internal state to reflect that we've loaded |info|.
  void UpdateStateForUnsafeContent(SSLRequestInfo* info);

  // The backend we use to enact our decisions.
  SSLPolicyBackend* backend_;

  DISALLOW_COPY_AND_ASSIGN(SSLPolicy);
};

#endif  // CHROME_BROWSER_SSL_SSL_POLICY_H_
