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

#include "chrome/browser/toolbar_model.h"

#include "chrome/browser/cert_store.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/ssl_error_info.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/common/gfx/url_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/base/net_util.h"

#include "generated_resources.h"

ToolbarModel::ToolbarModel() : input_in_progress_(false) {
}

ToolbarModel::~ToolbarModel() {
}

// ToolbarModel Implementation.
std::wstring ToolbarModel::GetText() {
  static const GURL kAboutBlankURL("about:blank");
  GURL url(kAboutBlankURL);
  std::wstring languages;  // Empty if we don't have a |navigation_controller|.

  NavigationController* navigation_controller = GetNavigationController();
  if (navigation_controller) {
    languages = navigation_controller->profile()->GetPrefs()->GetString(
                    prefs::kAcceptLanguages);
    NavigationEntry* entry = navigation_controller->GetActiveEntry();
    // We may not have a navigation entry yet
    if (!navigation_controller->active_contents()->ShouldDisplayURL()) {
      // Explicitly hide the URL for this tab.
      url = GURL();
    } else if (entry) {
      url = entry->GetDisplayURL();
    }
  }
  return gfx::ElideUrl(url, ChromeFont(), 0, languages);
}

ToolbarModel::SecurityLevel ToolbarModel::GetSecurityLevel() {
  if (input_in_progress_)  // When editing, assume no security style.
    return ToolbarModel::NORMAL;

  NavigationController* navigation_controller = GetNavigationController();
  if (!navigation_controller)  // We might not have a controller on init.
    return ToolbarModel::NORMAL;

  NavigationEntry* entry = navigation_controller->GetActiveEntry();
  if (!entry)
    return ToolbarModel::NORMAL;

  switch (entry->ssl().security_style()) {
    case SECURITY_STYLE_AUTHENTICATED:
      if (entry->ssl().has_mixed_content())
        return ToolbarModel::NORMAL;
      return ToolbarModel::SECURE;
    case SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return ToolbarModel::INSECURE;
    case SECURITY_STYLE_UNKNOWN:
    case SECURITY_STYLE_UNAUTHENTICATED:
      return ToolbarModel::NORMAL;
    default:
      NOTREACHED();
      return ToolbarModel::NORMAL;
  }
}

ToolbarModel::SecurityLevel ToolbarModel::GetSchemeSecurityLevel() {
  // For now, in sync with the security level.
  return GetSecurityLevel();
}

ToolbarModel::Icon ToolbarModel::GetIcon() {
  if (input_in_progress_)
    return ToolbarModel::NO_ICON;

  NavigationController* navigation_controller = GetNavigationController();
  if (!navigation_controller)  // We might not have a controller on init.
    return ToolbarModel::NO_ICON;

  NavigationEntry* entry = navigation_controller->GetActiveEntry();
  if (!entry)
    return ToolbarModel::NO_ICON;

  const NavigationEntry::SSLStatus& ssl = entry->ssl();
  switch (ssl.security_style()) {
    case SECURITY_STYLE_AUTHENTICATED:
      if (ssl.has_mixed_content())
        return ToolbarModel::WARNING_ICON;
      return ToolbarModel::LOCK_ICON;
    case SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return ToolbarModel::WARNING_ICON;
    case SECURITY_STYLE_UNKNOWN:
    case SECURITY_STYLE_UNAUTHENTICATED:
      return ToolbarModel::NO_ICON;
    default:
      NOTREACHED();
      return ToolbarModel::NO_ICON;
  }
}

