// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_MANAGER_H_
#define CHROME_BROWSER_SSL_SSL_MANAGER_H_

#include <string>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/ssl/ssl_policy_backend.h"
#include "chrome/browser/tab_contents/provisional_load_details.h"
#include "chrome/browser/tab_contents/security_style.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_info.h"
#include "net/base/x509_certificate.h"
#include "webkit/api/public/WebConsoleMessage.h"
#include "webkit/glue/resource_type.h"

class AutomationProvider;
class NavigationEntry;
class LoadFromMemoryCacheDetails;
class LoadNotificationDetails;
class NavigationController;
class PrefService;
class ResourceRedirectDetails;
class ResourceRequestDetails;
class SSLCertErrorHandler;
class SSLErrorHandler;
class SSLErrorInfo;
class SSLHostState;
class SSLMixedContentHandler;
class SSLRequestInfo;
class Task;
class URLRequest;
class TabContents;

// The SSLManager SSLManager controls the SSL UI elements in a TabContents.  It
// listens for various events that influence when these elements should or
// should not be displayed and adjusts them accordingly.
//
// There is one SSLManager per tab.
// The security state (secure/insecure) is stored in the navigation entry.
// Along with it are stored any SSL error code and the associated cert.

class SSLManager : public NotificationObserver {
 public:
  // The SSLManager will ask its delegate to decide how to handle events
  // relevant to SSL.  Delegates are expected to be stateless and intended to be
  // easily implementable.
  //
  // Delegates should interact with the rest of the browser only through their
  // parameters and through the delegate API of the SSLManager.
  //
  // If a delegate needs to do something tricky, consider having the SSLManager
  // do it instead.
  class Delegate {
   public:
    // An error occurred with the certificate in an SSL connection.
    virtual void OnCertError(SSLCertErrorHandler* handler) = 0;

    // A request for a mixed-content resource was made.  Note that the resource
    // request was not started yet and the delegate is responsible for starting
    // it.
    virtual void OnMixedContent(SSLMixedContentHandler* handler) = 0;

    // We have started a resource request with the given info.
    virtual void OnRequestStarted(SSLRequestInfo* info) = 0;

    // Update the SSL information in |entry| to match the current state.
    virtual void UpdateEntry(SSLPolicyBackend* backend, NavigationEntry* entry) = 0;
  };

  static void RegisterUserPrefs(PrefService* prefs);

  // Construct an SSLManager for the specified tab.
  // If |delegate| is NULL, SSLPolicy::GetDefaultPolicy() is used.
  SSLManager(NavigationController* controller, Delegate* delegate);

  ~SSLManager();

  // The delegate of the SSLManager.  This value may be changed at any time,
  // but it is not permissible for it to be NULL.
  Delegate* delegate() const { return delegate_; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  SSLPolicyBackend* backend() { return &backend_; }

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

  // Called by SSLCertErrorHandler::OnDispatch to kick off processing of the
  // cert error by the SSL manager.  The error originated from the
  // ResourceDispatcherHost.
  //
  // Called on the UI thread.
  void OnCertError(SSLCertErrorHandler* handler);

  // Called by SSLMixedContentHandler::OnDispatch to kick off processing of the
  // mixed-content resource request.  The info originated from the
  // ResourceDispatcherHost.
  //
  // Called on the UI thread.
  void OnMixedContent(SSLMixedContentHandler* handler);

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

  // The navigation controller associated with this SSLManager.  The
  // NavigationController is guaranteed to outlive the SSLManager.
  NavigationController* controller() { return controller_; }

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

  // Shows the pending messages (in info-bars) if any.
  void ShowPendingMessages();

  // Clears any pending messages.
  void ClearPendingMessages();

  // Our delegate.  The delegate is responsible for making policy decisions.
  // Must not be NULL.
  Delegate* delegate_;

  // The backend for the SSLPolicy to actuate its decisions.
  SSLPolicyBackend backend_;

  // The NavigationController that owns this SSLManager.  We are responsible
  // for the security UI of this tab.
  NavigationController* controller_;

  // Handles registering notifications with the NotificationService.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SSLManager);
};

#endif  // CHROME_BROWSER_SSL_SSL_MANAGER_H_
