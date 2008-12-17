// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl_manager.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/provisional_load_details.h"
#include "chrome/browser/infobar_delegate.h"
#include "chrome/browser/load_notification_details.h"
#include "chrome/browser/load_from_memory_cache_details.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/resource_request_details.h"
#include "chrome/browser/ssl_error_info.h"
#include "chrome/browser/ssl_policy.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_util.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/decision.h"
#include "chrome/views/link.h"
#include "net/base/cert_status_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_type.h"
#include "generated_resources.h"

class SSLInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
   SSLInfoBarDelegate(TabContents* contents,
                      const std::wstring message,
                      const std::wstring& button_label,
                      Task* task)
      : ConfirmInfoBarDelegate(contents),
        message_(message),
        button_label_(button_label),
        task_(task) {
  }
  virtual ~SSLInfoBarDelegate() {}

  // Overridden from ConfirmInfoBarDelegate:
  virtual void InfoBarClosed() {
    delete this;
  }
  virtual std::wstring GetMessageText() const {
    return message_;
  }
  virtual SkBitmap* GetIcon() const {
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_INFOBAR_SSL_WARNING);
  }
  virtual int GetButtons() const {
    return !button_label_.empty() ? BUTTON_OK : BUTTON_NONE;
  }
  virtual std::wstring GetButtonLabel(InfoBarButton button) const {
    return button_label_;
  }
  virtual bool Accept() {
    if (task_.get()) {
      task_->Run();
      task_.reset();  // Ensures we won't run the task again.
    }
    return true;
  }

 private:
  // Labels for the InfoBar's message and button.
  std::wstring message_;
  std::wstring button_label_;

  // A task to run when the InfoBar is accepted.
  scoped_ptr<Task> task_;

  DISALLOW_COPY_AND_ASSIGN(SSLInfoBarDelegate);
};

////////////////////////////////////////////////////////////////////////////////
// SSLManager

// static
void SSLManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kMixedContentFiltering,
                             FilterPolicy::DONT_FILTER);
}

SSLManager::SSLManager(NavigationController* controller, Delegate* delegate)
    : controller_(controller),
      delegate_(delegate) {
  DCHECK(controller_);

  // If do delegate is supplied, use the default policy.
  if (!delegate_)
    delegate_ = SSLPolicy::GetDefaultPolicy();

  // Subscribe to various notifications.
  registrar_.Add(this, NOTIFY_NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, NOTIFY_FAIL_PROVISIONAL_LOAD_WITH_ERROR,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, NOTIFY_RESOURCE_RESPONSE_STARTED,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, NOTIFY_RESOURCE_RECEIVED_REDIRECT,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, NOTIFY_LOAD_FROM_MEMORY_CACHE,
                 Source<NavigationController>(controller_));
}

SSLManager::~SSLManager() {
}

// Delegate API method.
void SSLManager::ShowMessage(const std::wstring& msg) {
  ShowMessageWithLink(msg, std::wstring(), NULL);
}

void SSLManager::ShowMessageWithLink(const std::wstring& msg,
                                     const std::wstring& link_text,
                                     Task* task) {
  if (controller_->GetPendingEntry()) {
    // The main frame is currently loading, wait until the load is committed so
    // to show the error on the right page (once the location bar shows the
    // correct url).
    if (std::find(pending_messages_.begin(), pending_messages_.end(), msg) ==
        pending_messages_.end())
      pending_messages_.push_back(SSLMessageInfo(msg, link_text, task));

    return;
  }

  NavigationEntry* entry = controller_->GetActiveEntry();
  if (!entry)
    return;

  // Don't show the message if the user doesn't expect an authenticated session.
  if (entry->ssl().security_style() <= SECURITY_STYLE_UNAUTHENTICATED)
    return;

  if (controller_->active_contents()) {
    controller_->active_contents()->AddInfoBar(
        new SSLInfoBarDelegate(controller_->active_contents(), msg, link_text,
                               task));
  }
}

// Delegate API method.
bool SSLManager::SetMaxSecurityStyle(SecurityStyle style) {
  NavigationEntry* entry = controller_->GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return false;
  }

  if (entry->ssl().security_style() > style) {
    entry->ssl().set_security_style(style);
    return true;
  }
  return false;
}

// Delegate API method.
void SSLManager::AddMessageToConsole(const std::wstring& msg,
                                     ConsoleMessageLevel level) {
  TabContents* tab_contents = controller_->GetTabContents(TAB_CONTENTS_WEB);
  if (!tab_contents)
    return;
  WebContents* web_contents = tab_contents->AsWebContents();
  if (!web_contents)
    return;

  web_contents->render_view_host()->AddMessageToConsole(
      std::wstring(), msg, level);
}

