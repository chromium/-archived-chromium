// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFO_BAR_ALTERNATE_NAV_URL_VIEW_H__
#define CHROME_BROWSER_VIEWS_INFO_BAR_ALTERNATE_NAV_URL_VIEW_H__

#include "chrome/browser/views/info_bar_item_view.h"
#include "chrome/views/link.h"

class InfoBarAlternateNavURLView : public InfoBarItemView,
                                   public views::LinkController {
 public:
  explicit InfoBarAlternateNavURLView(const std::wstring& alternate_nav_url);
  virtual ~InfoBarAlternateNavURLView() { }

  // LinkController
  virtual void LinkActivated(views::Link* source, int event_flags);

 private:
  std::wstring alternate_nav_url_;

  DISALLOW_EVIL_CONSTRUCTORS(InfoBarAlternateNavURLView);
};

#endif // CHROME_BROWSER_VIEWS_INFO_BAR_ALTERNATE_NAV_URL_VIEW_H__