void ToolbarModel::GetIconHoverText(std::wstring* text, SkColor* text_color) {
  static const SkColor kOKHttpsInfoBubbleTextColor =
      SkColorSetRGB(0, 153, 51);  // Green.
  static const SkColor kBrokenHttpsInfoBubbleTextColor =
      SkColorSetRGB(255, 0, 0);  // Red.

  DCHECK(text && text_color);

  NavigationController* navigation_controller = GetNavigationController();
  // We don't expect to be called during initialization, so the controller
  // should never be NULL.
  DCHECK(navigation_controller);
  NavigationEntry* entry = navigation_controller->GetActiveEntry();
  DCHECK(entry);

  
  const NavigationEntry::SSLStatus& ssl = entry->ssl();
  switch (ssl.security_style()) {
    case SECURITY_STYLE_AUTHENTICATED: {
      if (ssl.has_mixed_content()) {
        SSLErrorInfo error_info =
            SSLErrorInfo::CreateError(SSLErrorInfo::MIXED_CONTENTS,
                                      NULL, GURL::EmptyGURL());
        text->assign(error_info.short_description());
        *text_color = kBrokenHttpsInfoBubbleTextColor;
      } else {
        const GURL& url = entry->GetURL();
        DCHECK(url.has_host());
        text->assign(l10n_util::GetStringF(IDS_SECURE_CONNECTION,
                                           UTF8ToWide(url.host())));
        *text_color = kOKHttpsInfoBubbleTextColor;
      }
      break;
    }
    case SECURITY_STYLE_AUTHENTICATION_BROKEN: {
      CreateErrorText(entry, text);
      if (text->empty()) {
        // If the authentication is broken, we should always have at least one
        // error.
        NOTREACHED();
        return;
      }
      *text_color = kBrokenHttpsInfoBubbleTextColor;
      break;
    }
    default:
      // Don't show the info bubble in any other cases.
      text->clear();
      break;
  }
}

void ToolbarModel::GetInfoText(std::wstring* text,
                               SkColor* text_color,
                               std::wstring* tooltip) {
  static const SkColor kEVTextColor =
      SkColorSetRGB(0, 150, 20);  // Green.

  DCHECK(text && tooltip);
  text->clear();
  tooltip->clear();

  NavigationController* navigation_controller = GetNavigationController();
  if (!navigation_controller)  // We might not have a controller on init.
    return;

  NavigationEntry* entry = navigation_controller->GetActiveEntry();
  const NavigationEntry::SSLStatus& ssl = entry->ssl();
  if (!entry || ssl.has_mixed_content() ||
      net::IsCertStatusError(ssl.cert_status()) ||
      ((ssl.cert_status() & net::CERT_STATUS_IS_EV) == 0))
    return;

  scoped_refptr<net::X509Certificate> cert;
  CertStore::GetSharedInstance()->RetrieveCert(ssl.cert_id(), &cert);
  if (!cert.get()) {
    NOTREACHED();
    return;
  }

  *text_color = kEVTextColor;
  SSLManager::GetEVCertNames(*cert, text, tooltip);
}

void ToolbarModel::CreateErrorText(NavigationEntry* entry, std::wstring* text) {
  const NavigationEntry::SSLStatus& ssl = entry->ssl();
  std::vector<SSLErrorInfo> errors;
  SSLErrorInfo::GetErrorsForCertStatus(ssl.cert_id(),
                                       ssl.cert_status(),
                                       entry->GetURL(),
                                       &errors);
  if (ssl.has_mixed_content()) {
    errors.push_back(SSLErrorInfo::CreateError(SSLErrorInfo::MIXED_CONTENTS,
                                               NULL, GURL::EmptyGURL()));
  }
  if (ssl.has_unsafe_content()) {
   errors.push_back(SSLErrorInfo::CreateError(SSLErrorInfo::UNSAFE_CONTENTS,
                                              NULL, GURL::EmptyGURL()));
  }

  int error_count = static_cast<int>(errors.size());
  if (error_count == 0) {
    text->assign(L"");
  } else if (error_count == 1) {
    text->assign(errors[0].short_description());
  } else {
    // Multiple errors.
    text->assign(l10n_util::GetString(IDS_SEVERAL_SSL_ERRORS));
    text->append(L"\n");
    for (int i = 0; i < error_count; ++i) {
      text->append(errors[i].short_description());
      if (i != error_count - 1)
        text->append(L"\n");
    }
  }
}
