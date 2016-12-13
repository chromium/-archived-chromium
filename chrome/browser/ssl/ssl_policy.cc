// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_policy.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/ssl/ssl_cert_error_handler.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/ssl/ssl_mixed_content_handler.h"
#include "chrome/browser/ssl/ssl_request_info.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ssl_info.h"
#include "webkit/glue/resource_type.h"

using WebKit::WebConsoleMessage;

class SSLPolicy::ShowMixedContentTask : public Task {
 public:
  ShowMixedContentTask(SSLPolicy* policy, SSLMixedContentHandler* handler);
  virtual ~ShowMixedContentTask();

  virtual void Run();

 private:
  SSLPolicy* policy_;
  scoped_refptr<SSLMixedContentHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(ShowMixedContentTask);
};

SSLPolicy::ShowMixedContentTask::ShowMixedContentTask(SSLPolicy* policy,
                                           SSLMixedContentHandler* handler)
    : policy_(policy),
      handler_(handler) {
}

SSLPolicy::ShowMixedContentTask::~ShowMixedContentTask() {
}

void SSLPolicy::ShowMixedContentTask::Run() {
  policy_->AllowMixedContentForOrigin(handler_->frame_origin());
  policy_->AllowMixedContentForOrigin(handler_->main_frame_origin());
  policy_->backend()->Reload();
}

SSLPolicy::SSLPolicy(SSLPolicyBackend* backend)
    : backend_(backend) {
  DCHECK(backend_);
}

void SSLPolicy::OnCertError(SSLCertErrorHandler* handler) {
  // First we check if we know the policy for this error.
  net::X509Certificate::Policy::Judgment judgment =
      backend_->QueryPolicy(handler->ssl_info().cert,
                            handler->request_url().host());

  if (judgment == net::X509Certificate::Policy::ALLOWED) {
    handler->ContinueRequest();
    return;
  }

  // The judgment is either DENIED or UNKNOWN.
  // For now we handle the DENIED as the UNKNOWN, which means a blocking
  // page is shown to the user every time he comes back to the page.

  switch(handler->cert_error()) {
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
      OnOverridableCertError(handler);
      break;
    case net::ERR_CERT_NO_REVOCATION_MECHANISM:
      // Ignore this error.
      handler->ContinueRequest();
      break;
    case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
      // We ignore this error and display an infobar.
      handler->ContinueRequest();
      backend_->ShowMessage(l10n_util::GetString(
          IDS_CERT_ERROR_UNABLE_TO_CHECK_REVOCATION_INFO_BAR));
      break;
    case net::ERR_CERT_CONTAINS_ERRORS:
    case net::ERR_CERT_REVOKED:
    case net::ERR_CERT_INVALID:
      OnFatalCertError(handler);
      break;
    default:
      NOTREACHED();
      handler->CancelRequest();
      break;
  }
}

void SSLPolicy::OnMixedContent(SSLMixedContentHandler* handler) {
  FilterPolicy::Type filter_policy = FilterPolicy::DONT_FILTER;

  // If the user has added an exception, doctor the |filter_policy|.
  std::string host = GURL(handler->main_frame_origin()).host();
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kForceHTTPS) &&
      backend_->IsForceTLSEnabledForHost(host)) {
    // We're supposed to block all mixed content for this host.
    filter_policy = FilterPolicy::FILTER_ALL;
  } else if (backend_->DidAllowMixedContentForHost(host) ||
             backend_->DidMarkHostAsBroken(host, handler->pid())) {
    // Let the mixed content through.
    filter_policy = FilterPolicy::DONT_FILTER;
  } else if (filter_policy != FilterPolicy::DONT_FILTER) {
    backend_->ShowMessageWithLink(
        l10n_util::GetString(IDS_SSL_INFO_BAR_FILTERED_CONTENT),
        l10n_util::GetString(IDS_SSL_INFO_BAR_SHOW_CONTENT),
        new ShowMixedContentTask(this, handler));
  }

  handler->StartRequest(filter_policy);
  AddMixedContentWarningToConsole(handler);
}

void SSLPolicy::OnRequestStarted(SSLRequestInfo* info) {
  if (net::IsCertStatusError(info->ssl_cert_status()))
    UpdateStateForUnsafeContent(info);

  if (IsMixedContent(info->url(),
                     info->resource_type(),
                     info->filter_policy(),
                     info->frame_origin()))
    UpdateStateForMixedContent(info);
}

