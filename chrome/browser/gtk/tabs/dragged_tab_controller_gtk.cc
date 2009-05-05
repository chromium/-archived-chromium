// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/dragged_tab_controller_gtk.h"

#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"

namespace {

// Used to determine how far a tab must obscure another tab in order to swap
// their indexes.
const int kHorizontalMoveThreshold = 16;  // pixels

}  // namespace

DraggedTabControllerGtk::DraggedTabControllerGtk(TabGtk* source_tab,
                                                 TabStripGtk* source_tabstrip)
    : dragged_contents_(NULL),
      original_delegate_(NULL),
      source_tab_(source_tab),
      source_tabstrip_(source_tabstrip),
      source_model_index_(source_tabstrip->GetIndexOfTab(source_tab)),
      attached_tabstrip_(source_tabstrip),
      last_move_screen_x_(0) {
  SetDraggedContents(
      source_tabstrip_->model()->GetTabContentsAt(source_model_index_));
}

DraggedTabControllerGtk::~DraggedTabControllerGtk() {
}

void DraggedTabControllerGtk::CaptureDragInfo(const gfx::Point& mouse_offset) {
  start_screen_point_ = GetCursorScreenPoint();
  mouse_offset_ = mouse_offset;
  snap_bounds_ = source_tab_->bounds();
}

void DraggedTabControllerGtk::Drag() {
  ContinueDragging();
}

bool DraggedTabControllerGtk::EndDrag(bool canceled) {
  return EndDragImpl(canceled ? CANCELED : NORMAL);
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
  // TODO(jhawkins): We don't handle the situation where the last tab is dragged
  // out of a window, so we'll just go with the way Windows handles dragging for
  // now.
  gfx::Point screen_point = GetCursorScreenPoint();
  MoveTab(screen_point);
}

void DraggedTabControllerGtk::MoveTab(const gfx::Point& screen_point) {
  gfx::Point dragged_point = GetDraggedPoint(screen_point);

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
      gfx::Rect bounds = source_tab_->bounds();
      int to_index = GetInsertionIndexForDraggedBounds(bounds);
      to_index = NormalizeIndexToAttachedTabStrip(to_index);
      if (from_index != to_index) {
        last_move_screen_x_ = screen_point.x();
        snap_bounds_ = attached_tabstrip_->GetTabAt(to_index)->bounds();
        attached_model->MoveTabContentsAt(from_index, to_index, true);
      }
    }
  }

  // Move the tab. There are no changes to the model if we're detached.
  gfx::Rect bounds = source_tab_->bounds();
  bounds.set_x(dragged_point.x());
  source_tab_->SetBounds(bounds);
  gtk_fixed_move(GTK_FIXED(source_tabstrip_->tabstrip_.get()),
      source_tab_->widget(), bounds.x(), bounds.y());
}

TabStripGtk* DraggedTabControllerGtk::GetTabStripForPoint(
    const gfx::Point& screen_point) {
  // TODO(jhawkins): Actually get the correct tabstrip under |screen_point|.
  return source_tabstrip_;
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

gfx::Point DraggedTabControllerGtk::GetDraggedPoint(const gfx::Point& point) {
  int x = point.x() - mouse_offset_.x();
  int y = point.y() - mouse_offset_.y();

  // Snap the dragged tab to the tab strip.
  if (x < 0)
    x = 0;

  // Make sure the tab can't be dragged off the right side of the tab strip.
  int max_x = attached_tabstrip_->bounds_.right() - source_tab_->width();
  if (x > max_x)
    x = max_x;

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
  // We save this here because code below will modify |attached_tabstrip_|.
  if (attached_tabstrip_) {
    int index = attached_tabstrip_->model()->GetIndexOfTabContents(
        dragged_contents_);
    if (attached_tabstrip_ != source_tabstrip_) {
      // The Tab was inserted into another TabStrip. We need to put it back
      // into the original one.
      attached_tabstrip_->model()->DetachTabContentsAt(index);
      // TODO(beng): (Cleanup) seems like we should use Attach() for this
      //             somehow.
      attached_tabstrip_ = source_tabstrip_;
      source_tabstrip_->model()->InsertTabContentsAt(source_model_index_,
          dragged_contents_, true, false);
    } else {
      // The Tab was moved within the TabStrip where the drag was initiated.
      // Move it back to the starting location.
      source_tabstrip_->model()->MoveTabContentsAt(index, source_model_index_,
          true);
    }
  } else {
    // TODO(beng): (Cleanup) seems like we should use Attach() for this
    //             somehow.
    attached_tabstrip_ = source_tabstrip_;
    // The Tab was detached from the TabStrip where the drag began, and has not
    // been attached to any other TabStrip. We need to put it back into the
    // source TabStrip.
    source_tabstrip_->model()->InsertTabContentsAt(source_model_index_,
        dragged_contents_, true, false);
  }
}

bool DraggedTabControllerGtk::CompleteDrag() {
  // We don't need to do anything other than make the Tab visible again,
  // since the dragged tab is going away.
  gfx::Rect bounds = source_tab_->bounds();
  bounds.set_x(snap_bounds_.x());
  source_tab_->SetBounds(bounds);
  gtk_fixed_move(GTK_FIXED(source_tabstrip_->tabstrip_.get()),
      source_tab_->widget(), bounds.x(), bounds.y());

  return false;
}

gfx::Point DraggedTabControllerGtk::GetCursorScreenPoint() const {
  // Get default display and screen.
  GdkDisplay* display = gdk_display_get_default();

  // Get cursor position.
  int x, y;
  gdk_display_get_pointer(display, NULL, &x, &y, NULL);

  return gfx::Point(x, y);
}
