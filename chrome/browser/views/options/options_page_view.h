// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_PAGE_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_PAGE_VIEW_H__

#include "chrome/browser/options_page_base.h"
#include "views/controls/link.h"
#include "views/controls/button/native_button.h"

class PrefService;

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView
//
//  A base class for Options dialog pages that handles ensuring control
//  initialization is done just once.
//
class OptionsPageView : public views::View,
                        public OptionsPageBase {
 public:
  virtual ~OptionsPageView();

  // Returns true if the window containing this view can be closed, given the
  // current state of this view. This can be used to prevent the window from
  // being closed when a modal dialog box is showing, for example.
  virtual bool CanClose() const { return true; }

 protected:
  // This class cannot be instantiated directly, but its constructor must be
  // called by derived classes.
  explicit OptionsPageView(Profile* profile);

  // Initializes the layout of the controls within the panel.
  virtual void InitControlLayout() = 0;

  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  // Whether or not the control layout has been initialized for this page.
  bool initialized_;

  DISALLOW_EVIL_CONSTRUCTORS(OptionsPageView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_PAGE_VIEW_H__