void SSLPolicy::UpdateEntry(NavigationEntry* entry) {
  DCHECK(entry);

  InitializeEntryIfNeeded(entry);

  if (!entry->url().SchemeIsSecure())
    return;

  // An HTTPS response may not have a certificate for some reason.  When that
  // happens, use the unauthenticated (HTTP) rather than the authentication
  // broken security style so that we can detect this error condition.
  if (!entry->ssl().cert_id()) {
    entry->ssl().set_security_style(SECURITY_STYLE_UNAUTHENTICATED);
    return;
  }

  if (net::IsCertStatusError(entry->ssl().cert_status())) {
    entry->ssl().set_security_style(SECURITY_STYLE_AUTHENTICATION_BROKEN);
    return;
  }

  SiteInstance* site_instance = entry->site_instance();
  // Note that |site_instance| can be NULL here because NavigationEntries don't
  // necessarily have site instances.  Without a process, the entry can't
  // possibly have mixed content.  See bug http://crbug.com/12423.
  if (site_instance &&
      backend_->DidMarkHostAsBroken(entry->url().host(),
                                    site_instance->GetProcess()->pid()))
    entry->ssl().set_has_mixed_content();
}

// static
bool SSLPolicy::IsMixedContent(const GURL& url,
                               ResourceType::Type resource_type,
                               FilterPolicy::Type filter_policy,
                               const std::string& frame_origin) {
  ////////////////////////////////////////////////////////////////////////////
  // WARNING: This function is called from both the IO and UI threads.  Do  //
  //          not touch any non-thread-safe objects!  You have been warned. //
  ////////////////////////////////////////////////////////////////////////////

  // We can't possibly have mixed content when loading the main frame.
  if (resource_type == ResourceType::MAIN_FRAME)
    return false;

  // If we've filtered the resource, then it's no longer dangerous.
  if (filter_policy != FilterPolicy::DONT_FILTER)
    return false;

  // If the frame doing the loading is already insecure, then we must have
  // already dealt with whatever mixed content might be going on.
  if (!GURL(frame_origin).SchemeIsSecure())
    return false;

  // We aren't worried about mixed content if we're loading an HTTPS URL.
  if (url.SchemeIsSecure())
    return false;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// SSLBlockingPage::Delegate methods

SSLErrorInfo SSLPolicy::GetSSLErrorInfo(SSLCertErrorHandler* handler) {
  return SSLErrorInfo::CreateError(
      SSLErrorInfo::NetErrorToErrorType(handler->cert_error()),
      handler->ssl_info().cert, handler->request_url());
}

void SSLPolicy::OnDenyCertificate(SSLCertErrorHandler* handler) {
  // Default behavior for rejecting a certificate.
  //
  // While DenyCertForHost() executes synchronously on this thread,
  // CancelRequest() gets posted to a different thread. Calling
  // DenyCertForHost() first ensures deterministic ordering.
  backend_->DenyCertForHost(handler->ssl_info().cert,
                            handler->request_url().host());
  handler->CancelRequest();
}

void SSLPolicy::OnAllowCertificate(SSLCertErrorHandler* handler) {
  // Default behavior for accepting a certificate.
  // Note that we should not call SetMaxSecurityStyle here, because the active
  // NavigationEntry has just been deleted (in HideInterstitialPage) and the
  // new NavigationEntry will not be set until DidNavigate.  This is ok,
  // because the new NavigationEntry will have its max security style set
  // within DidNavigate.
  //
  // While AllowCertForHost() executes synchronously on this thread,
  // ContinueRequest() gets posted to a different thread. Calling
  // AllowCertForHost() first ensures deterministic ordering.
  backend_->AllowCertForHost(handler->ssl_info().cert,
                             handler->request_url().host());
  handler->ContinueRequest();
}

////////////////////////////////////////////////////////////////////////////////
// Certificate Error Routines

void SSLPolicy::OnOverridableCertError(SSLCertErrorHandler* handler) {
  if (handler->resource_type() != ResourceType::MAIN_FRAME) {
    // A sub-resource has a certificate error.  The user doesn't really
    // have a context for making the right decision, so block the
    // request hard, without an info bar to allow showing the insecure
    // content.
    handler->DenyRequest();
    return;
  }
  // We need to ask the user to approve this certificate.
  SSLBlockingPage* blocking_page = new SSLBlockingPage(handler, this);
  blocking_page->Show();
}

void SSLPolicy::OnFatalCertError(SSLCertErrorHandler* handler) {
  if (handler->resource_type() != ResourceType::MAIN_FRAME) {
    handler->DenyRequest();
    return;
  }
  handler->CancelRequest();
  ShowErrorPage(handler);
  // No need to degrade our security indicators because we didn't continue.
}

void SSLPolicy::ShowErrorPage(SSLCertErrorHandler* handler) {
  SSLErrorInfo error_info = GetSSLErrorInfo(handler);

  // Let's build the html error page.
  DictionaryValue strings;
  strings.SetString(L"title", l10n_util::GetString(IDS_SSL_ERROR_PAGE_TITLE));
  strings.SetString(L"headLine", error_info.title());
  strings.SetString(L"description", error_info.details());
  strings.SetString(L"moreInfoTitle",
                    l10n_util::GetString(IDS_CERT_ERROR_EXTRA_INFO_TITLE));
  SSLBlockingPage::SetExtraInfo(&strings, error_info.extra_information());

  strings.SetString(L"back", l10n_util::GetString(IDS_SSL_ERROR_PAGE_BACK));

  strings.SetString(L"textdirection",
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
      L"rtl" : L"ltr");

  static const StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SSL_ERROR_HTML));

  std::string html_text(jstemplate_builder::GetTemplateHtml(html, &strings,
                                                            "template_root"));

  TabContents* tab  = handler->GetTabContents();
  int cert_id = CertStore::GetSharedInstance()->StoreCert(
      handler->ssl_info().cert, tab->render_view_host()->process()->pid());
  std::string security_info =
      SSLManager::SerializeSecurityInfo(cert_id,
                                        handler->ssl_info().cert_status,
                                        handler->ssl_info().security_bits);
  tab->render_view_host()->LoadAlternateHTMLString(html_text,
                                                   true,
                                                   handler->request_url(),
                                                   security_info);
  tab->controller().GetActiveEntry()->set_page_type(
      NavigationEntry::ERROR_PAGE);
}

