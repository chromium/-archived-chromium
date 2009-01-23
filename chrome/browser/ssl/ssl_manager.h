// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_MANAGER_H_
#define CHROME_BROWSER_SSL_MANAGER_H_

#include <string>
#include <map>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/provisional_load_details.h"
#include "chrome/browser/tab_contents/security_style.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_info.h"
#include "net/base/x509_certificate.h"
#include "webkit/glue/console_message_level.h"
#include "webkit/glue/resource_type.h"

class AutomationProvider;
class NavigationEntry;
class LoadFromMemoryCacheDetails;
class LoadNotificationDetails;
class NavigationController;
class PrefService;
class ResourceRedirectDetails;
class ResourceRequestDetails;
class SSLErrorInfo;
class Task;
class URLRequest;
class WebContents;

// The SSLManager SSLManager controls the SSL UI elements in a TabContents.  It
// listens for various events that influence when these elements should or
// should not be displayed and adjusts them accordingly.
//
// There is one SSLManager per tab.
// The security state (secure/insecure) is stored in the navigation entry.
// Along with it are stored any SSL error code and the associated cert.
//

class SSLManager : public NotificationObserver {
 public:
  // An ErrorHandler carries information from the IO thread to the UI thread
  // and is dispatched to the appropriate SSLManager when it arrives on the
  // UI thread.  Subclasses should override the OnDispatched/OnDispatchFailed
  // methods to implement the actions that should be taken on the UI thread.
  // These methods can call the different convenience methods ContinueRequest/
  // CancelRequest/StartRequest to perform any required action on the URLRequest
  // the ErrorHandler was created with.
  // IMPORTANT NOTE: if you are not doing anything in
  // OnDispatched/OnDispatchFailed, make sure you call TakeNoAction().  This is
  // necessary for ensuring the instance is not leaked.
  class ErrorHandler : public base::RefCountedThreadSafe<ErrorHandler> {
   public:
     virtual ~ErrorHandler() { }

    // Find the appropriate SSLManager for the URLRequest and begin handling
    // this error.
    //
    // Call on UI thread.
    void Dispatch();

    // Available on either thread.
    const GURL& request_url() const { return request_url_; }

    // Call on the UI thread.
    SSLManager* manager() const { return manager_; };

    // Returns the WebContents this object is associated with.  Should be
    // called from the UI thread.
    WebContents* GetWebContents();

    // Cancels the associated URLRequest.
    // This method can be called from OnDispatchFailed and OnDispatched.
    void CancelRequest();

    // Continue the URLRequest ignoring any previous errors.  Note that some
    // errors cannot be ignored, in which case this will result in the request
    // being canceled.
    // This method can be called from OnDispatchFailed and OnDispatched.
    void ContinueRequest();

    // Cancels the associated URLRequest and mark it as denied.  The renderer
    // processes such request in a special manner, optionally replacing them
    // with alternate content (typically frames content is replaced with a
    // warning message).
    // This method can be called from OnDispatchFailed and OnDispatched.
    void DenyRequest();

    // Starts the associated URLRequest.  |filter_policy| specifies whether the
    // ResourceDispatcher should attempt to filter the loaded content in order
    // to make it secure (ex: images are made slightly transparent and are
    // stamped).
    // Should only be called when the URLRequest has not already been started.
    // This method can be called from OnDispatchFailed and OnDispatched.
    void StartRequest(FilterPolicy::Type filter_policy);

    // Does nothing on the URLRequest but ensures the current instance ref
    // count is decremented appropriately.  Subclasses that do not want to
    // take any specific actions in their OnDispatched/OnDispatchFailed should
    // call this.
    void TakeNoAction();

   protected:
    // Construct on the IO thread.
    ErrorHandler(ResourceDispatcherHost* resource_dispatcher_host,
                 URLRequest* request,
                 MessageLoop* ui_loop);

    // The following 2 methods are the methods subclasses should implement.
    virtual void OnDispatchFailed() { TakeNoAction(); }

    // Can use the manager_ member.
    virtual void OnDispatched() { TakeNoAction(); }

    // We cache the message loops to be able to proxy events across the thread
    // boundaries.
    MessageLoop* ui_loop_;
    MessageLoop* io_loop_;

    // Should only be accessed on the UI thread.
    SSLManager* manager_;  // Our manager.

