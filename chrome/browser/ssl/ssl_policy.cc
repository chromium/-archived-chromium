// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_policy.h"

#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ssl_info.h"
#include "webkit/glue/console_message_level.h"
#include "webkit/glue/resource_type.h"

#if defined(OS_WIN)
// TODO(port): port these files.
#include "chrome/browser/tab_contents/tab_contents.h"
#elif defined(OS_POSIX)
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

// Wrap all these helper classes in an anonymous namespace.
namespace {

class ShowMixedContentTask : public Task {
 public:
  ShowMixedContentTask(SSLManager::MixedContentHandler* handler);
  virtual ~ShowMixedContentTask();

  virtual void Run();

 private:
  scoped_refptr<SSLManager::MixedContentHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(ShowMixedContentTask);
};

ShowMixedContentTask::ShowMixedContentTask(
    SSLManager::MixedContentHandler* handler)
    : handler_(handler) {
}

ShowMixedContentTask::~ShowMixedContentTask() {
}

void ShowMixedContentTask::Run() {
  handler_->manager()->AllowMixedContentForHost(
      GURL(handler_->main_frame_origin()).host());

  // Reload the page.
  handler_->manager()->controller()->Reload(true);
}

static void ShowErrorPage(SSLPolicy* policy, SSLManager::CertError* error) {
  SSLErrorInfo error_info = policy->GetSSLErrorInfo(error);

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

  WebContents* tab  = error->GetWebContents();
  int cert_id = CertStore::GetSharedInstance()->StoreCert(
      error->ssl_info().cert, tab->render_view_host()->process()->pid());
  std::string security_info =
      SSLManager::SerializeSecurityInfo(cert_id,
                                        error->ssl_info().cert_status,
                                        error->ssl_info().security_bits);
  tab->render_view_host()->LoadAlternateHTMLString(html_text,
                                                   true,
                                                   error->request_url(),
                                                   security_info);
  tab->controller()->GetActiveEntry()->set_page_type(
      NavigationEntry::ERROR_PAGE);
}

static void ShowBlockingPage(SSLPolicy* policy, SSLManager::CertError* error) {
  SSLBlockingPage* blocking_page = new SSLBlockingPage(error, policy);
  blocking_page->Show();
}

}  // namespace

SSLPolicy::SSLPolicy() {
}

SSLPolicy* SSLPolicy::GetDefaultPolicy() {
  return Singleton<SSLPolicy>::get();
}

void SSLPolicy::OnCertError(SSLManager::CertError* error) {
  // First we check if we know the policy for this error.
  net::X509Certificate::Policy::Judgment judgment =
      error->manager()->QueryPolicy(error->ssl_info().cert,
                                    error->request_url().host());

  if (judgment == net::X509Certificate::Policy::ALLOWED) {
    // We've been told to allow this certificate.
    if (error->manager()->SetMaxSecurityStyle(
            SECURITY_STYLE_AUTHENTICATION_BROKEN)) {
      NotificationService::current()->Notify(
          NotificationType::SSL_VISIBLE_STATE_CHANGED,
          Source<NavigationController>(error->manager()->controller()),
          Details<NavigationEntry>(
              error->manager()->controller()->GetActiveEntry()));
    }
    error->ContinueRequest();
    return;
  }

  // The judgment is either DENIED or UNKNOWN.
  // For now we handle the DENIED as the UNKNOWN, which means a blocking
  // page is shown to the user every time he comes back to the page.

  switch(error->cert_error()) {
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
      OnOverridableCertError(error);
      break;
    case net::ERR_CERT_NO_REVOCATION_MECHANISM:
      // Ignore this error.
      error->ContinueRequest();
      break;
    case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
      // We ignore this error and display an infobar.
      error->ContinueRequest();
      error->manager()->ShowMessage(l10n_util::GetString(
          IDS_CERT_ERROR_UNABLE_TO_CHECK_REVOCATION_INFO_BAR));
      break;
    case net::ERR_CERT_CONTAINS_ERRORS:
    case net::ERR_CERT_REVOKED:
    case net::ERR_CERT_INVALID:
      OnFatalCertError(error);
      break;
    default:
      NOTREACHED();
      error->CancelRequest();
      break;
  }
}

