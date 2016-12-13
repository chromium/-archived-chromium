// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_ADVANCED_CONTENTS_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_ADVANCED_CONTENTS_VIEW_H__

#include "chrome/browser/views/options/options_page_view.h"

class AdvancedContentsView;
namespace views {
class ScrollView;
}

///////////////////////////////////////////////////////////////////////////////
// AdvancedScrollViewContainer
//
//  A View that contains a scroll view containing the Advanced options.

class AdvancedScrollViewContainer : public views::View {
 public:
  explicit AdvancedScrollViewContainer(Profile* profile);
  virtual ~AdvancedScrollViewContainer();

  // views::View overrides:
  virtual void Layout();

 private:
  // The contents of the advanced scroll view.
  AdvancedContentsView* contents_view_;

  // The scroll view that contains the advanced options.
  views::ScrollView* scroll_view_;

  DISALLOW_EVIL_CONSTRUCTORS(AdvancedScrollViewContainer);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_ADVANCED_CONTENTS_VIEW_H__