    // The id of the URLRequest associated with this object.
    // Should only be accessed from the IO thread.
    ResourceDispatcherHost::GlobalRequestID request_id_;

    // The ResourceDispatcherHost we are associated with.
    ResourceDispatcherHost* resource_dispatcher_host_;

   private:
    // Completes the CancelRequest operation on the IO thread.
    // Call on the IO thread.
    void CompleteCancelRequest(int error);

    // Completes the ContinueRequest operation on the IO thread.
    //
    // Call on the IO thread.
    void CompleteContinueRequest();

    // Completes the StartRequest operation on the IO thread.
    // Call on the IO thread.
    void CompleteStartRequest(FilterPolicy::Type filter_policy);

    // Derefs this instance.
    // Call on the IO thread.
    void CompleteTakeNoAction();

    // We use these members to find the correct SSLManager when we arrive on
    // the UI thread.
    int render_process_host_id_;
    int tab_contents_id_;

    // This read-only member can be accessed on any thread.
    const GURL request_url_;  // The URL that we requested.

    // Should only be accessed on the IO thread
    bool request_has_been_notified_; // A flag to make sure we notify the
                                     // URLRequest exactly once.

    DISALLOW_EVIL_CONSTRUCTORS(ErrorHandler);
  };

  // A CertError represents an error that occurred with the certificate in an
  // SSL session.  A CertError object exists both on the IO thread and on the UI
  // thread and allows us to cancel/continue a request it is associated with.
  class CertError : public ErrorHandler {
   public:
    // These accessors are available on either thread
    const net::SSLInfo& ssl_info() const { return ssl_info_; }
    int cert_error() const { return cert_error_; }

    ResourceType::Type resource_type() const { return resource_type_; }
   private:
    // SSLManager is responsible for creating CertError objects.
    friend class SSLManager;

    // Construct on the IO thread.
    // We mark this method as private because it is tricky to correctly
    // construct a CertError object.
    CertError(ResourceDispatcherHost* resource_dispatcher_host,
              URLRequest* request,
              ResourceType::Type resource_type,
              int cert_error,
              net::X509Certificate* cert,
              MessageLoop* ui_loop);

    // ErrorHandler methods
    virtual void OnDispatchFailed() { CancelRequest(); }
    virtual void OnDispatched() { manager_->OnCertError(this); }

    // These read-only members can be accessed on any thread.
    net::SSLInfo ssl_info_;
    const int cert_error_; // The error we represent.

    // What kind of resource is associated with the requested that generated
    // that error.
    ResourceType::Type resource_type_;

    DISALLOW_EVIL_CONSTRUCTORS(CertError);
  };

  // The MixedContentHandler class is used to query what to do with
  // mixed content, from the IO thread to the UI thread.
  class MixedContentHandler : public ErrorHandler {
   public:
    // Created on the IO thread.
    MixedContentHandler(ResourceDispatcherHost* rdh,
                        URLRequest* request,
                        MessageLoop* ui_loop)
        : ErrorHandler(rdh, request, ui_loop) { }

   protected:
    virtual void OnDispatchFailed() { TakeNoAction(); }
    virtual void OnDispatched() { manager()->OnMixedContent(this); }

   private:
    DISALLOW_EVIL_CONSTRUCTORS(MixedContentHandler);
  };

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
    virtual void OnCertError(const GURL& main_frame_url, CertError* error) = 0;

    // A request for a mixed-content resource was made.  Note that the resource
    // request was not started yet and the delegate is responsible for starting
    // it.
    virtual void OnMixedContent(
        NavigationController* navigation_controller,
        const GURL& main_frame_url,
        MixedContentHandler* mixed_content_handler) = 0;

    // We have started a resource request for the given URL.
    virtual void OnRequestStarted(SSLManager* manager,
                                  const GURL& url,
                                  ResourceType::Type resource_type,
                                  int ssl_cert_id,
                                  int ssl_cert_status) = 0;

