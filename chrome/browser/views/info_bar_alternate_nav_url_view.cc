// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/info_bar_alternate_nav_url_view.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/info_bar_view.h"
#include "chrome/browser/views/standard_layout.h"
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
    views::Label* label_1 = new views::Label(label.substr(0, offset));
    label_1->SetFont(rb.GetFont(ResourceBundle::MediumFont));
    AddChildViewLeading(label_1, 0);
  }

  views::Link* link = new views::Link(alternate_nav_url_);
  link->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  link->SetController(this);
  AddChildViewLeading(link, 0);

  if (offset < label.length()) {
    views::Label* label_2 = new views::Label(label.substr(offset));
    label_2->SetFont(rb.GetFont(ResourceBundle::MediumFont));
    AddChildViewLeading(label_2, 0);
  }

  SetIcon(*rb.GetBitmapNamed(IDR_INFOBAR_ALT_NAV_URL));
}

void InfoBarAlternateNavURLView::LinkActivated(views::Link* source,
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

  navigator->OpenURL(gurl, GURL(),
                     event_utils::DispositionFromEventFlags(event_flags),
                     // Pretend the user typed this URL, so that navigating to
                     // it will be the default action when it's typed again in
                     // the future.
                     PageTransition::TYPED);
}

