// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/dragged_tab_controller_gtk.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/tabs/dragged_tab_gtk.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/platform_util.h"

namespace {

// Delay, in ms, during dragging before we bring a window to front.
const int kBringToFrontDelay = 750;

// Used to determine how far a tab must obscure another tab in order to swap
// their indexes.
const int kHorizontalMoveThreshold = 16;  // pixels

// How far a drag must pull a tab out of the tabstrip in order to detach it.
const int kVerticalDetachMagnetism = 15;  // pixels

}  // namespace

DraggedTabControllerGtk::DraggedTabControllerGtk(TabGtk* source_tab,
                                                 TabStripGtk* source_tabstrip)
    : dragged_contents_(NULL),
      original_delegate_(NULL),
      source_tab_(source_tab),
      source_tabstrip_(source_tabstrip),
      source_model_index_(source_tabstrip->GetIndexOfTab(source_tab)),
      attached_tabstrip_(source_tabstrip),
      in_destructor_(false),
      last_move_screen_x_(0) {
  SetDraggedContents(
      source_tabstrip_->model()->GetTabContentsAt(source_model_index_));
}

DraggedTabControllerGtk::~DraggedTabControllerGtk() {
  in_destructor_ = true;
  CleanUpSourceTab();
  // Need to delete the dragged tab here manually _before_ we reset the dragged
  // contents to NULL, otherwise if the view is animating to its destination
  // bounds, it won't be able to clean up properly since its cleanup routine
  // uses GetIndexForDraggedContents, which will be invalid.
  dragged_tab_.reset();
  SetDraggedContents(NULL);
}

void DraggedTabControllerGtk::CaptureDragInfo(const gfx::Point& mouse_offset) {
  start_screen_point_ = GetCursorScreenPoint();
  mouse_offset_ = mouse_offset;
}

void DraggedTabControllerGtk::Drag() {
  if (!source_tab_)
    return;

  bring_to_front_timer_.Stop();

  // Before we get to dragging anywhere, ensure that we consider ourselves
  // attached to the source tabstrip.
  if (source_tab_->IsVisible()) {
    Attach(source_tabstrip_, gfx::Point());
  }

  if (!source_tab_->IsVisible()) {
    // TODO(jhawkins): Save focus.
    ContinueDragging();
  }
}

bool DraggedTabControllerGtk::EndDrag(bool canceled) {
  return EndDragImpl(canceled ? CANCELED : NORMAL);
}

TabGtk* DraggedTabControllerGtk::GetDragSourceTabForContents(
    TabContents* contents) const {
  if (attached_tabstrip_ == source_tabstrip_)
    return contents == dragged_contents_ ? source_tab_ : NULL;
  return NULL;
}

