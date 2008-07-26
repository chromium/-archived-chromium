// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_SSL_POLICY_H__
#define CHROME_BROWSER_SSL_POLICY_H__

#include "chrome/browser/ssl_blocking_page.h"
#include "chrome/browser/ssl_manager.h"

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

 private:
  DISALLOW_EVIL_CONSTRUCTORS(SSLPolicy);
};

#endif // CHROME_BROWSER_SSL_POLICY_H__
