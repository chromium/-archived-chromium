// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_manager.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/load_from_memory_cache_details.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_request_details.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/ssl/ssl_host_state.h"
#include "chrome/browser/ssl/ssl_policy.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/provisional_load_details.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_type.h"

#if defined(OS_WIN)
// TODO(port): Port these files.
#include "chrome/browser/load_notification_details.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/views/controls/link.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif


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
    : delegate_(delegate),
      controller_(controller),
      ssl_host_state_(controller->profile()->GetSSLHostState()) {
  DCHECK(controller_);

  // If do delegate is supplied, use the default policy.
  if (!delegate_)
    delegate_ = SSLPolicy::GetDefaultPolicy();

  // Subscribe to various notifications.
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, NotificationType::RESOURCE_RESPONSE_STARTED,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, NotificationType::RESOURCE_RECEIVED_REDIRECT,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, NotificationType::LOAD_FROM_MEMORY_CACHE,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, NotificationType::SSL_INTERNAL_STATE_CHANGED,
                 NotificationService::AllSources());
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
void SSLManager::MarkHostAsBroken(const std::string& host) {
  ssl_host_state_->MarkHostAsBroken(host);
  DispatchSSLInternalStateChanged();
}

// Delegate API method.
bool SSLManager::DidMarkHostAsBroken(const std::string& host) const {
  return ssl_host_state_->DidMarkHostAsBroken(host);
}

// Delegate API method.
void SSLManager::DenyCertForHost(net::X509Certificate* cert,
                                 const std::string& host) {
  // Remember that we don't like this cert for this host.
  ssl_host_state_->DenyCertForHost(cert, host);
  DispatchSSLInternalStateChanged();
}

// Delegate API method.
void SSLManager::AllowCertForHost(net::X509Certificate* cert,
                                  const std::string& host) {
  ssl_host_state_->AllowCertForHost(cert, host);
  DispatchSSLInternalStateChanged();
}

// Delegate API method.
net::X509Certificate::Policy::Judgment SSLManager::QueryPolicy(
    net::X509Certificate* cert, const std::string& host) {
  return ssl_host_state_->QueryPolicy(cert, host);
}

// Delegate API method.
void SSLManager::AllowMixedContentForHost(const std::string& host) {
  ssl_host_state_->AllowMixedContentForHost(host);
  DispatchSSLInternalStateChanged();
}

// Delegate API method.
bool SSLManager::DidAllowMixedContentForHost(const std::string& host) const {
  return ssl_host_state_->DidAllowMixedContentForHost(host);
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
                                       ResourceType::Type resource_type,
                                       const std::string& frame_origin,
                                       const std::string& main_frame_origin,
                                       MessageLoop* ui_loop)
    : ui_loop_(ui_loop),
      io_loop_(MessageLoop::current()),
      manager_(NULL),
      request_id_(0, 0),
      resource_dispatcher_host_(rdh),
      request_url_(request->url()),
      resource_type_(resource_type),
      frame_origin_(frame_origin),
      main_frame_origin_(main_frame_origin),
      request_has_been_notified_(false) {
  DCHECK(MessageLoop::current() != ui_loop);

  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request);
  request_id_.process_id = info->process_id;
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
      SSLManager::CertError* cert_error = AsCertError();
      if (cert_error)
        request->SimulateSSLError(error, cert_error->ssl_info());
      else
        request->SimulateError(error);
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
    const std::string& frame_origin,
    const std::string& main_frame_origin,
    int cert_error,
    net::X509Certificate* cert,
    MessageLoop* ui_loop)
    : ErrorHandler(rdh, request, resource_type, frame_origin,
                   main_frame_origin, ui_loop),
      cert_error_(cert_error) {
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
      NewRunnableMethod(new CertError(rdh,
                                      request,
                                      info->resource_type,
                                      info->frame_origin,
                                      info->main_frame_origin,
                                      cert_error,
                                      cert,
                                      ui_loop),
                        &CertError::Dispatch));
}

