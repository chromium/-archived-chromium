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

#include "chrome/browser/views/info_bar_alternate_nav_url_view.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/info_bar_view.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/label.h"

#include "generated_resources.h"

InfoBarAlternateNavURLView::InfoBarAlternateNavURLView(
    const std::wstring& alternate_nav_url)
    : alternate_nav_url_(alternate_nav_url) {
  size_t offset;
  const std::wstring label(l10n_util::GetStringF(
      IDS_ALTERNATE_NAV_URL_VIEW_LABEL, std::wstring(), &offset));
  DCHECK(offset != std::wstring::npos);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (offset > 0) {
    ChromeViews::Label* label_1 =
        new ChromeViews::Label(label.substr(0, offset));
    label_1->SetFont(rb.GetFont(ResourceBundle::MediumFont));
    AddChildViewLeading(label_1, 0);
  }

  ChromeViews::Link* link = new ChromeViews::Link(alternate_nav_url_);
  link->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  link->SetController(this);
  AddChildViewLeading(link, 0);

  if (offset < label.length()) {
    ChromeViews::Label* label_2 = new ChromeViews::Label(label.substr(offset));
    label_2->SetFont(rb.GetFont(ResourceBundle::MediumFont));
    AddChildViewLeading(label_2, 0);
  }

  SetIcon(*rb.GetBitmapNamed(IDR_INFOBAR_ALT_NAV_URL));
}

void InfoBarAlternateNavURLView::LinkActivated(ChromeViews::Link* source,
                                               int event_flags) {
  // Navigating may or may not automatically close the infobar, depending on
  // whether the desired disposition replaces the current tab.  We always want
  // the bar to close, so we close it ourselves before navigating (doing things
  // in the other order would be problematic if navigation synchronously closed
  // the bar, as on return from the call, |this| would not exist, and calling
  // Close() would corrupt memory).  This means we need to save off all members
  // we need before calling Close(), which destroys us.
  PageNavigator* const navigator =
      static_cast<InfoBarView*>(GetParent())->web_contents();
  const GURL gurl(alternate_nav_url_);

  BeginClose();

  navigator->OpenURL(gurl, event_utils::DispositionFromEventFlags(event_flags),
                     // Pretend the user typed this URL, so that navigating to
                     // it will be the default action when it's typed again in
                     // the future.
                     PageTransition::TYPED);
}
