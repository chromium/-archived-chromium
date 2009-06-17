// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info_window.h"

#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

PageInfoWindow::PageInfoWindow() : cert_id_(0) {
}

PageInfoWindow::~PageInfoWindow() {
}

#if defined(OS_LINUX)
// TODO(rsesek): Remove once we have a PageInfoWindowLinux implementation
PageInfoWindow* PageInfoWindow::Factory() {
  NOTIMPLEMENTED();
  return NULL;
}
#endif

// static
void PageInfoWindow::CreatePageInfo(Profile* profile,
                                    NavigationEntry* nav_entry,
                                    gfx::NativeView parent,
                                    PageInfoWindow::TabID tab) {
  PageInfoWindow* window = Factory();
  window->Init(profile, nav_entry->url(), nav_entry->ssl(),
               nav_entry->page_type(), true, parent);
  window->Show();
}

// static
void PageInfoWindow::CreateFrameInfo(Profile* profile,
                                     const GURL& url,
                                     const NavigationEntry::SSLStatus& ssl,
                                     gfx::NativeView parent,
                                     TabID tab) {
  PageInfoWindow* window = Factory();
  window->Init(profile, url, ssl, NavigationEntry::NORMAL_PAGE,
               false, parent);
  window->Show();
}

// static
void PageInfoWindow::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kPageInfoWindowPlacement);
}

// static
std::string PageInfoWindow::GetIssuerName(
    const net::X509Certificate::Principal& issuer) {
  if (!issuer.common_name.empty())
    return issuer.common_name;
  if (!issuer.organization_names.empty())
    return issuer.organization_names[0];
  if (!issuer.organization_unit_names.empty())
    return issuer.organization_unit_names[0];

  return std::string();
}
