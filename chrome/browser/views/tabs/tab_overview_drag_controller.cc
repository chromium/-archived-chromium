// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_overview_drag_controller.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/dock_info.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tabs/tab_overview_cell.h"
#include "chrome/browser/views/tabs/tab_overview_controller.h"
#include "chrome/browser/views/tabs/tab_overview_grid.h"
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "chrome/common/notification_service.h"
#include "views/fill_layout.h"
#include "views/view.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

TabOverviewDragController::TabOverviewDragController(
    TabOverviewController* controller)
    : controller_(controller),
      original_model_(controller->model()),
      current_index_(-1),
      original_index_(-1),
      detached_tab_(NULL),
      original_delegate_(NULL),
      x_offset_(0),
      y_offset_(0),
      dragging_(false),
      modifying_model_(false),
      detached_window_(NULL),
      hidden_browser_(NULL),
      mouse_over_mini_window_(false) {
}

TabOverviewDragController::~TabOverviewDragController() {
  if (dragging_)
    controller_->DragEnded();
  if (original_index_ != -1)
    RevertDrag(false);
}

bool TabOverviewDragController::Configure(const gfx::Point& location) {
  // Find the cell the user clicked on.
  TabOverviewCell* cell = NULL;
  int index = -1;
  for (int i = 0; i < grid()->GetChildViewCount(); ++i) {
    views::View* child = grid()->GetChildViewAt(i);
    if (child->bounds().Contains(location)) {
      index = i;
      cell = static_cast<TabOverviewCell*>(child);
      break;
    }
  }
  if (!cell)
    return false;  // User didn't click on a cell.

  // Only start a drag if the user clicks on the thumbnail.
  gfx::Point cell_point(location);
  grid()->ConvertPointToView(grid(), cell, &cell_point);
  if (!cell->IsPointInThumbnail(cell_point))
    return false;

  current_index_ = original_index_ = index;
  origin_ = location;
  x_offset_ = location.x() - cell->bounds().x();
  y_offset_ = location.y() - cell->bounds().y();

  // Ask the controller to select the cell.
  controller_->SelectTab(index);

  if (controller_->browser()) {
    browser_window_size_ =
        controller_->browser()->window()->GetNormalBounds().size();
  }

  return true;
}

void TabOverviewDragController::Drag(const gfx::Point& location) {
  if (original_index_ == -1)
    return;

  if (!dragging_ &&
      views::View::ExceededDragThreshold(location.x() - origin_.x(),
                                         location.y() - origin_.y())) {
    // Start dragging.
    dragging_ = true;
    controller_->DragStarted();
    grid()->set_floating_index(current_index_);
  }
  if (dragging_)
    DragCell(location);
}

void TabOverviewDragController::CommitDrag(const gfx::Point& location) {
  if (original_index_ == -1)
    return;

  Drag(location);
  if (detached_tab_) {
    if (mouse_over_mini_window_) {
      // Dragged over a mini window, add as the last tab to the browser.
      Attach(model()->count());
    } else {
      DropTab(location);
    }
  } else if (!dragging_ ) {
    // We haven't started dragging. Tell the controller to focus the browser.
    controller_->FocusBrowser();
  } else {
    // The tab is already in position, nothing to do but animate the change.
    grid()->set_floating_index(-1);
    grid()->AnimateToTargetBounds();
  }

  // Set the index to -1 so we know not to do any cleanup.
  original_index_ = -1;
}

void TabOverviewDragController::RevertDrag(bool tab_destroyed) {
  if (original_index_ == -1)
    return;

  modifying_model_ = true;
  if (detached_tab_) {
    // Tab is currently detached, add it back to the original tab strip.
    if (!tab_destroyed) {
      original_model_->InsertTabContentsAt(original_index_,
                                           detached_tab_, true, false);
    }
    SetDetachedContents(NULL);
    detached_window_->Close();
    detached_window_ = NULL;

    if (hidden_browser_) {
      gtk_widget_show(GTK_WIDGET(static_cast<BrowserWindowGtk*>(
          hidden_browser_->window())->GetNativeHandle()));
    }
  } else if (original_model_ != model() && !tab_destroyed) {
    // The tab was added to a different tab strip. Move it back to the
    // original.
    TabContents* contents = model()->DetachTabContentsAt(current_index_);
    original_model_->InsertTabContentsAt(original_index_, contents, true,
                                         false);
  } else if (current_index_ != original_index_ && !tab_destroyed) {
    original_model_->MoveTabContentsAt(current_index_, original_index_, true);
  }
  modifying_model_ = false;

  // Set the index to -1 so we know not to do any cleanup.
  original_index_ = -1;
}

TabOverviewGrid* TabOverviewDragController::grid() const {
  return controller_->grid();
}

TabStripModel* TabOverviewDragController::model() const {
  return controller_->model();
}

void TabOverviewDragController::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  DCHECK(Source<TabContents>(source).ptr() == detached_tab_);
  RevertDrag(true);
}

