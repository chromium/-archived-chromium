// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/toolbar_model.h"

#include "chrome/browser/cert_store.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
#include "chrome/browser/tab_contents/tab_contents.h"
#elif defined(OS_POSIX)
// TODO(port): remove when tab_contents is ported
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

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
      url = entry->display_url();
    }
  }
  return gfx::GetCleanStringFromUrl(url, languages, NULL, NULL);
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
        DCHECK(entry->url().has_host());
        text->assign(l10n_util::GetStringF(IDS_SECURE_CONNECTION,
                                           UTF8ToWide(entry->url().host())));
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
                                       entry->url(),
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