// Delegate API method.
void SSLManager::DenyCertForHost(net::X509Certificate* cert,
                                 const std::string& host) {
  // Remember that we don't like this cert for this host.
  // TODO(abarth): Do we want to persist this information in the user's profile?
  cert_policy_for_host_[host].Deny(cert);
}

// Delegate API method.
void SSLManager::AllowCertForHost(net::X509Certificate* cert,
                                  const std::string& host) {
  // Remember that we do like this cert for this host.
  // TODO(abarth): Do we want to persist this information in the user's profile?
  cert_policy_for_host_[host].Allow(cert);
}

// Delegate API method.
net::X509Certificate::Policy::Judgment SSLManager::QueryPolicy(
    net::X509Certificate* cert, const std::string& host) {
  // TODO(abarth): Do we want to read this information from the user's profile?
  return cert_policy_for_host_[host].Check(cert);
}

bool SSLManager::CanShowInsecureContent(const GURL& url) {
  // TODO(jcampan): Do we want to read this information from the user's profile?
  return (can_show_insecure_content_for_host_.find(url.host()) !=
      can_show_insecure_content_for_host_.end());
}

void SSLManager::AllowShowInsecureContentForURL(const GURL& url) {
  can_show_insecure_content_for_host_.insert(url.host());
}

bool SSLManager::ProcessedSSLErrorFromRequest() const {
  NavigationEntry* entry = controller_->GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return false;
  }

  return net::IsCertStatusError(entry->ssl().cert_status());
}

////////////////////////////////////////////////////////////////////////////////
// ErrorHandler

SSLManager::ErrorHandler::ErrorHandler(ResourceDispatcherHost* rdh,
                                       URLRequest* request,
                                       MessageLoop* ui_loop)
    : ui_loop_(ui_loop),
      io_loop_(MessageLoop::current()),
      manager_(NULL),
      resource_dispatcher_host_(rdh),
      request_has_been_notified_(false),
      request_id_(0, 0),
      request_url_(request->url()) {
  DCHECK(MessageLoop::current() != ui_loop);

  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request);
  request_id_.render_process_host_id = info->render_process_host_id;
  request_id_.request_id = info->request_id;

  if (!tab_util::GetTabContentsID(request,
                                  &render_process_host_id_,
                                  &tab_contents_id_))
    NOTREACHED();

  // This makes sure we don't disappear on the IO thread until we've given an
  // answer to the URLRequest.
  //
  // Release in CompleteCancelRequest, CompleteContinueRequest,
  // CompleteStartRequest or CompleteTakeNoAction.
  AddRef();
}

void SSLManager::ErrorHandler::Dispatch() {
  DCHECK(MessageLoop::current() == ui_loop_);

  TabContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id_, tab_contents_id_);

  if (!web_contents) {
    // We arrived on the UI thread, but the tab we're looking for is no longer
    // here.
    OnDispatchFailed();
    return;
  }

  // Hand ourselves off to the SSLManager.
  manager_ = web_contents->controller()->ssl_manager();
  OnDispatched();
}

WebContents* SSLManager::ErrorHandler::GetWebContents() {
  return tab_util::GetWebContentsByID(render_process_host_id_,
                                      tab_contents_id_);
}

void SSLManager::ErrorHandler::CancelRequest() {
  DCHECK(MessageLoop::current() == ui_loop_);

  // We need to complete this task on the IO thread.
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
       this, &SSLManager::ErrorHandler::CompleteCancelRequest,
       net::ERR_ABORTED));
}

void SSLManager::ErrorHandler::DenyRequest() {
  DCHECK(MessageLoop::current() == ui_loop_);

  // We need to complete this task on the IO thread.
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SSLManager::ErrorHandler::CompleteCancelRequest,
      net::ERR_INSECURE_RESPONSE));
}

void SSLManager::ErrorHandler::ContinueRequest() {
  DCHECK(MessageLoop::current() == ui_loop_);

  // We need to complete this task on the IO thread.
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SSLManager::ErrorHandler::CompleteContinueRequest));
}

void SSLManager::ErrorHandler::StartRequest(FilterPolicy::Type filter_policy) {
  DCHECK(MessageLoop::current() == ui_loop_);

  // We need to complete this task on the IO thread.
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SSLManager::ErrorHandler::CompleteStartRequest, filter_policy));
}