void TabOverviewDragController::OpenURLFromTab(
    TabContents* source,
    const GURL& url,
    const GURL& referrer,
    WindowOpenDisposition disposition,
    PageTransition::Type transition) {
  if (original_delegate_) {
    if (disposition == CURRENT_TAB)
      disposition = NEW_WINDOW;

    original_delegate_->OpenURLFromTab(source, url, referrer,
                                       disposition, transition);
  }
}

void TabOverviewDragController::NavigationStateChanged(
    const TabContents* source,
    unsigned changed_flags) {
}

void TabOverviewDragController::AddNewContents(
    TabContents* source,
    TabContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  DCHECK(disposition != CURRENT_TAB);

  // Theoretically could be called while dragging if the page tries to
  // spawn a window. Route this message back to the browser in most cases.
  if (original_delegate_) {
    original_delegate_->AddNewContents(source, new_contents, disposition,
                                       initial_pos, user_gesture);
  }
}

void TabOverviewDragController::ActivateContents(TabContents* contents) {
  // Ignored.
}

void TabOverviewDragController::LoadingStateChanged(TabContents* source) {
}

void TabOverviewDragController::CloseContents(TabContents* source) {
  // Theoretically could be called by a window. Should be ignored
  // because window.close() is ignored (usually, even though this
  // method gets called.)
}

void TabOverviewDragController::MoveContents(TabContents* source,
                                             const gfx::Rect& pos) {
  // Theoretically could be called by a web page trying to move its
  // own window. Should be ignored since we're moving the window...
}

bool TabOverviewDragController::IsPopup(TabContents* source) {
  return false;
}

void TabOverviewDragController::ToolbarSizeChanged(TabContents* source,
                                                   bool finished) {
  // Dragged tabs don't care about this.
}

void TabOverviewDragController::URLStarredChanged(TabContents* source,
                                                  bool starred) {
  // Ignored.
}

void TabOverviewDragController::UpdateTargetURL(TabContents* source,
                                                const GURL& url) {
  // Ignored.
}

void TabOverviewDragController::DragCell(const gfx::Point& location) {
  if (controller_->moved_offscreen()) {
    MoveDetachedWindow(location);
    return;
  }

  int col = (location.x() - x_offset_ + grid()->cell_width() / 2) /
        (grid()->cell_width() + TabOverviewGrid::kCellXPadding);
  int row = (location.y() - y_offset_ + grid()->cell_height() / 2) /
        (grid()->cell_height() + TabOverviewGrid::kCellYPadding);
  gfx::Rect local_bounds = grid()->GetLocalBounds(true);
  if (!local_bounds.Contains(location)) {
    // Local bounds doesn't contain the point, allow dragging to the left/right
    // of us.
    views::View* root = grid()->GetParent();
    gfx::Rect allowed_bounds = local_bounds;
    gfx::Point root_offset;
    views::View::ConvertPointToView(grid(), root, &root_offset);
    allowed_bounds.Offset(-root_offset.x(), 0);
    allowed_bounds.set_width(root->width());
    if (!allowed_bounds.Contains(location)) {
      // The user dragged outside the grid.
      if (detached_tab_) {
        // We've already created the detached window, move it.
        MoveDetachedWindow(location);
      } else {
        // Detach the cell.
        Detach(location);
      }
      return;
    }
    if (location.x() < 0) {
      col = 0;
    } else if (location.x() >= grid()->width()) {
      col = grid()->columns();
    } else {
      col = (location.x() + grid()->cell_width() / 2) /
          (grid()->cell_width() + TabOverviewGrid::kCellXPadding);
    }
  }
  int new_index = std::min(model()->count() - 1,
                           row * grid()->columns() + col);
  if (detached_tab_) {
    // The user dragged a detached tab back over the grid, reattach it.
    Attach(new_index);
  } else if (new_index != current_index_) {
    grid()->set_floating_index(new_index);
    modifying_model_ = true;
    model()->MoveTabContentsAt(current_index_, new_index, false);
    modifying_model_ = false;
    current_index_ = new_index;
  }
  gfx::Rect target_bounds = grid()->CellBounds(current_index_);
  target_bounds.Offset(location.x() - target_bounds.x() - x_offset_,
                       location.y() - target_bounds.y() - y_offset_);
  target_bounds.set_y(row * (grid()->cell_height() +
                             TabOverviewGrid::kCellYPadding));
  target_bounds = target_bounds.AdjustToFit(grid()->GetLocalBounds(true));
  views::View* cell = grid()->GetChildViewAt(new_index);
  gfx::Rect cell_bounds = cell->bounds();
  if (target_bounds.origin() != cell_bounds.origin()) {
    grid()->SchedulePaint(cell_bounds, false);
    grid()->SchedulePaint(target_bounds, false);
    cell->SetBounds(target_bounds);
  }
}

