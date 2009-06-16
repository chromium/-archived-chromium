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
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "chrome/browser/window_sizer.h"
#include "views/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

// Horizontal padding from the edge of the monitor to the overview.
static int kMonitorPadding = 20;
// Vertical padding between the overview and the windows along on the bottom.
static int kWindowToOverviewPadding = 25;
// Height of the windows along the bottom, as a percentage of the monitors
// height.
static float kWindowHeight = .30;
// Height of the tab overview, as a percentage of monitors height.
static float kOverviewHeight = .55;

TabOverviewController::TabOverviewController(
    const gfx::Point& monitor_origin)
    : host_(NULL),
      container_(NULL),
      grid_(NULL),
      browser_(NULL),
      drag_browser_(NULL),
      moved_offscreen_(false),
      shown_(false),
      horizontal_center_(0),
      change_window_bounds_on_animate_(false),
      mutating_grid_(false) {
  grid_ = new TabOverviewGrid(this);

  // Create the host.
  views::WidgetGtk* host = new views::WidgetGtk(views::WidgetGtk::TYPE_POPUP);
  host->set_delete_on_destroy(false);
  host->MakeTransparent();
  host->Init(NULL, gfx::Rect(), true);
  TabOverviewTypes::instance()->SetWindowType(
      host->GetNativeView(),
      TabOverviewTypes::WINDOW_TYPE_CHROME_TAB_SUMMARY,
      NULL);
  host_ = host;

  container_ = new TabOverviewContainer();
  container_->AddChildView(grid_);
  host->GetRootView()->SetLayoutManager(new views::FillLayout());
  host->GetRootView()->AddChildView(container_);

  // Determine the max size for the overview.
  scoped_ptr<WindowSizer::MonitorInfoProvider> provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  monitor_bounds_ = provider->GetMonitorWorkAreaMatching(
      gfx::Rect(monitor_origin.x(), monitor_origin.y(), 1, 1));
  monitor_bounds_.Inset(kMonitorPadding, 0);
  int max_width = monitor_bounds_.width();
  int max_height = static_cast<int>(monitor_bounds_.height() *
                                    kOverviewHeight);
  container_->SetMaxSize(gfx::Size(max_width, max_height));

  // TODO: remove this when we get mid point.
  horizontal_center_ = monitor_bounds_.x() + monitor_bounds_.width() / 2;
}

TabOverviewController::~TabOverviewController() {
  if (browser_)
    model()->RemoveObserver(this);
  host_->Close();
  // The drag controller may call back to us from it's destructor. Make sure
  // it's destroyed before us.
  grid()->CancelDrag();
}

void TabOverviewController::SetBrowser(Browser* browser,
                                       int horizontal_center) {
  // TODO: update this when we horizontal_center is correct.
  // horizontal_center_ = horizontal_center;
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

void TabOverviewController::GridAnimationEnded() {
  if (moved_offscreen_ || !change_window_bounds_on_animate_ || mutating_grid_)
    return;

  SetHostBounds(target_bounds_);
  grid_->UpdateDragController();
  change_window_bounds_on_animate_ = false;
}

void TabOverviewController::GridAnimationProgressed() {
  if (moved_offscreen_ || !change_window_bounds_on_animate_)
    return;

  DCHECK(!mutating_grid_);

  SetHostBounds(grid_->AnimationPosition(start_bounds_, target_bounds_));
  grid_->UpdateDragController();
}

void TabOverviewController::GridAnimationCanceled() {
  change_window_bounds_on_animate_ = false;
}

void TabOverviewController::TabInsertedAt(TabContents* contents,
                                          int index,
                                          bool foreground) {
  if (!grid_->modifying_model())
    grid_->CancelDrag();

  TabOverviewCell* child = new TabOverviewCell();
  ConfigureCell(child, index);
  mutating_grid_ = true;
  grid_->InsertCell(index, child);
  mutating_grid_ = false;

  UpdateStartAndTargetBounds();
}

void TabOverviewController::TabClosingAt(TabContents* contents, int index) {
  // Nothing to do, we only care when the tab is actually detached.
}

void TabOverviewController::TabDetachedAt(TabContents* contents, int index) {
  if (!grid_->modifying_model())
    grid_->CancelDrag();

  scoped_ptr<TabOverviewCell> child(grid_->GetTabOverviewCellAt(index));
  mutating_grid_ = true;
  grid_->RemoveCell(index);
  mutating_grid_ = false;

  UpdateStartAndTargetBounds();
}

void TabOverviewController::TabMoved(TabContents* contents,
                                     int from_index,
                                     int to_index) {
  if (!grid_->modifying_model())
    grid_->CancelDrag();

  mutating_grid_ = true;
  grid_->MoveCell(from_index, to_index);
  mutating_grid_ = false;

  UpdateStartAndTargetBounds();
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

  if (moved_offscreen_)
    return;

  SetHostBounds(CalculateHostBounds());
  if (grid()->GetChildViewCount() > 0) {
    if (shown_)
      host_->Show();
  } else {
    host_->Hide();
  }
}

void TabOverviewController::UpdateStartAndTargetBounds() {
  if (moved_offscreen_ || !shown_)
    return;

  if (grid()->GetChildViewCount() == 0) {
    host_->Hide();
  } else {
    host_->GetBounds(&start_bounds_, true);
    target_bounds_ = CalculateHostBounds();
    change_window_bounds_on_animate_ = (start_bounds_ != target_bounds_);
  }
}

void TabOverviewController::SetHostBounds(const gfx::Rect& bounds) {
  host_->SetBounds(bounds);
  container_->UpdateWidgetShape(horizontal_center_, bounds.width(),
                                bounds.height());
}

gfx::Rect TabOverviewController::CalculateHostBounds() {
  gfx::Size pref = container_->GetPreferredSize();
  int x = horizontal_center_ - pref.width() / 2;
  int y = monitor_bounds_.bottom() -
      static_cast<int>(monitor_bounds_.height() * kWindowHeight) -
      kWindowToOverviewPadding - pref.height();
  return gfx::Rect(x, y, pref.width(), pref.height()).
      AdjustToFit(monitor_bounds_);
}
