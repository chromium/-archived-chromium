// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/new_browser_window_widget.h"

#include "app/resource_bundle.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "chrome/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

NewBrowserWindowWidget::NewBrowserWindowWidget(Profile* profile)
    : profile_(profile),
      widget_(NULL) {
  views::ImageButton* button = new views::ImageButton(this);
  button->SetImage(views::CustomButton::BS_NORMAL,
                   ResourceBundle::GetSharedInstance().GetBitmapNamed(
                       IDR_NEW_BROWSER_WINDOW_ICON));
  gfx::Size pref = button->GetPreferredSize();
  views::WidgetGtk* widget =
      new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  widget->MakeTransparent();
  widget->Init(NULL, gfx::Rect(0, 0, pref.width(), pref.height()), false);
  TabOverviewTypes::instance()->SetWindowType(
      widget->GetNativeView(),
      TabOverviewTypes::WINDOW_TYPE_CREATE_BROWSER_WINDOW,
      NULL);
  widget->GetRootView()->SetLayoutManager(new views::FillLayout());
  widget->GetRootView()->AddChildView(button);
  widget_ = widget;
  widget->Show();
}

NewBrowserWindowWidget::~NewBrowserWindowWidget() {
  widget_->Close();
  widget_ = NULL;
}

void NewBrowserWindowWidget::ButtonPressed(views::Button* sender) {
  UserMetrics::RecordAction(L"TabOverview_PressedCreateNewBrowserButton",
                            profile_);

  Browser* browser = Browser::Create(profile_);
  browser->AddTabWithURL(GURL(), GURL(), PageTransition::START_PAGE,
                         true, -1, false, NULL);
  browser->window()->Show();
}