void SSLManager::ErrorHandler::TakeNoAction() {
  DCHECK(MessageLoop::current() == ui_loop_);

  // We need to complete this task on the IO thread.
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SSLManager::ErrorHandler::CompleteTakeNoAction));
}

void SSLManager::ErrorHandler::CompleteCancelRequest(int error) {
  DCHECK(MessageLoop::current() == io_loop_);

  // It is important that we notify the URLRequest only once.  If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);

  if (!request_has_been_notified_) {
    URLRequest* request = resource_dispatcher_host_->GetURLRequest(request_id_);
    if (request) {
      // The request can be NULL if it was cancelled by the renderer (as the
      // result of the user navigating to a new page from the location bar).
      DLOG(INFO) << "CompleteCancelRequest() url: " << request->url().spec();
      request->CancelWithError(error);
    }
    request_has_been_notified_ = true;

    // We're done with this object on the IO thread.
    Release();
  }
}

void SSLManager::ErrorHandler::CompleteContinueRequest() {
  DCHECK(MessageLoop::current() == io_loop_);

  // It is important that we notify the URLRequest only once.  If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);

  if (!request_has_been_notified_) {
    URLRequest* request = resource_dispatcher_host_->GetURLRequest(request_id_);
    if (request) {
      // The request can be NULL if it was cancelled by the renderer (as the
      // result of the user navigating to a new page from the location bar).
      DLOG(INFO) << "CompleteContinueRequest() url: " << request->url().spec();
      request->ContinueDespiteLastError();
    }
    request_has_been_notified_ = true;

    // We're done with this object on the IO thread.
    Release();
  }
}

void SSLManager::ErrorHandler::CompleteStartRequest(
    FilterPolicy::Type filter_policy) {
  DCHECK(MessageLoop::current() == io_loop_);

  // It is important that we notify the URLRequest only once.  If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);

  if (request_has_been_notified_)
    return;

  URLRequest* request = resource_dispatcher_host_->GetURLRequest(request_id_);
  if (request) {
    // The request can be NULL if it was cancelled by the renderer (as the
    // result of the user navigating to a new page from the location bar).
    DLOG(INFO) << "CompleteStartRequest() url: " << request->url().spec();
    // The request should not have been started (SUCCESS is the initial state).
    DCHECK(request->status().status() == URLRequestStatus::SUCCESS);
    ResourceDispatcherHost::ExtraRequestInfo* info =
        ResourceDispatcherHost::ExtraInfoForRequest(request);
    info->filter_policy = filter_policy;
    request->Start();
  }
  request_has_been_notified_ = true;

  // We're done with this object on the IO thread.
  Release();
}

void SSLManager::ErrorHandler::CompleteTakeNoAction() {
  DCHECK(MessageLoop::current() == io_loop_);

  // It is important that we notify the URLRequest only once.  If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);

  if (!request_has_been_notified_) {
    request_has_been_notified_ = true;

    // We're done with this object on the IO thread.
    Release();
  }
}


////////////////////////////////////////////////////////////////////////////////
// CertError

SSLManager::CertError::CertError(
    ResourceDispatcherHost* rdh,
    URLRequest* request,
    ResourceType::Type resource_type,
    int cert_error,
    net::X509Certificate* cert,
    MessageLoop* ui_loop)
    : ErrorHandler(rdh, request, ui_loop),
      cert_error_(cert_error),
      resource_type_(resource_type) {
  DCHECK(request == resource_dispatcher_host_->GetURLRequest(request_id_));

  // We cannot use the request->ssl_info(), it's not been initialized yet, so
  // we have to set the fields manually.
  ssl_info_.cert = cert;
  ssl_info_.SetCertError(cert_error);
}

// static
void SSLManager::OnSSLCertificateError(ResourceDispatcherHost* rdh,
                                       URLRequest* request,
                                       int cert_error,
                                       net::X509Certificate* cert,
                                       MessageLoop* ui_loop) {
  DLOG(INFO) << "OnSSLCertificateError() cert_error: " << cert_error <<
                " url: " << request->url().spec();

  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request);
  DCHECK(info);

  // A certificate error occurred.  Construct a CertError object and hand it
  // over to the UI thread for processing.
  ui_loop->PostTask(FROM_HERE,
      NewRunnableMethod(new CertError(rdh, request, info->resource_type,
                                      cert_error, cert, ui_loop),
                        &CertError::Dispatch));
}

