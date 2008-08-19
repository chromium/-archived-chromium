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