void SSLPolicy::AddMixedContentWarningToConsole(
    SSLMixedContentHandler* handler) {
  const std::wstring& text = l10n_util::GetStringF(
      IDS_MIXED_CONTENT_LOG_MESSAGE,
      UTF8ToWide(handler->frame_origin()),
      UTF8ToWide(handler->request_url().spec()));
  backend_->AddMessageToConsole(
      WideToUTF16Hack(text), WebConsoleMessage::LevelWarning);
}

void SSLPolicy::InitializeEntryIfNeeded(NavigationEntry* entry) {
  if (entry->ssl().security_style() != SECURITY_STYLE_UNKNOWN)
    return;

  entry->ssl().set_security_style(entry->url().SchemeIsSecure() ?
      SECURITY_STYLE_AUTHENTICATED : SECURITY_STYLE_UNAUTHENTICATED);
}

void SSLPolicy::MarkOriginAsBroken(const std::string& origin, int pid) {
  GURL parsed_origin(origin);
  if (!parsed_origin.SchemeIsSecure())
    return;

  backend_->MarkHostAsBroken(parsed_origin.host(), pid);
}

void SSLPolicy::AllowMixedContentForOrigin(const std::string& origin) {
  GURL parsed_origin(origin);
  if (!parsed_origin.SchemeIsSecure())
    return;

  backend_->AllowMixedContentForHost(parsed_origin.host());
}

void SSLPolicy::UpdateStateForMixedContent(SSLRequestInfo* info) {
  if (info->resource_type() != ResourceType::MAIN_FRAME ||
      info->resource_type() != ResourceType::SUB_FRAME) {
    // The frame's origin now contains mixed content and therefore is broken.
    MarkOriginAsBroken(info->frame_origin(), info->pid());
  }

  if (info->resource_type() != ResourceType::MAIN_FRAME) {
    // The main frame now contains a frame with mixed content.  Therefore, we
    // mark the main frame's origin as broken too.
    MarkOriginAsBroken(info->main_frame_origin(), info->pid());
  }
}

void SSLPolicy::UpdateStateForUnsafeContent(SSLRequestInfo* info) {
  // This request as a broken cert, which means its host is broken.
  backend_->MarkHostAsBroken(info->url().host(), info->pid());
  UpdateStateForMixedContent(info);
}
