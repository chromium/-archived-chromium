// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_overview_controller.h"

#include "chrome/browser/views/tabs/tab_overview_container.h"
#include "chrome/browser/window_sizer.h"
#include "views/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

static int kMonitorPadding = 20;
static int kInteriorPadding = 20;

TabOverviewController::TabOverviewController(
    const gfx::Point& monitor_origin)
    : model_(NULL) {
  view_ = new TabOverviewGrid();
  view_->set_host(this);

  // Create the host.
  views::WidgetGtk* host = new views::WidgetGtk(views::WidgetGtk::TYPE_POPUP);
  host->set_delete_on_destroy(false);
  host->MakeTransparent();
  host->Init(NULL, gfx::Rect(), true);
  host_.reset(host);

  container_ = new TabOverviewContainer();
  container_->AddChildView(view_);
  host->GetRootView()->SetLayoutManager(new views::FillLayout());
  host->GetRootView()->AddChildView(container_);

  // Determine the bounds we're going to show at.
  scoped_ptr<WindowSizer::MonitorInfoProvider> provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  monitor_bounds_ = provider->GetMonitorWorkAreaMatching(
      gfx::Rect(monitor_origin.x(), monitor_origin.y(), 1, 1));
  int max_width = monitor_bounds_.width() - kMonitorPadding * 2 -
                  kInteriorPadding * 2;
  int max_height = monitor_bounds_.height() / 2;
  container_->SetMaxSize(gfx::Size(max_width, max_height));
}

TabOverviewController::~TabOverviewController() {
}

void TabOverviewController::SetTabStripModel(TabStripModel* tab_strip_model) {
  model_ = tab_strip_model;
  view_->SetTabStripModel(tab_strip_model);
}

void TabOverviewController::Show() {
  if (host_->IsVisible())
    return;

  DCHECK(view_->model());  // The model needs to be set before showing.
  host_->Show();
}

void TabOverviewController::Hide() {
  host_->Hide();
}

void TabOverviewController::TabOverviewGridPreferredSizeChanged() {
  gfx::Size pref = container_->GetPreferredSize();
  int x = monitor_bounds_.x() + (monitor_bounds_.width() - pref.width()) / 2;
  int y = monitor_bounds_.y() + monitor_bounds_.height() / 2 - pref.height();
  host_->SetBounds(gfx::Rect(x, y, pref.width(), pref.height()));

  container_->UpdateWidgetShape(pref.width(), pref.height());
}

void TabOverviewController::SelectTabContents(TabContents* contents) {
  NOTIMPLEMENTED();
}
