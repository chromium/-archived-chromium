// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_MANAGER_H_
#define CHROME_BROWSER_SSL_SSL_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/ssl/ssl_policy.h"
#include "chrome/browser/ssl/ssl_policy_backend.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"

class LoadFromMemoryCacheDetails;
class MessageLoop;
class NavigationController;
class NavigationEntry;
class PrefService;
class ProvisionalLoadDetails;
class ResourceDispatcherHost;
class ResourceRedirectDetails;
class ResourceRequestDetails;
class SSLCertErrorHandler;
class SSLPolicy;
class Task;
class URLRequest;

// The SSLManager SSLManager controls the SSL UI elements in a TabContents.  It
// listens for various events that influence when these elements should or
// should not be displayed and adjusts them accordingly.
//
// There is one SSLManager per tab.
// The security state (secure/insecure) is stored in the navigation entry.
// Along with it are stored any SSL error code and the associated cert.

class SSLManager : public NotificationObserver {
 public:
  // Construct an SSLManager for the specified tab.
  // If |delegate| is NULL, SSLPolicy::GetDefaultPolicy() is used.
  explicit SSLManager(NavigationController* controller);
  ~SSLManager();

  SSLPolicy* policy() { return policy_.get(); }
  SSLPolicyBackend* backend() { return &backend_; }

  // The navigation controller associated with this SSLManager.  The
  // NavigationController is guaranteed to outlive the SSLManager.
  NavigationController* controller() { return controller_; }

  static void RegisterUserPrefs(PrefService* prefs);

  // Entry point for SSLCertificateErrors.  This function begins the process
  // of resolving a certificate error during an SSL connection.  SSLManager
  // will adjust the security UI and either call |Cancel| or
  // |ContinueDespiteLastError| on the URLRequest.
  //
  // Called on the IO thread.
  static void OnSSLCertificateError(ResourceDispatcherHost* resource_dispatcher,
                                    URLRequest* request,
                                    int cert_error,
                                    net::X509Certificate* cert,
                                    MessageLoop* ui_loop);

  // Called before a URL request is about to be started.  Returns false if the
  // resource request should be delayed while we figure out what to do.  We use
  // this function as the entry point for our mixed content detection.
  //
  // TODO(jcampan): Implement a way to just cancel the request.  This is not
  // straight-forward as canceling a request that has not been started will
  // not remove from the pending_requests_ of the ResourceDispatcherHost.
  // Called on the IO thread.
  static bool ShouldStartRequest(ResourceDispatcherHost* resource_dispatcher,
                                 URLRequest* request,
                                 MessageLoop* ui_loop);

  // Entry point for navigation.  This function begins the process of updating
  // the security UI when the main frame navigates to a new URL.
  //
  // Called on the UI thread.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // This entry point is called directly (instead of via the notification
  // service) because we need more precise control of the order in which folks
  // are notified of this event.
  void DidCommitProvisionalLoad(const NotificationDetails& details);

  // Called to determine if there were any processed SSL errors from request.
  bool ProcessedSSLErrorFromRequest() const;

  // Convenience methods for serializing/deserializing the security info.
  static std::string SerializeSecurityInfo(int cert_id,
                                           int cert_status,
                                           int security_bits);
  static bool DeserializeSecurityInfo(const std::string& state,
                                      int* cert_id,
                                      int* cert_status,
                                      int* security_bits);

  // Sets |short_name| to <organization_name> [<country>] and |ca_name|
  // to something like:
  // "Verified by <issuer_organization_name>"
  static bool GetEVCertNames(const net::X509Certificate& cert,
                             std::wstring* short_name,
                             std::wstring* ca_name);

 private:
  // SSLMessageInfo contains the information necessary for displaying a message
  // in an info-bar.
  struct SSLMessageInfo {
   public:
    explicit SSLMessageInfo(const std::wstring& text)
        : message(text),
          action(NULL) { }

    SSLMessageInfo(const std::wstring& message,
                   const std::wstring& link_text,
                   Task* action)
        : message(message), link_text(link_text), action(action) { }

    // Overridden so that std::find works.
    bool operator==(const std::wstring& other_message) const {
      // We are uniquing SSLMessageInfo by their message only.
      return message == other_message;
    }

    std::wstring message;
    std::wstring link_text;
    Task* action;
  };

  // Entry points for notifications to which we subscribe. Note that
  // DidCommitProvisionalLoad uses the abstract NotificationDetails type since
  // the type we need is in NavigationController which would create a circular
  // header file dependency.
  void DidLoadFromMemoryCache(LoadFromMemoryCacheDetails* details);
  void DidFailProvisionalLoadWithError(ProvisionalLoadDetails* details);
  void DidStartResourceResponse(ResourceRequestDetails* details);
  void DidReceiveResourceRedirect(ResourceRedirectDetails* details);
  void DidChangeSSLInternalState();

  // Dispatch NotificationType::SSL_VISIBLE_STATE_CHANGED notification.
  void DispatchSSLVisibleStateChanged();

  // Update the NavigationEntry with our current state.
  void UpdateEntry(NavigationEntry* entry);

  // The backend for the SSLPolicy to actuate its decisions.
  SSLPolicyBackend backend_;

  // The SSLPolicy instance for this manager.
  scoped_ptr<SSLPolicy> policy_;

  // The NavigationController that owns this SSLManager.  We are responsible
  // for the security UI of this tab.
  NavigationController* controller_;

  // Handles registering notifications with the NotificationService.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SSLManager);
};

#endif  // CHROME_BROWSER_SSL_SSL_MANAGER_H_
