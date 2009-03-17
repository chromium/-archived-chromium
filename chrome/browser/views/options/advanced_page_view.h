// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_ADVANCED_PAGE_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_ADVANCED_PAGE_VIEW_H_

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/views/controls/button/native_button.h"

class AdvancedOptionsListModel;
class AdvancedScrollViewContainer;
class PrefService;

///////////////////////////////////////////////////////////////////////////////
// AdvancedPageView

class AdvancedPageView : public OptionsPageView,
                         public views::NativeButton::Listener {
 public:
  explicit AdvancedPageView(Profile* profile);
  virtual ~AdvancedPageView();

  // Resets all prefs to their default values.
  void ResetToDefaults();

  // views::NativeButton::Listener implementation:
  virtual void ButtonPressed(views::NativeButton* sender);

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();

 private:
  // Controls for the Advanced page
  AdvancedScrollViewContainer* advanced_scroll_view_;
  views::NativeButton* reset_to_default_button_;

  DISALLOW_EVIL_CONSTRUCTORS(AdvancedPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_ADVANCED_PAGE_VIEW_H_
