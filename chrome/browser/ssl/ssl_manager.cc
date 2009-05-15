// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_manager.h"

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/load_from_memory_cache_details.h"
#include "chrome/browser/renderer_host/resource_request_details.h"
#include "chrome/browser/ssl/ssl_cert_error_handler.h"
#include "chrome/browser/ssl/ssl_mixed_content_handler.h"
#include "chrome/browser/ssl/ssl_policy.h"
#include "chrome/browser/ssl/ssl_request_info.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/provisional_load_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"
#include "net/base/cert_status_flags.h"

// static
void SSLManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kMixedContentFiltering,
                             FilterPolicy::DONT_FILTER);
}

SSLManager::SSLManager(NavigationController* controller)
    : backend_(controller),
      policy_(new SSLPolicy(&backend_)),
      controller_(controller) {
  DCHECK(controller_);

  // Subscribe to various notifications.
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

bool SSLManager::ProcessedSSLErrorFromRequest() const {
  NavigationEntry* entry = controller_->GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return false;
  }

  return net::IsCertStatusError(entry->ssl().cert_status());
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

  // A certificate error occurred.  Construct a SSLCertErrorHandler object and
  // hand it over to the UI thread for processing.
  ui_loop->PostTask(FROM_HERE,
      NewRunnableMethod(new SSLCertErrorHandler(rdh,
                                                request,
                                                info->resource_type,
                                                info->frame_origin,
                                                info->main_frame_origin,
                                                cert_error,
                                                cert,
                                                ui_loop),
                        &SSLCertErrorHandler::Dispatch));
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
                                 info->filter_policy,
                                 info->frame_origin))
    return true;


  ui_loop->PostTask(FROM_HERE,
      NewRunnableMethod(new SSLMixedContentHandler(rdh,
                                                   request,
                                                   info->resource_type,
                                                   info->frame_origin,
                                                   info->main_frame_origin,
                                                   info->process_id,
                                                   ui_loop),
                        &SSLMixedContentHandler::Dispatch));
  return false;
}

void SSLManager::Observe(NotificationType type,
                         const NotificationSource& source,
                         const NotificationDetails& details) {
  // Dispatch by type.
  switch (type.value) {
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

void SSLManager::DispatchSSLVisibleStateChanged() {
  NotificationService::current()->Notify(
      NotificationType::SSL_VISIBLE_STATE_CHANGED,
      Source<NavigationController>(controller_),
      NotificationService::NoDetails());
}

void SSLManager::UpdateEntry(NavigationEntry* entry) {
  // We don't always have a navigation entry to update, for example in the
  // case of the Web Inspector.
  if (!entry)
    return;

  NavigationEntry::SSLStatus original_ssl_status = entry->ssl();  // Copy!

  policy()->UpdateEntry(entry);

  if (!entry->ssl().Equals(original_ssl_status))
    DispatchSSLVisibleStateChanged();
}

void SSLManager::DidLoadFromMemoryCache(LoadFromMemoryCacheDetails* details) {
  DCHECK(details);

  // Simulate loading this resource through the usual path.
  // Note that we specify SUB_RESOURCE as the resource type as WebCore only
  // caches sub-resources.
  // This resource must have been loaded with FilterPolicy::DONT_FILTER because
  // filtered resouces aren't cachable.
  scoped_refptr<SSLRequestInfo> info = new SSLRequestInfo(
      details->url(),
      ResourceType::SUB_RESOURCE,
      details->frame_origin(),
      details->main_frame_origin(),
      FilterPolicy::DONT_FILTER,
      details->pid(),
      details->ssl_cert_id(),
      details->ssl_cert_status());

  // Simulate loading this resource through the usual path.
  policy()->OnRequestStarted(info.get());
}

void SSLManager::DidCommitProvisionalLoad(
    const NotificationDetails& in_details) {
  NavigationController::LoadCommittedDetails* details =
      Details<NavigationController::LoadCommittedDetails>(in_details).ptr();

  // Ignore in-page navigations, they should not change the security style or
  // the info-bars.
  if (details->is_in_page)
    return;

  NavigationEntry* entry = controller_->GetActiveEntry();

  if (details->is_main_frame) {
    if (entry) {
      // Decode the security details.
      int ssl_cert_id, ssl_cert_status, ssl_security_bits;
      DeserializeSecurityInfo(details->serialized_security_info,
                              &ssl_cert_id,
                              &ssl_cert_status,
                              &ssl_security_bits);

      // We may not have an entry if this is a navigation to an initial blank
      // page. Reset the SSL information and add the new data we have.
      entry->ssl() = NavigationEntry::SSLStatus();
      entry->ssl().set_cert_id(ssl_cert_id);
      entry->ssl().set_cert_status(ssl_cert_status);
      entry->ssl().set_security_bits(ssl_security_bits);
    }
    backend_.ShowPendingMessages();
  }

  UpdateEntry(entry);
}

void SSLManager::DidFailProvisionalLoadWithError(
    ProvisionalLoadDetails* details) {
  DCHECK(details);

  // Ignore in-page navigations.
  if (details->in_page_navigation())
    return;

  if (details->main_frame())
    backend_.ClearPendingMessages();
}

void SSLManager::DidStartResourceResponse(ResourceRequestDetails* details) {
  DCHECK(details);

  scoped_refptr<SSLRequestInfo> info = new SSLRequestInfo(
      details->url(),
      details->resource_type(),
      details->frame_origin(),
      details->main_frame_origin(),
      details->filter_policy(),
      details->origin_pid(),
      details->ssl_cert_id(),
      details->ssl_cert_status());

  // Notify our policy that we started a resource request.  Ideally, the
  // policy should have the ability to cancel the request, but we can't do
  // that yet.
  policy()->OnRequestStarted(info.get());
}

void SSLManager::DidReceiveResourceRedirect(ResourceRedirectDetails* details) {
  // TODO(abarth): Make sure our redirect behavior is correct.  If we ever see
  //               a non-HTTPS resource in the redirect chain, we want to
  //               trigger mixed content, even if the redirect chain goes back
  //               to HTTPS.  This is because the network attacker can redirect
  //               the HTTP request to https://attacker.com/payload.js.
}

void SSLManager::DidChangeSSLInternalState() {
  UpdateEntry(controller_->GetActiveEntry());
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
  return pickle.ReadInt(&iter, cert_id) &&
         pickle.ReadInt(&iter, cert_status) &&
         pickle.ReadInt(&iter, security_bits);
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