void SSLPolicy::OnMixedContent(SSLManager::MixedContentHandler* handler) {
  // Get the user's mixed content preference.
  PrefService* prefs = handler->GetWebContents()->profile()->GetPrefs();
  FilterPolicy::Type filter_policy =
      FilterPolicy::FromInt(prefs->GetInteger(prefs::kMixedContentFiltering));

  // If the user have added an exception, doctor the |filter_policy|.
  if (handler->manager()->DidAllowMixedContentForHost(
           GURL(handler->main_frame_origin()).host()))
    filter_policy = FilterPolicy::DONT_FILTER;

  if (filter_policy != FilterPolicy::DONT_FILTER) {
    handler->manager()->ShowMessageWithLink(
        l10n_util::GetString(IDS_SSL_INFO_BAR_FILTERED_CONTENT),
        l10n_util::GetString(IDS_SSL_INFO_BAR_SHOW_CONTENT),
        new ShowMixedContentTask(handler));
  }
  handler->StartRequest(filter_policy);

  NavigationEntry* entry =
      handler->manager()->controller()->GetLastCommittedEntry();
  // We might not have a navigation entry in some cases (e.g. when a	
  // HTTPS page opens a popup with no URL and then populate it with	
  // document.write()). See bug http://crbug.com/3845.
  if (!entry)
    return;

  // Even though we are loading the mixed-content resource, it will not be
  // included in the page when we set the policy to FILTER_ALL or
  // FILTER_ALL_EXCEPT_IMAGES (only images and they are stamped with warning
  // icons), so we don't set the mixed-content mode in these cases.
  if (filter_policy == FilterPolicy::DONT_FILTER)
    entry->ssl().set_has_mixed_content();

  // Print a message indicating the mixed-contents resource in the console.
  const std::wstring& msg = l10n_util::GetStringF(
      IDS_MIXED_CONTENT_LOG_MESSAGE,
      UTF8ToWide(entry->url().spec()),
      UTF8ToWide(handler->request_url().spec()));
  handler->manager()->AddMessageToConsole(msg, MESSAGE_LEVEL_WARNING);

  NotificationService::current()->Notify(
      NotificationType::SSL_VISIBLE_STATE_CHANGED,
      Source<NavigationController>(handler->manager()->controller()),
      Details<NavigationEntry>(entry));
}

void SSLPolicy::OnRequestStarted(SSLManager::RequestInfo* info) {
  // These schemes never leave the browser and don't require a warning.
  if (info->url().SchemeIs(chrome::kDataScheme) ||
      info->url().SchemeIs(chrome::kJavaScriptScheme) ||
      info->url().SchemeIs(chrome::kAboutScheme))
    return;

  NavigationEntry* entry = info->manager()->controller()->GetActiveEntry();
  if (!entry) {
    // We may not have an entry for cases such as the inspector.
    return;
  }

  NavigationEntry::SSLStatus& ssl = entry->ssl();
  bool changed = false;
  if (!entry->url().SchemeIsSecure() ||  // Current page is not secure.
      info->resource_type() == ResourceType::MAIN_FRAME ||  // Main frame load.
      net::IsCertStatusError(ssl.cert_status())) {  // There is already
          // an error for the main page, don't report sub-resources as unsafe
          // content.
    // No mixed/unsafe content check necessary.
    return;
  }

  if (info->url().SchemeIsSecure()) {
    // Check for insecure content (anything served over intranet is considered
    // insecure).

    // TODO(jcampan): bug #1178228 Disabling the broken style for intranet
    // hosts for beta as it is missing error strings (and cert status).
    // if (IsIntranetHost(url.host()) ||
    //    net::IsCertStatusError(info->ssl_cert_status())) {
    if (net::IsCertStatusError(info->ssl_cert_status())) {
      // The resource is unsafe.
      if (!ssl.has_unsafe_content()) {
        changed = true;
        ssl.set_has_unsafe_content();
        info->manager()->SetMaxSecurityStyle(
            SECURITY_STYLE_AUTHENTICATION_BROKEN);
      }
    }
  }

  if (changed) {
    // Only send the notification when something actually changed.
    NotificationService::current()->Notify(
        NotificationType::SSL_VISIBLE_STATE_CHANGED,
        Source<NavigationController>(info->manager()->controller()),
        NotificationService::NoDetails());
  }
}

