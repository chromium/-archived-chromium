// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/navigation_entry.h"

#include "app/resource_bundle.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/app_resources.h"
#include "net/base/net_util.h"

// Use this to get a new unique ID for a NavigationEntry during construction.
// The returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
static int GetUniqueID() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

NavigationEntry::SSLStatus::SSLStatus()
    : security_style_(SECURITY_STYLE_UNKNOWN),
      cert_id_(0),
      cert_status_(0),
      security_bits_(-1),
      content_status_(NORMAL_CONTENT) {
}

NavigationEntry::FaviconStatus::FaviconStatus() : valid_(false) {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  bitmap_ = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
}


NavigationEntry::NavigationEntry()
    : unique_id_(GetUniqueID()),
      site_instance_(NULL),
      page_type_(NORMAL_PAGE),
      page_id_(-1),
      transition_type_(PageTransition::LINK),
      has_post_data_(false),
      restored_(false) {
}

NavigationEntry::NavigationEntry(SiteInstance* instance,
                                 int page_id,
                                 const GURL& url,
                                 const GURL& referrer,
                                 const string16& title,
                                 PageTransition::Type transition_type)
    : unique_id_(GetUniqueID()),
      site_instance_(instance),
      page_type_(NORMAL_PAGE),
      url_(url),
      referrer_(referrer),
      title_(title),
      page_id_(page_id),
      transition_type_(transition_type),
      has_post_data_(false),
      restored_(false) {
}

const string16& NavigationEntry::GetTitleForDisplay(
    const NavigationController* navigation_controller) {
  // Most pages have real titles. Don't even bother caching anything if this is
  // the case.
  if (!title_.empty())
    return title_;

  // More complicated cases will use the URLs as the title. This result we will
  // cache since it's more complicated to compute.
  if (!cached_display_title_.empty())
    return cached_display_title_;

  // Use the display URL first if any, and fall back on using the real URL.
  std::wstring languages;
  if (navigation_controller) {
      languages = navigation_controller->profile()->GetPrefs()->GetString(
          prefs::kAcceptLanguages);
  }
  if (!display_url_.is_empty()) {
    cached_display_title_ = WideToUTF16Hack(net::FormatUrl(
        display_url_, languages));
  } else if (!url_.is_empty()) {
    cached_display_title_ = WideToUTF16Hack(net::FormatUrl(url_, languages));
  }
  return cached_display_title_;
}

bool NavigationEntry::IsViewSourceMode() const {
  return display_url_.SchemeIs(chrome::kViewSourceScheme);
}