// static
void SSLManager::OnMixedContentRequest(ResourceDispatcherHost* rdh,
                                       URLRequest* request,
                                       MessageLoop* ui_loop) {
  ui_loop->PostTask(FROM_HERE,
      NewRunnableMethod(new MixedContentHandler(rdh, request, ui_loop),
                        &MixedContentHandler::Dispatch));
}

void SSLManager::OnCertError(CertError* error) {
  // Ask our delegate to deal with the error.
  NavigationEntry* entry = controller_->GetActiveEntry();
  delegate()->OnCertError(entry->url(), error);
}

void SSLManager::OnMixedContent(MixedContentHandler* mixed_content) {
  // Ask our delegate to deal with the mixed content.
  NavigationEntry* entry = controller_->GetActiveEntry();
  delegate()->OnMixedContent(controller_, entry->url(), mixed_content);
}

void SSLManager::Observe(NotificationType type,
                         const NotificationSource& source,
                         const NotificationDetails& details) {
  // We should only be getting notifications from our controller.
  DCHECK(source == Source<NavigationController>(controller_));

  // Dispatch by type.
  switch (type) {
    case NOTIFY_NAV_ENTRY_COMMITTED:
      DidCommitProvisionalLoad(details);
      break;
    case NOTIFY_FAIL_PROVISIONAL_LOAD_WITH_ERROR:
      DidFailProvisionalLoadWithError(
          Details<ProvisionalLoadDetails>(details).ptr());
      break;
    case NOTIFY_RESOURCE_RESPONSE_STARTED:
      DidStartResourceResponse(Details<ResourceRequestDetails>(details).ptr());
      break;
    case NOTIFY_RESOURCE_RECEIVED_REDIRECT:
      DidReceiveResourceRedirect(
          Details<ResourceRedirectDetails>(details).ptr());
      break;
    case NOTIFY_LOAD_FROM_MEMORY_CACHE:
      DidLoadFromMemoryCache(
          Details<LoadFromMemoryCacheDetails>(details).ptr());
      break;
   default:
    NOTREACHED() << "The SSLManager received an unexpected notification.";
  }
}

void SSLManager::InitializeEntryIfNeeded(NavigationEntry* entry) {
  DCHECK(entry);

  // If the security style of the entry is SECURITY_STYLE_UNKNOWN, then it is a
  // fresh entry and should get the default style.
  if (entry->ssl().security_style() == SECURITY_STYLE_UNKNOWN) {
    entry->ssl().set_security_style(
        delegate()->GetDefaultStyle(entry->url()));
  }
}

void SSLManager::NavigationStateChanged() {
  NavigationEntry* active_entry = controller_->GetActiveEntry();
  if (!active_entry)
    return;  // Nothing showing yet.

  // This might be a new entry we've never seen before.
  InitializeEntryIfNeeded(active_entry);
}

void SSLManager::DidLoadFromMemoryCache(LoadFromMemoryCacheDetails* details) {
  DCHECK(details);

  // Simulate loading this resource through the usual path.
  // Note that we specify SUB_RESOURCE as the resource type as WebCore only
  // caches sub-resources.
  delegate()->OnRequestStarted(this, details->url(),
                               ResourceType::SUB_RESOURCE,
                               details->ssl_cert_id(),
                               details->ssl_cert_status());
}

void SSLManager::DidCommitProvisionalLoad(
    const NotificationDetails& in_details) {
  NavigationController::LoadCommittedDetails* details =
      Details<NavigationController::LoadCommittedDetails>(in_details).ptr();

  // Ignore in-page navigations, they should not change the security style or
  // the info-bars.
  if (details->is_in_page)
    return;

  // Decode the security details.
  int ssl_cert_id, ssl_cert_status, ssl_security_bits;
  DeserializeSecurityInfo(details->serialized_security_info,
                          &ssl_cert_id, &ssl_cert_status, &ssl_security_bits);

  bool changed = false;
  if (details->is_main_frame) {
    // Update the SSL states of the pending entry.
    NavigationEntry* entry = controller_->GetActiveEntry();
    if (entry) {
      // We may not have an entry if this is a navigation to an initial blank
      // page. Reset the SSL information and add the new data we have.
      entry->ssl() = NavigationEntry::SSLStatus();
      InitializeEntryIfNeeded(entry);  // For security_style.
      entry->ssl().set_cert_id(ssl_cert_id);
      entry->ssl().set_cert_status(ssl_cert_status);
      entry->ssl().set_security_bits(ssl_security_bits);
      changed = true;
    }

    ShowPendingMessages();
  }

  // An HTTPS response may not have a certificate for some reason.  When that
  // happens, use the unauthenticated (HTTP) rather than the authentication
  // broken security style so that we can detect this error condition.
  if (net::IsCertStatusError(ssl_cert_status)) {
    changed |= SetMaxSecurityStyle(SECURITY_STYLE_AUTHENTICATION_BROKEN);
    if (!details->is_main_frame &&
        !details->entry->ssl().has_unsafe_content()) {
      details->entry->ssl().set_has_unsafe_content();
      changed = true;
    }
  } else if (details->entry->url().SchemeIsSecure() && !ssl_cert_id) {
    if (details->is_main_frame) {
      changed |= SetMaxSecurityStyle(SECURITY_STYLE_UNAUTHENTICATED);
    } else {
      // If the frame has been blocked we keep our security style as
      // authenticated in that case as nothing insecure is actually showing or
      // loaded.
      if (!details->is_content_filtered && 
          !details->entry->ssl().has_mixed_content()) {
        details->entry->ssl().set_has_mixed_content();
        changed = true;
      }
    }
  }

  if (changed) {
    // Only send the notification when something actually changed.
    NotificationService::current()->Notify(
        NOTIFY_SSL_STATE_CHANGED,
        Source<NavigationController>(controller_),
        NotificationService::NoDetails());
  }
}