SecurityStyle SSLPolicy::GetDefaultStyle(const GURL& url) {
  // Show the secure style for HTTPS.
  if (url.SchemeIsSecure()) {
    // TODO(jcampan): bug #1178228 Disabling the broken style for intranet
    // hosts for beta as it is missing error strings (and cert status).
    // CAs issue certs for intranet hosts to anyone.
    // if (IsIntranetHost(url.host()))
    //   return SECURITY_STYLE_AUTHENTICATION_BROKEN;

    return SECURITY_STYLE_AUTHENTICATED;
  }

  // Otherwise, show the unauthenticated style.
  return SECURITY_STYLE_UNAUTHENTICATED;
}

// static
bool SSLPolicy::IsMixedContent(const GURL& url,
                               ResourceType::Type resource_type,
                               const std::string& main_frame_origin) {
  ////////////////////////////////////////////////////////////////////////////
  // WARNING: This function is called from both the IO and UI threads.  Do  //
  //          not touch any non-thread-safe objects!  You have been warned. //
  ////////////////////////////////////////////////////////////////////////////

  // We can't possibly have mixed content when loading the main frame.
  if (resource_type == ResourceType::MAIN_FRAME)
    return false;

  // TODO(abarth): This is wrong, but it matches our current behavior.
  //               I'll fix this in a subsequent step.
  return GURL(main_frame_origin).SchemeIsSecure() && !url.SchemeIsSecure();
}

////////////////////////////////////////////////////////////////////////////////
// SSLBlockingPage::Delegate methods

SSLErrorInfo SSLPolicy::GetSSLErrorInfo(SSLManager::CertError* error) {
  return SSLErrorInfo::CreateError(
      SSLErrorInfo::NetErrorToErrorType(error->cert_error()),
      error->ssl_info().cert, error->request_url());
}

void SSLPolicy::OnDenyCertificate(SSLManager::CertError* error) {
  // Default behavior for rejecting a certificate.
  error->CancelRequest();
  error->manager()->DenyCertForHost(error->ssl_info().cert,
                                    error->request_url().host());
}

void SSLPolicy::OnAllowCertificate(SSLManager::CertError* error) {
  // Default behavior for accepting a certificate.
  // Note that we should not call SetMaxSecurityStyle here, because the active
  // NavigationEntry has just been deleted (in HideInterstitialPage) and the
  // new NavigationEntry will not be set until DidNavigate.  This is ok,
  // because the new NavigationEntry will have its max security style set
  // within DidNavigate.
  error->ContinueRequest();
  error->manager()->AllowCertForHost(error->ssl_info().cert,
                                     error->request_url().host());
}

////////////////////////////////////////////////////////////////////////////////
// Certificate Error Routines

void SSLPolicy::OnOverridableCertError(SSLManager::CertError* error) {
  if (error->resource_type() != ResourceType::MAIN_FRAME) {
    // A sub-resource has a certificate error.  The user doesn't really
    // have a context for making the right decision, so block the
    // request hard, without an info bar to allow showing the insecure
    // content.
    error->DenyRequest();
    return;
  }
  // We need to ask the user to approve this certificate.
  ShowBlockingPage(this, error);
}

void SSLPolicy::OnFatalCertError(SSLManager::CertError* error) {
  if (error->resource_type() != ResourceType::MAIN_FRAME) {
    error->DenyRequest();
    return;
  }
  error->CancelRequest();
  ShowErrorPage(this, error);
  // No need to degrade our security indicators because we didn't continue.
}