// static
bool SSLManager::ShouldStartRequest(ResourceDispatcherHost* rdh,
                                    URLRequest* request,
                                    MessageLoop* ui_loop) {
  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request);
  DCHECK(info);

  // We cheat here and talk to the SSLPolicy on the IO thread because we need
  // to respond synchronously to avoid delaying all network requests...
  if (!SSLPolicy::IsMixedContent(request->url(),
                                 info->resource_type,
                                 info->main_frame_origin))
    return true;


  ui_loop->PostTask(FROM_HERE,
      NewRunnableMethod(new MixedContentHandler(rdh, request,
                                                info->resource_type,
                                                info->frame_origin,
                                                info->main_frame_origin,
                                                ui_loop),
                        &MixedContentHandler::Dispatch));
  return false;
}

void SSLManager::OnCertError(CertError* error) {
  delegate()->OnCertError(error);
}

void SSLManager::OnMixedContent(MixedContentHandler* handler) {
  delegate()->OnMixedContent(handler);
}

void SSLManager::Observe(NotificationType type,
                         const NotificationSource& source,
                         const NotificationDetails& details) {
  // We should only be getting notifications from our controller.
  DCHECK(source == Source<NavigationController>(controller_));

  // Dispatch by type.
  switch (type.value) {
    case NotificationType::NAV_ENTRY_COMMITTED:
      DidCommitProvisionalLoad(details);
      break;
    case NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR:
      DidFailProvisionalLoadWithError(
          Details<ProvisionalLoadDetails>(details).ptr());
      break;
    case NotificationType::RESOURCE_RESPONSE_STARTED:
      DidStartResourceResponse(Details<ResourceRequestDetails>(details).ptr());
      break;
    case NotificationType::RESOURCE_RECEIVED_REDIRECT:
      DidReceiveResourceRedirect(
          Details<ResourceRedirectDetails>(details).ptr());
      break;
    case NotificationType::LOAD_FROM_MEMORY_CACHE:
      DidLoadFromMemoryCache(
          Details<LoadFromMemoryCacheDetails>(details).ptr());
      break;
    case NotificationType::SSL_INTERNAL_STATE_CHANGED:
      DidChangeSSLInternalState();
      break;
    default:
      NOTREACHED() << "The SSLManager received an unexpected notification.";
  }
}

void SSLManager::DispatchSSLInternalStateChanged() {
  NotificationService::current()->Notify(
      NotificationType::SSL_INTERNAL_STATE_CHANGED,
      Source<NavigationController>(controller_),
      NotificationService::NoDetails());
}

void SSLManager::DispatchSSLVisibleStateChanged() {
  NotificationService::current()->Notify(
      NotificationType::SSL_VISIBLE_STATE_CHANGED,
      Source<NavigationController>(controller_),
      NotificationService::NoDetails());
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
  scoped_refptr<RequestInfo> info = new RequestInfo(
      this,
      details->url(),
      ResourceType::SUB_RESOURCE,
      details->frame_origin(),
      details->main_frame_origin(),
      details->ssl_cert_id(),
      details->ssl_cert_status());

  // Simulate loading this resource through the usual path.
  delegate()->OnRequestStarted(info.get());
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
  if (net::IsCertStatusError(ssl_cert_status) &&
      !details->is_content_filtered) {
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
        NotificationType::SSL_VISIBLE_STATE_CHANGED,
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

  scoped_refptr<RequestInfo> info = new RequestInfo(
      this,
      details->url(),
      details->resource_type(),
      details->frame_origin(),
      details->main_frame_origin(),
      details->ssl_cert_id(),
      details->ssl_cert_status());

  // Notify our delegate that we started a resource request.  Ideally, the
  // delegate should have the ability to cancel the request, but we can't do
  // that yet.
  delegate()->OnRequestStarted(info.get());
}

void SSLManager::DidReceiveResourceRedirect(ResourceRedirectDetails* details) {
  // TODO(abarth): Make sure our redirect behavior is correct.  If we ever see
  //               a non-HTTPS resource in the redirect chain, we want to
  //               trigger mixed content, even if the redirect chain goes back
  //               to HTTPS.  This is because the network attacker can redirect
  //               the HTTP request to https://attacker.com/payload.js.
}

void SSLManager::ShowPendingMessages() {
  std::vector<SSLMessageInfo>::const_iterator iter;
  for (iter = pending_messages_.begin();
       iter != pending_messages_.end(); ++iter) {
    ShowMessageWithLink(iter->message, iter->link_text, iter->action);
  }
  ClearPendingMessages();
}

void SSLManager::DidChangeSSLInternalState() {
  // TODO(abarth): We'll need to do something here in the next step.
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
