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

#include "chrome/browser/ssl_policy.h"

#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "chrome/browser/browser_resources.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/ssl_error_info.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/time_format.h"
#include "generated_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ssl_info.h"
#include "webkit/glue/console_message_level.h"
#include "webkit/glue/resource_type.h"

// Wrap all these helper classes in an anonymous namespace.
namespace {

static const char kDot = '.';

class ShowUnsafeContentTask : public Task {
 public:
  ShowUnsafeContentTask(const GURL& main_frame_url,
                        SSLManager::ErrorHandler* error_handler);
  virtual ~ShowUnsafeContentTask();

  virtual void Run();

 private:
  scoped_refptr<SSLManager::ErrorHandler> error_handler_;
  GURL main_frame_url_;

  DISALLOW_EVIL_CONSTRUCTORS(ShowUnsafeContentTask);
};

ShowUnsafeContentTask::ShowUnsafeContentTask(
    const GURL& main_frame_url,
    SSLManager::ErrorHandler* error_handler)
    : main_frame_url_(main_frame_url),
      error_handler_(error_handler) {
}

ShowUnsafeContentTask::~ShowUnsafeContentTask() {
}

void ShowUnsafeContentTask::Run() {
  error_handler_->manager()->AllowShowInsecureContentForURL(main_frame_url_);
  // Reload the page.
  DCHECK(error_handler_->GetTabContents()->type() == TAB_CONTENTS_WEB);
  WebContents* tab = error_handler_->GetTabContents()->AsWebContents();
  tab->controller()->Reload();
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

  DCHECK(error->GetTabContents()->type() == TAB_CONTENTS_WEB);
  WebContents* tab  = error->GetTabContents()->AsWebContents();
  int cert_id = CertStore::GetSharedInstance()->StoreCert(
      error->ssl_info().cert, tab->render_view_host()->process()->host_id());
  std::string security_info =
      SSLManager::SerializeSecurityInfo(cert_id,
                                        error->ssl_info().cert_status,
                                        error->ssl_info().security_bits);
  tab->render_view_host()->LoadAlternateHTMLString(html_text,
                                                   true,
                                                   error->request_url(),
                                                   security_info);
  tab->controller()->GetActiveEntry()->SetPageType(NavigationEntry::ERROR_PAGE);
}

static void ShowBlockingPage(SSLPolicy* policy, SSLManager::CertError* error) {
  SSLBlockingPage* blocking_page = new SSLBlockingPage(error, policy);
  blocking_page->Show();
}

static bool IsIntranetHost(const std::string& host) {
  const size_t dot = host.find(kDot);
  return dot == std::basic_string<CHAR>::npos ||
         dot == host.length() - 1;
}

class CommonNameInvalidPolicy : public SSLPolicy {
 public:
  static SSLPolicy* GetInstance() {
    return Singleton<CommonNameInvalidPolicy>::get();
  }

  void OnCertError(const GURL& main_frame_url,
                   SSLManager::CertError* error) {
    // We need to ask the user to approve this certificate.
    ShowBlockingPage(this, error);
  }
};

class DateInvalidPolicy : public SSLPolicy {
 public:
  static SSLPolicy* GetInstance() {
    return Singleton<DateInvalidPolicy>::get();
  }

  void OnCertError(const GURL& main_frame_url,
                   SSLManager::CertError* error) {
    // We need to ask the user to approve this certificate.
    ShowBlockingPage(this, error);
  }
};

class AuthorityInvalidPolicy : public SSLPolicy {
 public:
  static SSLPolicy* GetInstance() {
    return Singleton<AuthorityInvalidPolicy>::get();
  }

  void OnCertError(const GURL& main_frame_url,
                   SSLManager::CertError* error) {
    // We need to ask the user to approve this certificate.
    ShowBlockingPage(this, error);
  }
};

class ContainsErrorsPolicy : public SSLPolicy {
 public:
  static SSLPolicy* GetInstance() {
    return Singleton<ContainsErrorsPolicy>::get();;
  }

  void OnCertError(const GURL& main_frame_url,
                   SSLManager::CertError* error) {
    error->CancelRequest();
    ShowErrorPage(this, error);
    // No need to degrade our security indicators because we didn't continue.
  }
};

class NoRevocationMechanismPolicy : public SSLPolicy {
 public:
  static SSLPolicy* GetInstance() {
    return Singleton<NoRevocationMechanismPolicy>::get();
  }

  void OnCertError(const GURL& main_frame_url,
                   SSLManager::CertError* error) {
    // Silently ignore this error.
    error->ContinueRequest();
  }
};

class UnableToCheckRevocationPolicy : public SSLPolicy {
 public:
  static SSLPolicy* GetInstance() {
    return Singleton<UnableToCheckRevocationPolicy>::get();
  }