void TabOverviewDragController::Attach(int index) {
  DCHECK(detached_tab_);
  DCHECK(model());
  current_index_ = index;
  modifying_model_ = true;
  model()->InsertTabContentsAt(index, detached_tab_, true, false);
  modifying_model_ = false;
  grid()->set_floating_index(index);
  SetDetachedContents(NULL);

  detached_window_->Close();
  detached_window_ = NULL;
}

void TabOverviewDragController::Detach(const gfx::Point& location) {
  if (detached_tab_) {
    // Already detached.
    return;
  }

  detached_window_ = CreateDetachedWindow(
      location, model()->GetTabContentsAt(current_index_));
  detached_window_->Show();

  grid()->set_floating_index(-1);
  SetDetachedContents(model()->GetTabContentsAt(current_index_));
  if (model()->count() == 1) {
    // The model is going to be empty. Tell the host to move us offscreen.
    // NOTE: it would be nice to hide and destroy the window here but this
    // causes two problems: we'll stop getting events, and we don't want
    // to empty out the tabstrip as otherwise they may trigger Chrome to
    // exit.
    controller_->MoveOffscreen();
    hidden_browser_ = controller_->browser();
    gtk_widget_hide(GTK_WIDGET(static_cast<BrowserWindowGtk*>(
        hidden_browser_->window())->GetNativeHandle()));
  }
  modifying_model_ = true;
  model()->DetachTabContentsAt(current_index_);
  modifying_model_ = false;
}

void TabOverviewDragController::DropTab(const gfx::Point& location) {
  TabContents* contents = detached_tab_;
  SetDetachedContents(NULL);

  gfx::Point screen_loc(location);
  grid()->ConvertPointToScreen(grid(), &screen_loc);
  gfx::Rect window_bounds(screen_loc, browser_window_size_);
  Browser* new_browser =
      original_model_->delegate()->CreateNewStripWithContents(
          contents, window_bounds, DockInfo());
  new_browser->window()->Show();

  detached_window_->Close();
  detached_window_ = NULL;
}

void TabOverviewDragController::MoveDetachedWindow(
    const gfx::Point& location) {
  gfx::Point screen_loc = location;
  screen_loc.Offset(-x_offset_, -y_offset_);
  grid()->ConvertPointToScreen(grid(), &screen_loc);
  detached_window_->SetBounds(
      gfx::Rect(screen_loc,
                detached_window_->GetRootView()->GetPreferredSize()));

  // Notify the wm of the move.
  TabOverviewTypes::Message message;
  message.set_type(TabOverviewTypes::Message::WM_MOVE_FLOATING_TAB);
  message.set_param(0, x11_util::GetX11WindowFromGtkWidget(
                        detached_window_->GetNativeView()));
  message.set_param(1, screen_loc.x() + x_offset_);
  message.set_param(2, screen_loc.y() + y_offset_);
  TabOverviewTypes::instance()->SendMessage(message);
}

views::Widget* TabOverviewDragController::CreateDetachedWindow(
    const gfx::Point& location,
    TabContents* tab_contents) {
  // TODO: wrap the cell in another view that provides a background.
  views::WidgetGtk* widget =
      new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  widget->MakeTransparent();
  gfx::Point screen_loc = location;
  screen_loc.Offset(-x_offset_, -y_offset_);
  grid()->ConvertPointToScreen(grid(), &screen_loc);
  TabOverviewCell* cell = new TabOverviewCell();
  cell->set_preferred_size(
      gfx::Size(grid()->cell_width(), grid()->cell_height()));
  controller_->ConfigureCell(cell, tab_contents);
  widget->Init(NULL, gfx::Rect(screen_loc, cell->GetPreferredSize()), true);
  widget->GetRootView()->SetLayoutManager(new views::FillLayout());
  widget->GetRootView()->AddChildView(cell);

  std::vector<int> params(4);
  params[0] = screen_loc.x() + x_offset_;
  params[1] = screen_loc.y() + y_offset_;
  params[2] = x_offset_;
  params[3] = y_offset_;
  TabOverviewTypes::instance()->SetWindowType(
      widget->GetNativeView(),
      TabOverviewTypes::WINDOW_TYPE_CHROME_FLOATING_TAB,
      &params);

  return widget;
}

void TabOverviewDragController::SetDetachedContents(TabContents* tab) {
  if (detached_tab_) {
    registrar_.Remove(this,
                      NotificationType::TAB_CONTENTS_DESTROYED,
                      Source<TabContents>(detached_tab_));
    if (detached_tab_->delegate() == this)
      detached_tab_->set_delegate(original_delegate_);
    else
      DLOG(WARNING) << " delegate changed";
  }
  original_delegate_ = NULL;
  detached_tab_ = tab;
  if (tab) {
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab));

    // We need to be the delegate so we receive messages about stuff,
    // otherwise our dragged contents may be replaced and subsequently
    // collected/destroyed while the drag is in process, leading to
    // nasty crashes.
    original_delegate_ = tab->delegate();
    tab->set_delegate(this);
  }
}