    // Returns the default security style for a given URL.
    virtual SecurityStyle GetDefaultStyle(const GURL& url) = 0;
  };

  static void RegisterUserPrefs(PrefService* prefs);

  // Construct an SSLManager for the specified tab.
  // If |delegate| is NULL, SSLPolicy::GetDefaultPolicy() is used.
  SSLManager(NavigationController* controller, Delegate* delegate);

  ~SSLManager();

  //////////////////////////////////////////////////////////////////////////////
  // Delegate API
  //
  // The SSL manager expects these methods to be called by its delegate.  They
  // exist to make Delegates easy to implement.

  // Ensure that the specified message is displayed to the user.  This will
  // display an InfoBar at the top of the associated tab.
  void ShowMessage(const std::wstring& msg);

  // Same as ShowMessage but also contains a link that when clicked run the
  // specified task.  The SSL Manager becomes the owner of the task.
  void ShowMessageWithLink(const std::wstring& msg,
                           const std::wstring& link_text,
                           Task* task);

  // Sets the maximum security style for the page.  If the current security
  // style is lower than |style|, this will not have an effect on the security
  // indicators.
  //
  // It will return true if the navigation entry was updated or false if
  // nothing changed. The caller is responsible for broadcasting
  // NOTIFY_SSY_STATE_CHANGED if it returns true.
  bool SetMaxSecurityStyle(SecurityStyle style);

  // Logs a message to the console of the page.
  void AddMessageToConsole(const std::wstring& msg,
                           ConsoleMessageLevel level);

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

  //
  //////////////////////////////////////////////////////////////////////////////

  // The delegate of the SSLManager.  This value may be changed at any time,
  // but it is not permissible for it to be NULL.
  Delegate* delegate() const { return delegate_; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

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

  // Called when a mixed-content sub-resource request has been detected.  The
  // request is not started yet.  The SSLManager will make a decision on whether
  // to filter that request's content (with the filter_policy flag).
  // TODO (jcampan): Implement a way to just cancel the request.  This is not
  // straight-forward as canceling a request that has not been started will
  // not remove from the pending_requests_ of the ResourceDispatcherHost.
  // Called on the IO thread.
  static void OnMixedContentRequest(ResourceDispatcherHost* resource_dispatcher,
                                    URLRequest* request,
                                    MessageLoop* ui_loop);

  // Called by CertError::Dispatch to kick off processing of the cert error by
  // the SSL manager.  The error originated from the ResourceDispatcherHost.
  //
  // Called on the UI thread.
  void OnCertError(CertError* error);

  // Called by MixedContentHandler::Dispatch to kick off processing of the
  // mixed-content resource request.  The info originated from the
  // ResourceDispatcherHost.
  //
  // Called on the UI thread.
  void OnMixedContent(MixedContentHandler* mixed_content);

  // Entry point for navigation.  This function begins the process of updating
  // the security UI when the main frame navigates to a new URL.
  //
  // Called on the UI thread.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Entry point for navigation.  This function begins the process of updating
  // the security UI when the main frame navigates.
  //
  // Called on the UI thread.
  void NavigationStateChanged();

  // Called to determine if there were any processed SSL errors from request.
  bool ProcessedSSLErrorFromRequest() const;

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
  void DidCommitProvisionalLoad(const NotificationDetails& details);
  void DidFailProvisionalLoadWithError(ProvisionalLoadDetails* details);
  void DidStartResourceResponse(ResourceRequestDetails* details);
  void DidReceiveResourceRedirect(ResourceRedirectDetails* details);

  // Convenience method for initializing navigation entries.
  void InitializeEntryIfNeeded(NavigationEntry* entry);

  // Shows the pending messages (in info-bars) if any.
  void ShowPendingMessages();

  // Clears any pending messages.
  void ClearPendingMessages();

  // Our delegate.  The delegate is responsible for making policy decisions.
  // Must not be NULL.
  Delegate* delegate_;

  // The NavigationController that owns this SSLManager.  We are responsible
  // for the security UI of this tab.
  NavigationController* controller_;

  // Handles registering notifications with the NotificationService.
  NotificationRegistrar registrar_;

  // Certificate policies for each host.
  std::map<std::string, net::X509Certificate::Policy> cert_policy_for_host_;

  // Domains for which it is OK to show insecure content.
  std::set<std::string> can_show_insecure_content_for_host_;

  // The list of messages that should be displayed (in info bars) when the page
  // currently loading had loaded.
  std::vector<SSLMessageInfo> pending_messages_;

  DISALLOW_COPY_AND_ASSIGN(SSLManager);
};

#endif // CHROME_BROWSER_SSL_MANAGER_H_