  void OnCertError(const GURL& main_frame_url,
                   SSLManager::CertError* error) {
    // We keep the style as secure and we display an info-bar.
    error->ContinueRequest();
    error->manager()->ShowMessage(l10n_util::GetString(
        IDS_CERT_ERROR_UNABLE_TO_CHECK_REVOCATION_INFO_BAR));
  }
};

class RevokedPolicy : public SSLPolicy {
 public:
  static SSLPolicy* GetInstance() {
    return Singleton<RevokedPolicy>::get();
  }

  void OnCertError(const GURL& main_frame_url,
                   SSLManager::CertError* error) {
    error->CancelRequest();
    DCHECK(error->GetTabContents()->type() == TAB_CONTENTS_WEB);
    ShowErrorPage(this, error);
    // No need to degrade our security indicators because we didn't continue.
  }
};

class InvalidPolicy : public SSLPolicy {
 public:
  static SSLPolicy* GetInstance() {
    return Singleton<InvalidPolicy>::get();;
  }

  void OnCertError(const GURL& main_frame_url,
                   SSLManager::CertError* error) {
    error->CancelRequest();
    DCHECK(error->GetTabContents()->type() == TAB_CONTENTS_WEB);
    ShowErrorPage(this, error);
    // No need to degrade our security indicators because we didn't continue.
  }
};

class DefaultPolicy : public SSLPolicy {
 public:
  DefaultPolicy() {
    // Load our helper classes to handle various cert errors.
    DCHECK(SubPolicyIndex(net::ERR_CERT_COMMON_NAME_INVALID) == 0);
    sub_policies_[0] = CommonNameInvalidPolicy::GetInstance();
    DCHECK(SubPolicyIndex(net::ERR_CERT_DATE_INVALID) == 1);
    sub_policies_[1] = DateInvalidPolicy::GetInstance();
    DCHECK(SubPolicyIndex(net::ERR_CERT_AUTHORITY_INVALID) == 2);
    sub_policies_[2] = AuthorityInvalidPolicy::GetInstance();
    DCHECK(SubPolicyIndex(net::ERR_CERT_CONTAINS_ERRORS) == 3);
    sub_policies_[3] = ContainsErrorsPolicy::GetInstance();
    DCHECK(SubPolicyIndex(net::ERR_CERT_NO_REVOCATION_MECHANISM) == 4);
    sub_policies_[4] = NoRevocationMechanismPolicy::GetInstance();
    DCHECK(SubPolicyIndex(net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION) == 5);
    sub_policies_[5] = UnableToCheckRevocationPolicy::GetInstance();
    DCHECK(SubPolicyIndex(net::ERR_CERT_REVOKED) == 6);
    sub_policies_[6] = RevokedPolicy::GetInstance();
    DCHECK(SubPolicyIndex(net::ERR_CERT_INVALID) == 7);
    sub_policies_[7] = InvalidPolicy::GetInstance();
    DCHECK(SubPolicyIndex(net::ERR_CERT_END) == 8);
  }

  void OnCertError(const GURL& main_frame_url,
                   SSLManager::CertError* error) {
    int index = SubPolicyIndex(error->cert_error());
    if (index < 0 || index >= arraysize(sub_policies_)) {
      NOTREACHED();
      error->CancelRequest();
      return;
    }

    if (error->resource_type() != ResourceType::MAIN_FRAME) {
      if (main_frame_url.SchemeIsSecure() &&
          !error->manager()->CanShowInsecureContent(main_frame_url)) {
        error->manager()->ShowMessageWithLink(
            l10n_util::GetString(IDS_SSL_INFO_BAR_FILTERED_CONTENT),
            l10n_util::GetString(IDS_SSL_INFO_BAR_SHOW_CONTENT),
            new ShowUnsafeContentTask(main_frame_url, error));
        error->DenyRequest();
      } else {
        // TODO (jcampan): if we get a bad HTTPS resource from a secure frame in
        // an insecure page, it might compromise any other page from the secure
        // frame domain, we should change their style to insecure, or just
        // filter the resource and show an info-bar.
        error->ContinueRequest();
      }
      return;
    }

    // First we check if we know the policy for this error.
    net::X509Certificate::Policy::Judgment judgment =
        error->manager()->QueryPolicy(error->ssl_info().cert,
                                      error->request_url().host());

    switch (judgment) {
    case net::X509Certificate::Policy::ALLOWED:
      // We've been told to allow this certificate.
      error->manager()->SetMaxSecurityStyle(
          SECURITY_STYLE_AUTHENTICATION_BROKEN);
      error->ContinueRequest();
      break;
    case net::X509Certificate::Policy::DENIED:
      // For now we handle the DENIED as the UNKNOWN, which means a blocking
      // page is shown to the user every time he comes back to the page.
    case net::X509Certificate::Policy::UNKNOWN:
      // We don't know how to handle this error.  Ask our sub-policies.
      sub_policies_[index]->OnCertError(main_frame_url, error);
      break;
    default:
      NOTREACHED();
    }
  }

