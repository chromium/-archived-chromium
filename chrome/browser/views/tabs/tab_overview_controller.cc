// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_overview_controller.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tabs/tab_overview_cell.h"
#include "chrome/browser/views/tabs/tab_overview_container.h"
#include "chrome/browser/views/tabs/tab_overview_grid.h"
#include "chrome/browser/window_sizer.h"
#include "views/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

static int kMonitorPadding = 20;
static int kInteriorPadding = 20;

TabOverviewController::TabOverviewController(
    const gfx::Point& monitor_origin)
    : host_(NULL),
      container_(NULL),
      grid_(NULL),
      browser_(NULL),
      drag_browser_(NULL),
      moved_offscreen_(false),
      shown_(false) {
  grid_ = new TabOverviewGrid(this);

  // Create the host.
  views::WidgetGtk* host = new views::WidgetGtk(views::WidgetGtk::TYPE_POPUP);
  host->set_delete_on_destroy(false);
  host->MakeTransparent();
  host->Init(NULL, gfx::Rect(), true);
  host_ = host;

  container_ = new TabOverviewContainer();
  container_->AddChildView(grid_);
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
  if (browser_)
    model()->RemoveObserver(this);
  host_->Close();
  // The drag controller may call back to us from it's destructor. Make sure
  // it's destroyed before us.
  grid()->CancelDrag();
}

void TabOverviewController::SetBrowser(Browser* browser) {
  if (browser_)
    model()->RemoveObserver(this);
  browser_ = browser;
  if (browser_)
    model()->AddObserver(this);
  moved_offscreen_ = false;
  RecreateCells();
}

TabStripModel* TabOverviewController::model() const {
  return browser_ ? browser_->tabstrip_model() : NULL;
}

void TabOverviewController::Show() {
  if (host_->IsVisible())
    return;

  shown_ = true;
  DCHECK(model());  // The model needs to be set before showing.
  host_->Show();
}

void TabOverviewController::ConfigureCell(TabOverviewCell* cell,
                                          TabContents* contents) {
  // TODO: need to set thumbnail here.
  if (contents) {
    cell->SetTitle(contents->GetTitle());
    cell->SetFavIcon(contents->GetFavIcon());
    cell->SchedulePaint();
  } else {
    // Need to figure out under what circumstances this is null and deal.
    NOTIMPLEMENTED();
  }
}

void TabOverviewController::DragStarted() {
  DCHECK(!drag_browser_);
  drag_browser_ = browser_;
  static_cast<BrowserWindowGtk*>(browser_->window())->set_drag_active(true);
}

void TabOverviewController::DragEnded() {
  static_cast<BrowserWindowGtk*>(drag_browser_->window())->
      set_drag_active(false);
  if (drag_browser_->tabstrip_model()->count() == 0)
    drag_browser_->tabstrip_model()->delegate()->CloseFrameAfterDragSession();
  drag_browser_ = NULL;
}

void TabOverviewController::MoveOffscreen() {
  gfx::Rect bounds;
  moved_offscreen_ = true;
  host_->GetBounds(&bounds, true);
  host_->SetBounds(gfx::Rect(-10000, -10000, bounds.width(), bounds.height()));
}

void TabOverviewController::SelectTabContents(TabContents* contents) {
  NOTIMPLEMENTED();
}

void TabOverviewController::TabInsertedAt(TabContents* contents,
                                          int index,
                                          bool foreground) {
  if (!grid_->modifying_model())
    grid_->CancelDrag();

  TabOverviewCell* child = new TabOverviewCell();
  ConfigureCell(child, index);
  grid_->InsertCell(index, child);

  TabCountChanged();
}

void TabOverviewController::TabClosingAt(TabContents* contents, int index) {
  // Nothing to do, we only care when the tab is actually detached.
}

void TabOverviewController::TabDetachedAt(TabContents* contents, int index) {
  if (!grid_->modifying_model())
    grid_->CancelDrag();

  scoped_ptr<TabOverviewCell> child(grid_->GetTabOverviewCellAt(index));
  grid_->RemoveCell(index);

  TabCountChanged();
}

void TabOverviewController::TabMoved(TabContents* contents,
                                     int from_index,
                                     int to_index) {
  if (!grid_->modifying_model())
    grid_->CancelDrag();

  grid_->MoveCell(from_index, to_index);
}

void TabOverviewController::TabChangedAt(TabContents* contents, int index,
                                         bool loading_only) {
  ConfigureCell(grid_->GetTabOverviewCellAt(index), index);
}

void TabOverviewController::TabStripEmpty() {
  if (!grid_->modifying_model()) {
    grid_->CancelDrag();
    // The tab strip is empty, hide the grid.
    host_->Hide();
  }
}

void TabOverviewController::ConfigureCell(TabOverviewCell* cell, int index) {
  ConfigureCell(cell, model()->GetTabContentsAt(index));
}

void TabOverviewController::RecreateCells() {
  grid_->RemoveAllChildViews(true);

  if (model()) {
    for (int i = 0; i < model()->count(); ++i) {
      TabOverviewCell* child = new TabOverviewCell();
      ConfigureCell(child, i);
      grid_->AddChildView(child);
    }
  }
  TabCountChanged();
}

void TabOverviewController::TabCountChanged() {
  if (moved_offscreen_)
    return;

  gfx::Size pref = container_->GetPreferredSize();
  int x = monitor_bounds_.x() + (monitor_bounds_.width() - pref.width()) / 2;
  int y = monitor_bounds_.y() + monitor_bounds_.height() / 2 - pref.height();
  host_->SetBounds(gfx::Rect(x, y, pref.width(), pref.height()));

  container_->UpdateWidgetShape(pref.width(), pref.height());
  if (grid()->GetChildViewCount() > 0) {
    if (shown_)
      host_->Show();
  } else {
    host_->Hide();
  }
}
