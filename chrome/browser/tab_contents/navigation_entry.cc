// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/navigation_entry.h"

#include "chrome/common/url_constants.h"
#include "chrome/common/resource_bundle.h"

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

NavigationEntry::NavigationEntry(TabContentsType type)
    : unique_id_(GetUniqueID()),
      tab_type_(type),
      site_instance_(NULL),
      page_type_(NORMAL_PAGE),
      page_id_(-1),
      transition_type_(PageTransition::LINK),
      has_post_data_(false),
      restored_(false) {
}

NavigationEntry::NavigationEntry(TabContentsType type,
                                 SiteInstance* instance,
                                 int page_id,
                                 const GURL& url,
                                 const GURL& referrer,
                                 const std::wstring& title,
                                 PageTransition::Type transition_type)
    : unique_id_(GetUniqueID()),
      tab_type_(type),
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

const std::wstring& NavigationEntry::GetTitleForDisplay() {
  if (title_.empty())
    return display_url_as_string_;
  return title_;
}

bool NavigationEntry::IsViewSourceMode() const {
  return display_url_.SchemeIs(chrome::kViewSourceScheme);
}