  void OnMixedContent(NavigationController* navigation_controller,
                      const GURL& main_frame_url,
                      SSLManager::MixedContentHandler* mixed_content_handler) {
    PrefService* prefs = navigation_controller->profile()->GetPrefs();
    FilterPolicy::Type filter_policy = FilterPolicy::DONT_FILTER;
    if (!mixed_content_handler->manager()->
        CanShowInsecureContent(main_frame_url)) {
      filter_policy = FilterPolicy::FromInt(
            prefs->GetInteger(prefs::kMixedContentFiltering));
    }
    if (filter_policy != FilterPolicy::DONT_FILTER) {
      mixed_content_handler->manager()->ShowMessageWithLink(
          l10n_util::GetString(IDS_SSL_INFO_BAR_FILTERED_CONTENT),
          l10n_util::GetString(IDS_SSL_INFO_BAR_SHOW_CONTENT),
          new ShowUnsafeContentTask(main_frame_url, mixed_content_handler));
    }
    mixed_content_handler->StartRequest(filter_policy);

    NavigationEntry* entry = navigation_controller->GetActiveEntry();
    entry->SetHasMixedContent();
    navigation_controller->EntryUpdated(entry);
  }

  void OnDenyCertificate(SSLManager::CertError* error) {
    int index = SubPolicyIndex(error->cert_error());
    if (index < 0 || index >= arraysize(sub_policies_)) {
      NOTREACHED();
      return;
    }
    sub_policies_[index]->OnDenyCertificate(error);
  }

  void OnAllowCertificate(SSLManager::CertError* error) {
    int index = SubPolicyIndex(error->cert_error());
    if (index < 0 || index >= arraysize(sub_policies_)) {
      NOTREACHED();
      return;
    }
    sub_policies_[index]->OnAllowCertificate(error);
  }

 private:
  // Returns the index of the sub-policy for |cert_error| in the
  // sub_policies_ array.
  int SubPolicyIndex(int cert_error) {
    // Certificate errors are negative integers from net::ERR_CERT_BEGIN
    // (inclusive) to net::ERR_CERT_END (exclusive) in *decreasing* order.
    return net::ERR_CERT_BEGIN - cert_error;
  }
  SSLPolicy* sub_policies_[net::ERR_CERT_BEGIN - net::ERR_CERT_END];
};

} // namespace

SSLPolicy* SSLPolicy::GetDefaultPolicy() {
  // Lazily initialize our default policy instance.
  static SSLPolicy* default_policy = new DefaultPolicy();
  return default_policy;
}

SSLPolicy::SSLPolicy() {
}

void SSLPolicy::OnCertError(const GURL& main_frame_url,
                            SSLManager::CertError* error) {
  // Default to secure behavior.
  error->CancelRequest();
}

void SSLPolicy::OnRequestStarted(SSLManager* manager, const GURL& url,
                                 ResourceType::Type resource_type,
                                 int ssl_cert_id, int ssl_cert_status) {
  // These schemes never leave the browser and don't require a warning.
  if (url.SchemeIs("data") || url.SchemeIs("javascript") ||
      url.SchemeIs("about"))
    return;

  NavigationEntry* entry = manager->controller()->GetActiveEntry();
  if (!entry) {
    // We may not have an entry for cases such as the inspector.
    return;
  }

  if (!entry->GetURL().SchemeIsSecure() ||  // Current page is not secure.
      resource_type == ResourceType::MAIN_FRAME ||  // Main frame load.
      net::IsCertStatusError(entry->GetSSLCertStatus())) {  // There is already
          // an error for the main page, don't report sub-resources as unsafe
          // content.
    // No mixed/unsafe content check necessary.
    return;
  }

  if (url.SchemeIsSecure()) {
    // Check for insecure content (anything served over intranet is considered
    // insecure).

    // TODO(jcampan): bug #1178228 Disabling the broken style for intranet
    // hosts for beta as it is missing error strings (and cert status).
    // if (IsIntranetHost(url.host()) ||
    //    net::IsCertStatusError(ssl_cert_status)) {
    if (net::IsCertStatusError(ssl_cert_status)) {
      // The resource is unsafe.
      if (!entry->HasUnsafeContent()) {
        entry->SetHasUnsafeContent();
        manager->SetMaxSecurityStyle(SECURITY_STYLE_AUTHENTICATION_BROKEN);
      }
    }
  }

  // Note that when navigating to an inner-frame, we get this notification
  // before the new navigation entry is created.  For now we just copy the
  // mixed/unsafe content state from the old entry to the new one.  It is OK
  // to set the state on the wrong entry, as if we navigate back to it, its
  // state will be reset.

  // Now check for mixed content.
  if (entry->GetURL().SchemeIsSecure() && !url.SchemeIsSecure()) {
    entry->SetHasMixedContent();
    const std::wstring& msg = l10n_util::GetStringF(
        IDS_MIXED_CONTENT_LOG_MESSAGE,
        UTF8ToWide(entry->GetURL().spec()),
        UTF8ToWide(url.spec()));
    manager->AddMessageToConsole(msg, MESSAGE_LEVEL_WARNING);
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