void SSLManager::DidFailProvisionalLoadWithError(
    ProvisionalLoadDetails* details) {
  DCHECK(details);

  // Ignore in-page navigations.
  if (details->in_page_navigation())
    return;

  if (details->main_frame())
    ClearPendingMessages();
}

void SSLManager::DidStartResourceResponse(ResourceRequestDetails* details) {
  DCHECK(details);

  // Notify our delegate that we started a resource request.  Ideally, the
  // delegate should have the ability to cancel the request, but we can't do
  // that yet.
  delegate()->OnRequestStarted(this, details->url(),
                               details->resource_type(),
                               details->ssl_cert_id() ,
                               details->ssl_cert_status());
}

void SSLManager::DidReceiveResourceRedirect(ResourceRedirectDetails* details) {
  // TODO(jcampan): when we receive a redirect for a sub-resource, we may want
  // to clear any mixed/unsafe content error that it may have triggered.
}

void SSLManager::ShowPendingMessages() {
  std::vector<SSLMessageInfo>::const_iterator iter;
  for (iter = pending_messages_.begin();
       iter != pending_messages_.end(); ++iter) {
    ShowMessageWithLink(iter->message, iter->link_text, iter->action);
  }
  ClearPendingMessages();
}

void SSLManager::ClearPendingMessages() {
  pending_messages_.clear();
}

// static
std::string SSLManager::SerializeSecurityInfo(int cert_id,
                                              int cert_status,
                                              int security_bits) {
  Pickle pickle;
  pickle.WriteInt(cert_id);
  pickle.WriteInt(cert_status);
  pickle.WriteInt(security_bits);
  return std::string(static_cast<const char*>(pickle.data()), pickle.size());
}

// static
bool SSLManager::DeserializeSecurityInfo(const std::string& state,
                                         int* cert_id,
                                         int* cert_status,
                                         int* security_bits) {
  DCHECK(cert_id && cert_status && security_bits);
  if (state.empty()) {
    // No SSL used.
    *cert_id = 0;
    *cert_status = 0;
    *security_bits = -1;
    return false;
  }

  Pickle pickle(state.data(), static_cast<int>(state.size()));
  void * iter = NULL;
  pickle.ReadInt(&iter, cert_id);
  pickle.ReadInt(&iter, cert_status);
  pickle.ReadInt(&iter, security_bits);
  return true;
}

// static
bool SSLManager::GetEVCertNames(const net::X509Certificate& cert,
                                std::wstring* short_name,
                                std::wstring* ca_name) {
  DCHECK(short_name || ca_name);

  // EV are required to have an organization name and country.
  if (cert.subject().organization_names.empty() ||
      cert.subject().country_name.empty()) {
    NOTREACHED();
    return false;
  }

  if (short_name) {
    *short_name = l10n_util::GetStringF(
        IDS_SECURE_CONNECTION_EV,
        UTF8ToWide(cert.subject().organization_names[0]),
        UTF8ToWide(cert.subject().country_name));
  }

  if (ca_name) {
    // TODO(wtc): should we show the root CA's name instead?
    *ca_name = l10n_util::GetStringF(
        IDS_SECURE_CONNECTION_EV_CA,
        UTF8ToWide(cert.issuer().organization_names[0]));
  }
  return true;
}

