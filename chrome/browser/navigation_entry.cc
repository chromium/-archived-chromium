// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_entry.h"

#include "chrome/common/resource_bundle.h"

int NavigationEntry::unique_id_counter_ = 0;

NavigationEntry::SSLStatus::SSLStatus()
    : security_style_(SECURITY_STYLE_UNKNOWN),
      cert_id_(0),
      cert_status_(0),
      security_bits_(-1),
      content_status_(NORMAL_CONTENT) {
}

NavigationEntry::NavigationEntry(TabContentsType type)
    : type_(type),
      unique_id_(GetUniqueID()),
      site_instance_(NULL),
      page_id_(-1),
      transition_type_(PageTransition::LINK),
      page_type_(NORMAL_PAGE),
      valid_fav_icon_(false),
      has_post_data_(false),
      restored_(false) {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  favicon_ = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
}

NavigationEntry::NavigationEntry(TabContentsType type,
                                 SiteInstance* instance,
                                 int page_id,
                                 const GURL& url,
                                 const std::wstring& title,
                                 PageTransition::Type transition_type)
    : type_(type),
      unique_id_(GetUniqueID()),
      site_instance_(instance),
      page_id_(page_id),
      url_(url),
      title_(title),
      transition_type_(transition_type),
      page_type_(NORMAL_PAGE),
      valid_fav_icon_(false),
      has_post_data_(false),
      restored_(false) {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  favicon_ = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
}


void NavigationEntry::SetSiteInstance(SiteInstance* site_instance) {
  // Note that the SiteInstance should usually not be changed after it is set,
  // but this may happen if the NavigationEntry was cloned and needs to use a
  // different SiteInstance.

  if (site_instance_ == site_instance) {
    // No change necessary.
    return;
  }

  // The ref counting is handled automatically, since these are scoped_refptrs.
  site_instance_ = site_instance;
}

void NavigationEntry::SetContentState(const std::string& state) {
  state_ = state;
}

int NavigationEntry::GetUniqueID() {
  // Never return 0, as that is the "no ID" value.
  do {
    ++unique_id_counter_;
  } while (!unique_id_counter_);
  return unique_id_counter_;
}