bool DraggedTabControllerGtk::IsDragSourceTab(TabGtk* tab) const {
  return source_tab_ == tab;
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabControllerGtk, TabContentsDelegate implementation:

void DraggedTabControllerGtk::OpenURLFromTab(TabContents* source,
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

void DraggedTabControllerGtk::NavigationStateChanged(const TabContents* source,
                                                     unsigned changed_flags) {
  if (dragged_tab_.get())
    dragged_tab_->Update();
}

void DraggedTabControllerGtk::AddNewContents(TabContents* source,
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

void DraggedTabControllerGtk::ActivateContents(TabContents* contents) {
  // Ignored.
}

void DraggedTabControllerGtk::LoadingStateChanged(TabContents* source) {
  // TODO(jhawkins): It would be nice to respond to this message by changing the
  // screen shot in the dragged tab.
  if (dragged_tab_.get())
    dragged_tab_->Update();
}

void DraggedTabControllerGtk::CloseContents(TabContents* source) {
  // Theoretically could be called by a window. Should be ignored
  // because window.close() is ignored (usually, even though this
  // method gets called.)
}

void DraggedTabControllerGtk::MoveContents(TabContents* source,
                                        const gfx::Rect& pos) {
  // Theoretically could be called by a web page trying to move its
  // own window. Should be ignored since we're moving the window...
}

bool DraggedTabControllerGtk::IsPopup(TabContents* source) {
  return false;
}

void DraggedTabControllerGtk::ToolbarSizeChanged(TabContents* source,
                                                 bool finished) {
  // Dragged tabs don't care about this.
}

void DraggedTabControllerGtk::URLStarredChanged(TabContents* source,
                                                bool starred) {
  // Ignored.
}

void DraggedTabControllerGtk::UpdateTargetURL(TabContents* source,
                                              const GURL& url) {
  // Ignored.
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabControllerGtk, NotificationObserver implementation:

void DraggedTabControllerGtk::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  DCHECK(Source<TabContents>(source).ptr() == dragged_contents_);
  EndDragImpl(TAB_DESTROYED);
}

void DraggedTabControllerGtk::InitWindowCreatePoint() {
  window_create_point_.SetPoint(mouse_offset_.x(), mouse_offset_.y());
}

gfx::Point DraggedTabControllerGtk::GetWindowCreatePoint() const {
  gfx::Point cursor_point = GetCursorScreenPoint();
  return gfx::Point(cursor_point.x() - window_create_point_.x(),
                    cursor_point.y() - window_create_point_.y());
}

void DraggedTabControllerGtk::SetDraggedContents(TabContents* new_contents) {
  if (dragged_contents_) {
    registrar_.Remove(this,
                      NotificationType::TAB_CONTENTS_DESTROYED,
                      Source<TabContents>(dragged_contents_));
    if (original_delegate_)
      dragged_contents_->set_delegate(original_delegate_);
  }
  original_delegate_ = NULL;
  dragged_contents_ = new_contents;
  if (dragged_contents_) {
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(dragged_contents_));

    // We need to be the delegate so we receive messages about stuff,
    // otherwise our dragged_contents() may be replaced and subsequently
    // collected/destroyed while the drag is in process, leading to
    // nasty crashes.
    original_delegate_ = dragged_contents_->delegate();
    dragged_contents_->set_delegate(this);
  }
}

void DraggedTabControllerGtk::ContinueDragging() {
  EnsureDraggedTab();

  // TODO(jhawkins): We don't handle the situation where the last tab is dragged
  // out of a window, so we'll just go with the way Windows handles dragging for
  // now.
  gfx::Point screen_point = GetCursorScreenPoint();

  // Determine whether or not we have dragged over a compatible TabStrip in
  // another browser window. If we have, we should attach to it and start
  // dragging within it.
  TabStripGtk* target_tabstrip = GetTabStripForPoint(screen_point);
  if (target_tabstrip != attached_tabstrip_) {
    // Make sure we're fully detached from whatever TabStrip we're attached to
    // (if any).
    if (attached_tabstrip_)
      Detach();

    if (target_tabstrip)
      Attach(target_tabstrip, screen_point);
  }

  if (!target_tabstrip) {
    bring_to_front_timer_.Start(
        base::TimeDelta::FromMilliseconds(kBringToFrontDelay), this,
        &DraggedTabControllerGtk::BringWindowUnderMouseToFront);
  }

  MoveTab(screen_point);
}

void DraggedTabControllerGtk::MoveTab(const gfx::Point& screen_point) {
  gfx::Point dragged_tab_point = GetDraggedTabPoint(screen_point);

  if (attached_tabstrip_) {
    // Determine the horizontal move threshold. This is dependent on the width
    // of tabs. The smaller the tabs compared to the standard size, the smaller
    // the threshold.
    double unselected, selected;
    attached_tabstrip_->GetCurrentTabWidths(&unselected, &selected);
    double ratio = unselected / TabGtk::GetStandardSize().width();
    int threshold = static_cast<int>(ratio * kHorizontalMoveThreshold);

    // Update the model, moving the TabContents from one index to another. Do
    // this only if we have moved a minimum distance since the last reorder (to
    // prevent jitter).
    if (abs(screen_point.x() - last_move_screen_x_) > threshold) {
      TabStripModel* attached_model = attached_tabstrip_->model();
      int from_index =
          attached_model->GetIndexOfTabContents(dragged_contents_);
      gfx::Rect bounds = GetDraggedTabTabStripBounds(dragged_tab_point);
      int to_index = GetInsertionIndexForDraggedBounds(bounds);
      to_index = NormalizeIndexToAttachedTabStrip(to_index);
      if (from_index != to_index) {
        last_move_screen_x_ = screen_point.x();
        attached_model->MoveTabContentsAt(from_index, to_index, true);
      }
    }
  }

  // Move the dragged tab. There are no changes to the model if we're detached.
  dragged_tab_->MoveTo(dragged_tab_point);
}

TabStripGtk* DraggedTabControllerGtk::GetTabStripForPoint(
    const gfx::Point& screen_point) {
  GtkWidget* dragged_window = dragged_tab_->widget();
  dock_windows_.insert(dragged_window);
  gfx::NativeWindow local_window =
      DockInfo::GetLocalProcessWindowAtPoint(screen_point, dock_windows_);
  dock_windows_.erase(dragged_window);
  if (!local_window)
    return NULL;

  BrowserWindowGtk* browser =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(local_window);
  if (!browser)
    return NULL;

  TabStripGtk* other_tabstrip = browser->tabstrip();
  if (!other_tabstrip->IsCompatibleWith(source_tabstrip_))
    return NULL;

  return GetTabStripIfItContains(other_tabstrip, screen_point);
}

TabStripGtk* DraggedTabControllerGtk::GetTabStripIfItContains(
    TabStripGtk* tabstrip, const gfx::Point& screen_point) const {
  // Make sure the specified screen point is actually within the bounds of the
  // specified tabstrip...
  gfx::Rect tabstrip_bounds =
      gtk_util::GetWidgetScreenBounds(tabstrip->tabstrip_.get());
  if (screen_point.x() < tabstrip_bounds.right() &&
      screen_point.x() >= tabstrip_bounds.x()) {
    // TODO(beng): make this be relative to the start position of the mouse for
    // the source TabStrip.
    int upper_threshold = tabstrip_bounds.bottom() + kVerticalDetachMagnetism;
    int lower_threshold = tabstrip_bounds.y() - kVerticalDetachMagnetism;
    if (screen_point.y() >= lower_threshold &&
        screen_point.y() <= upper_threshold) {
      return tabstrip;
    }
  }

  return NULL;
}

void DraggedTabControllerGtk::Attach(TabStripGtk* attached_tabstrip,
                                     const gfx::Point& screen_point) {
  attached_tabstrip_ = attached_tabstrip;
  InitWindowCreatePoint();
  attached_tabstrip_->GenerateIdealBounds();

  TabGtk* tab = GetTabMatchingDraggedContents(attached_tabstrip_);

  // Update the tab first, so we can ask it for its bounds and determine
  // where to insert the hidden tab.

  // If this is the first time Attach is called for this drag, we're attaching
  // to the source tabstrip, and we should assume the tab count already
  // includes this tab since we haven't been detached yet. If we don't do this,
  // the dragged representation will be a different size to others in the
  // tabstrip.
  int tab_count = attached_tabstrip_->GetTabCount();
  if (!tab)
    ++tab_count;
  double unselected_width = 0, selected_width = 0;
  attached_tabstrip_->GetDesiredTabWidths(tab_count, &unselected_width,
                                          &selected_width);
  EnsureDraggedTab();
  dragged_tab_->Attach(static_cast<int>(selected_width));

  if (!tab) {
    // There is no tab in |attached_tabstrip| that corresponds to the dragged
    // TabContents. We must now create one.

    // Remove ourselves as the delegate now that the dragged TabContents is
    // being inserted back into a Browser.
    dragged_contents_->set_delegate(NULL);
    original_delegate_ = NULL;

    // Return the TabContents' to normalcy.
    dragged_contents_->set_capturing_contents(false);

    // We need to ask the tabstrip we're attached to ensure that the ideal
    // bounds for all its tabs are correctly generated, because the calculation
    // in GetInsertionIndexForDraggedBounds needs them to be to figure out the
    // appropriate insertion index.
    attached_tabstrip_->GenerateIdealBounds();

    // Inserting counts as a move. We don't want the tabs to jitter when the
    // user moves the tab immediately after attaching it.
    last_move_screen_x_ = screen_point.x();

    // Figure out where to insert the tab based on the bounds of the dragged
    // representation and the ideal bounds of the other tabs already in the
    // strip. ("ideal bounds" are stable even if the tabs' actual bounds are
    // changing due to animation).
    gfx::Rect bounds = GetDraggedTabTabStripBounds(screen_point);
    int index = GetInsertionIndexForDraggedBounds(bounds);
    index = std::max(std::min(index, attached_tabstrip_->model()->count()), 0);
    attached_tabstrip_->model()->InsertTabContentsAt(index, dragged_contents_,
        true, false);

    tab = GetTabMatchingDraggedContents(attached_tabstrip_);
  }
  DCHECK(tab);  // We should now have a tab.
  tab->SetVisible(false);

  // TODO(jhawkins): Move the corresponding window to the front.
}

void DraggedTabControllerGtk::Detach() {
  // Update the Model.
  TabStripModel* attached_model = attached_tabstrip_->model();
  int index = attached_model->GetIndexOfTabContents(dragged_contents_);
  if (index >= 0 && index < attached_model->count()) {
    // Sometimes, DetachTabContentsAt has consequences that result in
    // attached_tabstrip_ being set to NULL, so we need to save it first.
    TabStripGtk* attached_tabstrip = attached_tabstrip_;
    attached_model->DetachTabContentsAt(index);
    attached_tabstrip->SchedulePaint();
  }

  // If we've removed the last tab from the tabstrip, hide the frame now.
  if (attached_model->empty())
    HideFrame();

  // Update the dragged tab. This NULL check is necessary apparently in some
  // conditions during automation where the view_ is destroyed inside a
  // function call preceding this point but after it is created.
  if (dragged_tab_.get()) {
    RenderViewHost* host = dragged_contents_->render_view_host();
    dragged_tab_->Detach(dragged_contents_->GetContentNativeView(),
                         host->GetBackingStore(false));
  }

  // Detaching resets the delegate, but we still want to be the delegate.
  dragged_contents_->set_delegate(this);

  attached_tabstrip_ = NULL;
}

gfx::Point DraggedTabControllerGtk::ConvertScreenPointToTabStripPoint(
    TabStripGtk* tabstrip, const gfx::Point& screen_point) {
  gint x, y;
  gdk_window_get_origin(tabstrip->tabstrip_->window, &x, &y);
  return gfx::Point(screen_point.x() - x, screen_point.y() - y);
}

gfx::Rect DraggedTabControllerGtk::GetDraggedTabTabStripBounds(
    const gfx::Point& screen_point) {
  gfx::Point client_point =
      ConvertScreenPointToTabStripPoint(attached_tabstrip_, screen_point);
  gfx::Size tab_size = dragged_tab_->attached_tab_size();
  return gfx::Rect(client_point.x(), client_point.y(),
                   tab_size.width(), tab_size.height());
}

int DraggedTabControllerGtk::GetInsertionIndexForDraggedBounds(
    const gfx::Rect& dragged_bounds) const {
  int right_tab_x = 0;

  // TODO(jhawkins): Handle RTL layout.

  // Divides each tab into two halves to see if the dragged tab has crossed
  // the halfway boundary necessary to move past the next tab.
  for (int i = 0; i < attached_tabstrip_->GetTabCount(); i++) {
    gfx::Rect ideal_bounds = attached_tabstrip_->GetIdealBounds(i);

    gfx::Rect left_half = ideal_bounds;
    left_half.set_width(left_half.width() / 2);

    gfx::Rect right_half = ideal_bounds;
    right_half.set_width(ideal_bounds.width() - left_half.width());
    right_half.set_x(left_half.right());

    right_tab_x = right_half.right();

    if (dragged_bounds.x() >= right_half.x() &&
        dragged_bounds.x() < right_half.right()) {
      return i + 1;
    } else if (dragged_bounds.x() >= left_half.x() &&
               dragged_bounds.x() < left_half.right()) {
      return i;
    }
  }

  if (dragged_bounds.right() > right_tab_x)
    return attached_tabstrip_->model()->count();

  return TabStripModel::kNoTab;
}

gfx::Point DraggedTabControllerGtk::GetDraggedTabPoint(
    const gfx::Point& screen_point) {
  int x = screen_point.x() - mouse_offset_.x();
  int y = screen_point.y() - mouse_offset_.y();

  // If we're not attached, we just use x and y from above.
  if (attached_tabstrip_) {
    gfx::Rect tabstrip_bounds =
        gtk_util::GetWidgetScreenBounds(attached_tabstrip_->tabstrip_.get());
    // Snap the dragged tab to the tabstrip if we are attached, detaching
    // only when the mouse position (screen_point) exceeds the screen bounds
    // of the tabstrip.
    if (x < tabstrip_bounds.x() && screen_point.x() >= tabstrip_bounds.x())
      x = tabstrip_bounds.x();

    gfx::Size tab_size = dragged_tab_->attached_tab_size();
    int vertical_drag_magnetism = tab_size.height() * 2;
    int vertical_detach_point = tabstrip_bounds.y() - vertical_drag_magnetism;
    if (y < tabstrip_bounds.y() && screen_point.y() >= vertical_detach_point)
      y = tabstrip_bounds.y();

    // Make sure the tab can't be dragged off the right side of the tabstrip
    // unless the mouse pointer passes outside the bounds of the strip by
    // clamping the position of the dragged window to the tabstrip width less
    // the width of one tab until the mouse pointer (screen_point) exceeds the
    // screen bounds of the tabstrip.
    int max_x = tabstrip_bounds.right() - tab_size.width();
    int max_y = tabstrip_bounds.bottom() - tab_size.height();
    if (x > max_x && screen_point.x() <= tabstrip_bounds.right())
      x = max_x;
    if (y > max_y && screen_point.y() <=
        (tabstrip_bounds.bottom() + vertical_drag_magnetism)) {
      y = max_y;
    }
  }
  return gfx::Point(x, y);
}

int DraggedTabControllerGtk::NormalizeIndexToAttachedTabStrip(int index) const {
  if (index >= attached_tabstrip_->model_->count())
    return attached_tabstrip_->model_->count() - 1;
  if (index == TabStripModel::kNoTab)
    return 0;
  return index;
}

TabGtk* DraggedTabControllerGtk::GetTabMatchingDraggedContents(
    TabStripGtk* tabstrip) const {
  int index = tabstrip->model()->GetIndexOfTabContents(dragged_contents_);
  return index == TabStripModel::kNoTab ? NULL : tabstrip->GetTabAt(index);
}

bool DraggedTabControllerGtk::EndDragImpl(EndDragType type) {
  // In gtk, it's possible to receive a drag-begin signal and an drag-end signal
  // without ever getting a drag-motion signal.  In this case, dragged_tab_ has
  // never been created, so bail out.
  if (!dragged_tab_.get())
    return true;

  bring_to_front_timer_.Stop();

  // WARNING: this may be invoked multiple times. In particular, if deletion
  // occurs after a delay (as it does when the tab is released in the original
  // tab strip) and the navigation controller/tab contents is deleted before
  // the animation finishes, this is invoked twice. The second time through
  // type == TAB_DESTROYED.

  bool destroy_now = true;
  if (type != TAB_DESTROYED) {
    if (type == CANCELED) {
      RevertDrag();
    } else {
      destroy_now = CompleteDrag();
    }

    if (dragged_contents_ && dragged_contents_->delegate() == this)
      dragged_contents_->set_delegate(original_delegate_);
  } else {
    // If we get here it means the NavigationController is going down. Don't
    // attempt to do any cleanup other than resetting the delegate (if we're
    // still the delegate).
    if (dragged_contents_ && dragged_contents_->delegate() == this)
      dragged_contents_->set_delegate(NULL);
    dragged_contents_ = NULL;
  }

  // The delegate of the dragged contents should have been reset. Unset the
  // original delegate so that we don't attempt to reset the delegate when
  // deleted.
  DCHECK(!dragged_contents_ || dragged_contents_->delegate() != this);
  original_delegate_ = NULL;

  // If we're not destroyed now, we'll be destroyed asynchronously later.
  if (destroy_now)
    source_tabstrip_->DestroyDragController();

  return destroy_now;
}

void DraggedTabControllerGtk::RevertDrag() {
  // TODO(jhawkins): Restore the window frame.
  // We save this here because code below will modify |attached_tabstrip_|.
  if (attached_tabstrip_) {
    int index = attached_tabstrip_->model()->GetIndexOfTabContents(
        dragged_contents_);
    if (attached_tabstrip_ != source_tabstrip_) {
      // The tab was inserted into another tabstrip. We need to put it back
      // into the original one.
      attached_tabstrip_->model()->DetachTabContentsAt(index);
      // TODO(beng): (Cleanup) seems like we should use Attach() for this
      //             somehow.
      attached_tabstrip_ = source_tabstrip_;
      source_tabstrip_->model()->InsertTabContentsAt(source_model_index_,
          dragged_contents_, true, false);
    } else {
      // The tab was moved within the tabstrip where the drag was initiated.
      // Move it back to the starting location.
      source_tabstrip_->model()->MoveTabContentsAt(index, source_model_index_,
          true);
    }
  } else {
    // TODO(beng): (Cleanup) seems like we should use Attach() for this
    //             somehow.
    attached_tabstrip_ = source_tabstrip_;
    // The tab was detached from the tabstrip where the drag began, and has not
    // been attached to any other tabstrip. We need to put it back into the
    // source tabstrip.
    source_tabstrip_->model()->InsertTabContentsAt(source_model_index_,
        dragged_contents_, true, false);
  }

  source_tab_->SetVisible(true);
}

bool DraggedTabControllerGtk::CompleteDrag() {
  bool destroy_immediately = true;
  if (attached_tabstrip_) {
    // We don't need to do anything other than make the tab visible again,
    // since the dragged tab is going away.
    TabGtk* tab = GetTabMatchingDraggedContents(attached_tabstrip_);
    gfx::Rect rect = GetTabScreenBounds(tab);
    dragged_tab_->AnimateToBounds(GetTabScreenBounds(tab),
        NewCallback(this, &DraggedTabControllerGtk::OnAnimateToBoundsComplete));
    destroy_immediately = false;
  } else {
    // Compel the model to construct a new window for the detached TabContents.
    GtkWindow* browser_window =
        platform_util::GetTopLevel(source_tabstrip_->widget());
    gint x, y, width, height;
    gtk_window_get_position(browser_window, &x, &y);
    gtk_window_get_size(browser_window, &width, &height);
    gfx::Rect browser_rect = gfx::Rect(x, y, width, height);
    gfx::Rect window_bounds(
        GetWindowCreatePoint(),
        gfx::Size(browser_rect.width(), browser_rect.height()));
    Browser* new_browser =
        source_tabstrip_->model()->delegate()->CreateNewStripWithContents(
            dragged_contents_, window_bounds, dock_info_);
    new_browser->window()->Show();
    CleanUpHiddenFrame();
  }

  return destroy_immediately;
}

void DraggedTabControllerGtk::EnsureDraggedTab() {
  if (!dragged_tab_.get()) {
    gfx::Rect rect;
    dragged_contents_->GetContainerBounds(&rect);

    dragged_tab_.reset(new DraggedTabGtk(dragged_contents_, mouse_offset_,
        gfx::Size(rect.width(), rect.height())));
  }
}

gfx::Point DraggedTabControllerGtk::GetCursorScreenPoint() const {
  // Get default display and screen.
  GdkDisplay* display = gdk_display_get_default();

  // Get cursor position.
  int x, y;
  gdk_display_get_pointer(display, NULL, &x, &y, NULL);

  return gfx::Point(x, y);
}

// static
gfx::Rect DraggedTabControllerGtk::GetTabScreenBounds(TabGtk* tab) {
  // A hidden widget moved with gtk_fixed_move in a GtkFixed container doesn't
  // update its allocation until after the widget is shown, so we have to use
  // the tab bounds we keep track of.
  int x, y;
  x = tab->bounds().x();
  y = tab->bounds().y();

  GtkWidget* widget = tab->widget();
  GtkWidget* parent = gtk_widget_get_parent(widget);
  gfx::Point point = gtk_util::GetWidgetScreenPosition(parent);
  x += point.x();
  y += point.y();

  return gfx::Rect(x, y, widget->allocation.width, widget->allocation.height);
}

void DraggedTabControllerGtk::HideFrame() {
  GtkWidget* tabstrip = source_tabstrip_->widget();
  GtkWindow* window = platform_util::GetTopLevel(tabstrip);
  gtk_widget_hide(GTK_WIDGET(window));
}

void DraggedTabControllerGtk::CleanUpHiddenFrame() {
  // If the model we started dragging from is now empty, we must ask the
  // delegate to close the frame.
  if (source_tabstrip_->model()->empty())
    source_tabstrip_->model()->delegate()->CloseFrameAfterDragSession();
}

void DraggedTabControllerGtk::CleanUpSourceTab() {
  // If we were attached to the source tabstrip, source tab will be in use
  // as the tab. If we were detached or attached to another tabstrip, we can
  // safely remove this item and delete it now.
  if (attached_tabstrip_ != source_tabstrip_) {
    source_tabstrip_->DestroyDraggedSourceTab(source_tab_);
    source_tab_ = NULL;
  }
}

void DraggedTabControllerGtk::OnAnimateToBoundsComplete() {
  // Sometimes, for some reason, in automation we can be called back on a
  // detach even though we aren't attached to a tabstrip. Guard against that.
  if (attached_tabstrip_) {
    TabGtk* tab = GetTabMatchingDraggedContents(attached_tabstrip_);
    if (tab) {
      tab->SetVisible(true);
      // Paint the tab now, otherwise there may be slight flicker between the
      // time the dragged tab window is destroyed and we paint.
      tab->SchedulePaint();
    }
  }

  CleanUpHiddenFrame();

  if (!in_destructor_)
    source_tabstrip_->DestroyDragController();
}

void DraggedTabControllerGtk::BringWindowUnderMouseToFront() {
  // If we're going to dock to another window, bring it to the front.
  gfx::NativeWindow window = dock_info_.window();
  if (!window) {
    gfx::NativeView dragged_tab = dragged_tab_->widget();
    dock_windows_.insert(dragged_tab);
    window = DockInfo::GetLocalProcessWindowAtPoint(GetCursorScreenPoint(),
                                                    dock_windows_);
    dock_windows_.erase(dragged_tab);
  }

  if (window)
    gtk_window_present(GTK_WINDOW(window));
}
