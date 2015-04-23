// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/root_view.h"

#include <algorithm>

#include "app/drag_drop_types.h"
#include "app/gfx/canvas.h"
#if defined(OS_WIN)
#include "base/base_drag_source.h"
#endif
#include "base/logging.h"
#include "base/message_loop.h"
#include "views/focus/view_storage.h"
#if defined(OS_WIN)
#include "views/widget/root_view_drop_target.h"
#endif
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// A Task to trigger non urgent painting.
//
/////////////////////////////////////////////////////////////////////////////
class PaintTask : public Task {
 public:
  explicit PaintTask(RootView* target) : root_view_(target) {
  }

  ~PaintTask() {}

  void Cancel() {
    root_view_ = NULL;
  }

  void Run() {
    if (root_view_)
      root_view_->PaintNow();
  }
 private:
  // The target root view.
  RootView* root_view_;

  DISALLOW_COPY_AND_ASSIGN(PaintTask);
};

const char RootView::kViewClassName[] = "views/RootView";

/////////////////////////////////////////////////////////////////////////////
//
// RootView - constructors, destructors, initialization
//
/////////////////////////////////////////////////////////////////////////////

RootView::RootView(Widget* widget)
  : mouse_pressed_handler_(NULL),
    mouse_move_handler_(NULL),
    last_click_handler_(NULL),
    widget_(widget),
    invalid_rect_urgent_(false),
    pending_paint_task_(NULL),
    paint_task_needed_(false),
    explicit_mouse_handler_(false),
#if defined(OS_WIN)
    previous_cursor_(NULL),
#endif
    default_keyboard_handler_(NULL),
    focus_on_mouse_pressed_(false),
    ignore_set_focus_calls_(false),
    focus_traversable_parent_(NULL),
    focus_traversable_parent_view_(NULL),
    drag_view_(NULL)
#ifndef NDEBUG
    ,
    is_processing_paint_(false)
#endif
{
}

RootView::~RootView() {
  // If we have children remove them explicitly so to make sure a remove
  // notification is sent for each one of them.
  if (!child_views_.empty())
    RemoveAllChildViews(true);

  if (pending_paint_task_)
    pending_paint_task_->Cancel();  // Ensure we're not called any more.
}

/////////////////////////////////////////////////////////////////////////////
//
// RootView - layout, painting
//
/////////////////////////////////////////////////////////////////////////////

void RootView::SchedulePaint(const gfx::Rect& r, bool urgent) {
  // If there is an existing invalid rect, add the union of the scheduled
  // rect with the invalid rect. This could be optimized further if
  // necessary.
  if (invalid_rect_.IsEmpty())
    invalid_rect_ = r;
  else
    invalid_rect_ = invalid_rect_.Union(r);

  if (urgent || invalid_rect_urgent_) {
    invalid_rect_urgent_ = true;
  } else {
    if (!pending_paint_task_) {
      pending_paint_task_ = new PaintTask(this);
      MessageLoop::current()->PostTask(FROM_HERE, pending_paint_task_);
    }
    paint_task_needed_ = true;
  }
}

void RootView::SchedulePaint() {
  View::SchedulePaint();
}

void RootView::SchedulePaint(int x, int y, int w, int h) {
  View::SchedulePaint();
}

#ifndef NDEBUG
// Sets the value of RootView's |is_processing_paint_| member to true as long
// as ProcessPaint is being called. Sets it to |false| when it returns.
class ScopedProcessingPaint {
 public:
  explicit ScopedProcessingPaint(bool* is_processing_paint)
      : is_processing_paint_(is_processing_paint) {
    *is_processing_paint_ = true;
  }

  ~ScopedProcessingPaint() {
    *is_processing_paint_ = false;
  }
 private:
  bool* is_processing_paint_;

  DISALLOW_EVIL_CONSTRUCTORS(ScopedProcessingPaint);
};
#endif

void RootView::ProcessPaint(gfx::Canvas* canvas) {
#ifndef NDEBUG
  ScopedProcessingPaint processing_paint(&is_processing_paint_);
#endif

  // Clip the invalid rect to our bounds. If a view is in a scrollview
  // it could be a lot larger
  invalid_rect_ = GetScheduledPaintRectConstrainedToSize();

  if (invalid_rect_.IsEmpty())
    return;

  // Clear the background.
  canvas->drawColor(SK_ColorBLACK, SkXfermode::kClear_Mode);

  // Save the current transforms.
  canvas->save();

  // Set the clip rect according to the invalid rect.
  int clip_x = invalid_rect_.x() + x();
  int clip_y = invalid_rect_.y() + y();
  canvas->ClipRectInt(clip_x, clip_y, invalid_rect_.width(),
                      invalid_rect_.height());

  // Paint the tree
  View::ProcessPaint(canvas);

  // Restore the previous transform
  canvas->restore();

  ClearPaintRect();
}

void RootView::PaintNow() {
  if (pending_paint_task_) {
    pending_paint_task_->Cancel();
    pending_paint_task_ = NULL;
  }
  if (!paint_task_needed_)
    return;
  Widget* widget = GetWidget();
  if (widget)
    widget->PaintNow(invalid_rect_);
}

bool RootView::NeedsPainting(bool urgent) {
  bool has_invalid_rect = !invalid_rect_.IsEmpty();
  if (urgent) {
    if (invalid_rect_urgent_)
      return has_invalid_rect;
    else
      return false;
  } else {
    return has_invalid_rect;
  }
}

const gfx::Rect& RootView::GetScheduledPaintRect() {
  return invalid_rect_;
}

gfx::Rect RootView::GetScheduledPaintRectConstrainedToSize() {
  if (invalid_rect_.IsEmpty())
    return invalid_rect_;

  return invalid_rect_.Intersect(GetLocalBounds(true));
}

/////////////////////////////////////////////////////////////////////////////
//
// RootView - tree
//
/////////////////////////////////////////////////////////////////////////////

Widget* RootView::GetWidget() const {
  return widget_;
}

void RootView::ThemeChanged() {
  View::ThemeChanged();
}

/////////////////////////////////////////////////////////////////////////////
//
// RootView - event dispatch and propagation
//
/////////////////////////////////////////////////////////////////////////////

void RootView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (!is_add) {
    if (!explicit_mouse_handler_ && mouse_pressed_handler_ == child) {
      mouse_pressed_handler_ = NULL;
    }

#if defined(OS_WIN)
    if (drop_target_.get())
      drop_target_->ResetTargetViewIfEquals(child);
#else
    NOTIMPLEMENTED();
#endif

    if (mouse_move_handler_ == child) {
      mouse_move_handler_ = NULL;
    }

    if (GetFocusedView() == child) {
      FocusView(NULL);
    }

    if (child == drag_view_)
      drag_view_ = NULL;

    if (default_keyboard_handler_ == child) {
      default_keyboard_handler_ = NULL;
    }

    FocusManager* focus_manager = widget_->GetFocusManager();
    // An unparanted RootView does not have a FocusManager.
    if (focus_manager)
      focus_manager->ViewRemoved(parent, child);
#if defined(OS_WIN)
    ViewStorage::GetSharedInstance()->ViewRemoved(parent, child);
#else
    NOTIMPLEMENTED();
#endif
  }
}

void RootView::SetFocusOnMousePressed(bool f) {
  focus_on_mouse_pressed_ = f;
}

bool RootView::OnMousePressed(const MouseEvent& e) {
  // This function does not normally handle non-client messages except for
  // non-client double-clicks. Actually, all double-clicks are special as the
  // are formed from a single-click followed by a double-click event. When the
  // double-click event lands on a different view than its single-click part,
  // we transform it into a single-click which prevents odd things.
  if ((e.GetFlags() & MouseEvent::EF_IS_NON_CLIENT) &&
      !(e.GetFlags() & MouseEvent::EF_IS_DOUBLE_CLICK)) {
    last_click_handler_ = NULL;
    return false;
  }

  UpdateCursor(e);
  SetMouseLocationAndFlags(e);

  // If mouse_pressed_handler_ is non null, we are currently processing
  // a pressed -> drag -> released session. In that case we send the
  // event to mouse_pressed_handler_
  if (mouse_pressed_handler_) {
    MouseEvent mouse_pressed_event(e, this, mouse_pressed_handler_);
    drag_info.Reset();
    mouse_pressed_handler_->ProcessMousePressed(mouse_pressed_event,
                                                &drag_info);
    return true;
  }
  DCHECK(!explicit_mouse_handler_);

  bool hit_disabled_view = false;
  // Walk up the tree until we find a view that wants the mouse event.
  for (mouse_pressed_handler_ = GetViewForPoint(e.location());
       mouse_pressed_handler_ && (mouse_pressed_handler_ != this);
       mouse_pressed_handler_ = mouse_pressed_handler_->GetParent()) {
    if (!mouse_pressed_handler_->IsEnabled()) {
      // Disabled views should eat events instead of propagating them upwards.
      hit_disabled_view = true;
      break;
    }

    // See if this view wants to handle the mouse press.
    MouseEvent mouse_pressed_event(e, this, mouse_pressed_handler_);

    // Remove the double-click flag if the handler is different than the
    // one which got the first click part of the double-click.
    if (mouse_pressed_handler_ != last_click_handler_)
      mouse_pressed_event.set_flags(e.GetFlags() &
                                    ~MouseEvent::EF_IS_DOUBLE_CLICK);

    drag_info.Reset();
    bool handled = mouse_pressed_handler_->ProcessMousePressed(
        mouse_pressed_event, &drag_info);

    // The view could have removed itself from the tree when handling
    // OnMousePressed().  In this case, the removal notification will have
    // reset mouse_pressed_handler_ to NULL out from under us.  Detect this
    // case and stop.  (See comments in view.h.)
    //
    // NOTE: Don't return true here, because we don't want the frame to
    // forward future events to us when there's no handler.
    if (!mouse_pressed_handler_)
      break;

    // If the view handled the event, leave mouse_pressed_handler_ set and
    // return true, which will cause subsequent drag/release events to get
    // forwarded to that view.
    if (handled) {
      last_click_handler_ = mouse_pressed_handler_;
      return true;
    }
  }

  // Reset mouse_pressed_handler_ to indicate that no processing is occurring.
  mouse_pressed_handler_ = NULL;

  if (focus_on_mouse_pressed_) {
#if defined(OS_WIN)
    HWND hwnd = GetWidget()->GetNativeView();
    if (::GetFocus() != hwnd) {
      ::SetFocus(hwnd);
    }
#else
    NOTIMPLEMENTED();
#endif
  }

  // In the event that a double-click is not handled after traversing the
  // entire hierarchy (even as a single-click when sent to a different view),
  // it must be marked as handled to avoid anything happening from default
  // processing if it the first click-part was handled by us.
  if (last_click_handler_ && e.GetFlags() & MouseEvent::EF_IS_DOUBLE_CLICK)
    hit_disabled_view = true;

  last_click_handler_ = NULL;
  return hit_disabled_view;
}

bool RootView::ConvertPointToMouseHandler(const gfx::Point& l,
                                          gfx::Point* p) {
  //
  // If the mouse_handler was set explicitly, we need to keep
  // sending events even if it was reparented in a different
  // window. (a non explicit mouse handler is automatically
  // cleared when the control is removed from the hierarchy)
  if (explicit_mouse_handler_) {
    if (mouse_pressed_handler_->GetWidget()) {
      *p = l;
      ConvertPointToScreen(this, p);
      ConvertPointToView(NULL, mouse_pressed_handler_, p);
    } else {
      // If the mouse_pressed_handler_ is not connected, we send the
      // event in screen coordinate system
      *p = l;
      ConvertPointToScreen(this, p);
      return true;
    }
  } else {
    *p = l;
    ConvertPointToView(this, mouse_pressed_handler_, p);
  }
  return true;
}

void RootView::UpdateCursor(const MouseEvent& e) {
  gfx::NativeCursor cursor = NULL;
  View* v = GetViewForPoint(e.location());
  if (v && v != this) {
    gfx::Point l(e.location());
    View::ConvertPointToView(this, v, &l);
    cursor = v->GetCursorForPoint(e.GetType(), l.x(), l.y());
  }
  SetActiveCursor(cursor);
}

bool RootView::OnMouseDragged(const MouseEvent& e) {
  UpdateCursor(e);

  if (mouse_pressed_handler_) {
    SetMouseLocationAndFlags(e);

    gfx::Point p;
    ConvertPointToMouseHandler(e.location(), &p);
    MouseEvent mouse_event(e.GetType(), p.x(), p.y(), e.GetFlags());
    return mouse_pressed_handler_->ProcessMouseDragged(mouse_event, &drag_info);
  }
  return false;
}

void RootView::OnMouseReleased(const MouseEvent& e, bool canceled) {
  UpdateCursor(e);

  if (mouse_pressed_handler_) {
    gfx::Point p;
    ConvertPointToMouseHandler(e.location(), &p);
    MouseEvent mouse_released(e.GetType(), p.x(), p.y(), e.GetFlags());
    // We allow the view to delete us from ProcessMouseReleased. As such,
    // configure state such that we're done first, then call View.
    View* mouse_pressed_handler = mouse_pressed_handler_;
    mouse_pressed_handler_ = NULL;
    explicit_mouse_handler_ = false;
    mouse_pressed_handler->ProcessMouseReleased(mouse_released, canceled);
    // WARNING: we may have been deleted.
  }
}

void RootView::OnMouseMoved(const MouseEvent& e) {
  View* v = GetViewForPoint(e.location());
  // Find the first enabled view.
  while (v && !v->IsEnabled())
    v = v->GetParent();
  if (v && v != this) {
    if (v != mouse_move_handler_) {
      if (mouse_move_handler_ != NULL) {
        MouseEvent exited_event(Event::ET_MOUSE_EXITED, 0, 0, 0);
        mouse_move_handler_->OnMouseExited(exited_event);
      }

      mouse_move_handler_ = v;

      MouseEvent entered_event(Event::ET_MOUSE_ENTERED,
                               this,
                               mouse_move_handler_,
                               e.location(),
                               0);
      mouse_move_handler_->OnMouseEntered(entered_event);
    }
    MouseEvent moved_event(Event::ET_MOUSE_MOVED,
                           this,
                           mouse_move_handler_,
                           e.location(),
                           0);
    mouse_move_handler_->OnMouseMoved(moved_event);

    gfx::NativeCursor cursor = mouse_move_handler_->GetCursorForPoint(
        moved_event.GetType(), moved_event.x(), moved_event.y());
    SetActiveCursor(cursor);
  } else if (mouse_move_handler_ != NULL) {
    MouseEvent exited_event(Event::ET_MOUSE_EXITED, 0, 0, 0);
    mouse_move_handler_->OnMouseExited(exited_event);
    SetActiveCursor(NULL);
  }
}

void RootView::ProcessOnMouseExited() {
  if (mouse_move_handler_ != NULL) {
    MouseEvent exited_event(Event::ET_MOUSE_EXITED, 0, 0, 0);
    mouse_move_handler_->OnMouseExited(exited_event);
    mouse_move_handler_ = NULL;
  }
}

void RootView::SetMouseHandler(View *new_mh) {
  // If we're clearing the mouse handler, clear explicit_mouse_handler as well.
  explicit_mouse_handler_ = (new_mh != NULL);
  mouse_pressed_handler_ = new_mh;
}

void RootView::OnWidgetCreated() {
#if defined(OS_WIN)
  DCHECK(!drop_target_.get());
  drop_target_ = new RootViewDropTarget(this);
#else
  // TODO(port): Port RootViewDropTarget and this goes away.
  NOTIMPLEMENTED();
#endif
}

void RootView::OnWidgetDestroyed() {
#if defined(OS_WIN)
  if (drop_target_.get()) {
    RevokeDragDrop(GetWidget()->GetNativeView());
    drop_target_ = NULL;
  }
#else
  // TODO(port): Port RootViewDropTarget and this goes away.
  NOTIMPLEMENTED();
#endif
}

void RootView::ProcessMouseDragCanceled() {
  if (mouse_pressed_handler_) {
    // Synthesize a release event.
    MouseEvent release_event(Event::ET_MOUSE_RELEASED, last_mouse_event_x_,
                             last_mouse_event_y_, last_mouse_event_flags_);
    OnMouseReleased(release_event, true);
  }
}

void RootView::FocusView(View* view) {
  if (view != GetFocusedView()) {
#if defined(OS_WIN)
    FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager) << "No Focus Manager for Window " <<
        (GetWidget() ? GetWidget()->GetNativeView() : 0);
    if (!focus_manager)
      return;

    View* prev_focused_view = focus_manager->GetFocusedView();
    focus_manager->SetFocusedView(view);
#else
    // TODO(port): Port the focus manager and this goes away.
    NOTIMPLEMENTED();
#endif
  }
}

View* RootView::GetFocusedView() {
  FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    // We may not have a FocusManager when the window that contains us is being
    // deleted. Sadly we cannot wait for the window to be destroyed before we
    // remove the FocusManager (see xp_frame.cc for more info).
    return NULL;
  }

  // Make sure the focused view belongs to this RootView's view hierarchy.
  View* view = focus_manager->GetFocusedView();
  if (view && (view->GetRootView() == this))
    return view;
  return NULL;
}

View* RootView::FindNextFocusableView(View* starting_view,
                                      bool reverse,
                                      Direction direction,
                                      bool check_starting_view,
                                      FocusTraversable** focus_traversable,
                                      View** focus_traversable_view) {
  *focus_traversable = NULL;
  *focus_traversable_view = NULL;

  if (GetChildViewCount() == 0) {
    NOTREACHED();
    // Nothing to focus on here.
    return NULL;
  }

  if (!starting_view) {
    // Default to the first/last child
    starting_view = reverse ? GetChildViewAt(GetChildViewCount() - 1) :
                              GetChildViewAt(0) ;
    // If there was no starting view, then the one we select is a potential
    // focus candidate.
    check_starting_view = true;
  } else {
    // The starting view should be part of this RootView.
    DCHECK(IsParentOf(starting_view));
  }

  View* v = NULL;
  if (!reverse) {
    v = FindNextFocusableViewImpl(starting_view, check_starting_view,
                                  true,
                                  (direction == DOWN) ? true : false,
                                  starting_view->GetGroup(),
                                  focus_traversable,
                                  focus_traversable_view);
  } else {
    // If the starting view is focusable, we don't want to go down, as we are
    // traversing the view hierarchy tree bottom-up.
    bool can_go_down = (direction == DOWN) && !starting_view->IsFocusable();
    v = FindPreviousFocusableViewImpl(starting_view, check_starting_view,
                                      true,
                                      can_go_down,
                                      starting_view->GetGroup(),
                                      focus_traversable,
                                      focus_traversable_view);
  }

  // Doing some sanity checks.
  if (v) {
    DCHECK(v->IsFocusable());
    return v;
  }
  if (*focus_traversable) {
    DCHECK(*focus_traversable_view);
    return NULL;
  }
  // Nothing found.
  return NULL;
}

// Strategy for finding the next focusable view:
// - keep going down the first child, stop when you find a focusable view or
//   a focus traversable view (in that case return it) or when you reach a view
//   with no children.
// - go to the right sibling and start the search from there (by invoking
//   FindNextFocusableViewImpl on that view).
// - if the view has no right sibling, go up the parents until you find a parent
//   with a right sibling and start the search from there.
View* RootView::FindNextFocusableViewImpl(View* starting_view,
                                          bool check_starting_view,
                                          bool can_go_up,
                                          bool can_go_down,
                                          int skip_group_id,
                                          FocusTraversable** focus_traversable,
                                          View** focus_traversable_view) {
  if (check_starting_view) {
    if (IsViewFocusableCandidate(starting_view, skip_group_id))
      return FindSelectedViewForGroup(starting_view);

    *focus_traversable = starting_view->GetFocusTraversable();
    if (*focus_traversable) {
      *focus_traversable_view = starting_view;
      return NULL;
    }
  }

  // First let's try the left child.
  if (can_go_down) {
    if (starting_view->GetChildViewCount() > 0) {
      View* v = FindNextFocusableViewImpl(starting_view->GetChildViewAt(0),
                                          true, false, true, skip_group_id,
                                          focus_traversable,
                                          focus_traversable_view);
      if (v || *focus_traversable)
        return v;
    }
  }

  // Then try the right sibling.
  View* sibling = starting_view->GetNextFocusableView();
  if (sibling) {
    View* v = FindNextFocusableViewImpl(sibling,
                                        true, false, true, skip_group_id,
                                        focus_traversable,
                                        focus_traversable_view);
    if (v || *focus_traversable)
      return v;
  }

  // Then go up to the parent sibling.
  if (can_go_up) {
    View* parent = starting_view->GetParent();
    while (parent) {
      sibling = parent->GetNextFocusableView();
      if (sibling) {
        return FindNextFocusableViewImpl(sibling,
                                         true, true, true,
                                         skip_group_id,
                                         focus_traversable,
                                         focus_traversable_view);
      }
      parent = parent->GetParent();
    }
  }

  // We found nothing.
  return NULL;
}

// Strategy for finding the previous focusable view:
// - keep going down on the right until you reach a view with no children, if it
//   it is a good candidate return it.
// - start the search on the left sibling.
// - if there are no left sibling, start the search on the parent (without going
//   down).
View* RootView::FindPreviousFocusableViewImpl(
    View* starting_view,
    bool check_starting_view,
    bool can_go_up,
    bool can_go_down,
    int skip_group_id,
    FocusTraversable** focus_traversable,
    View** focus_traversable_view) {
  // Let's go down and right as much as we can.
  if (can_go_down) {
    // Before we go into the direct children, we have to check if this view has
    // a FocusTraversable.
    *focus_traversable = starting_view->GetFocusTraversable();
    if (*focus_traversable) {
      *focus_traversable_view = starting_view;
      return NULL;
    }

    if (starting_view->GetChildViewCount() > 0) {
      View* view =
          starting_view->GetChildViewAt(starting_view->GetChildViewCount() - 1);
      View* v = FindPreviousFocusableViewImpl(view, true, false, true,
                                              skip_group_id,
                                              focus_traversable,
                                              focus_traversable_view);
      if (v || *focus_traversable)
        return v;
    }
  }

  // Then look at this view. Here, we do not need to see if the view has
  // a FocusTraversable, since we do not want to go down any more.
  if (check_starting_view &&
      IsViewFocusableCandidate(starting_view, skip_group_id)) {
    return FindSelectedViewForGroup(starting_view);
  }

  // Then try the left sibling.
  View* sibling = starting_view->GetPreviousFocusableView();
  if (sibling) {
    return FindPreviousFocusableViewImpl(sibling,
                                         true, true, true,
                                         skip_group_id,
                                         focus_traversable,
                                         focus_traversable_view);
  }

  // Then go up the parent.
  if (can_go_up) {
    View* parent = starting_view->GetParent();
    if (parent)
      return FindPreviousFocusableViewImpl(parent,
                                           true, true, false,
                                           skip_group_id,
                                           focus_traversable,
                                           focus_traversable_view);
  }

  // We found nothing.
  return NULL;
}

FocusTraversable* RootView::GetFocusTraversableParent() {
  return focus_traversable_parent_;
}

void RootView::SetFocusTraversableParent(FocusTraversable* focus_traversable) {
  DCHECK(focus_traversable != this);
  focus_traversable_parent_ = focus_traversable;
}

View* RootView::GetFocusTraversableParentView() {
  return focus_traversable_parent_view_;
}

void RootView::SetFocusTraversableParentView(View* view) {
  focus_traversable_parent_view_ = view;
}

// static
View* RootView::FindSelectedViewForGroup(View* view) {
  if (view->IsGroupFocusTraversable() ||
      view->GetGroup() == -1)  // No group for that view.
    return view;

  View* selected_view = view->GetSelectedViewForGroup(view->GetGroup());
  if (selected_view)
    return selected_view;

  // No view selected for that group, default to the specified view.
  return view;
}

// static
bool RootView::IsViewFocusableCandidate(View* v, int skip_group_id) {
  return v->IsFocusable() &&
      (v->IsGroupFocusTraversable() || skip_group_id == -1 ||
       v->GetGroup() != skip_group_id);
}

bool RootView::ProcessKeyEvent(const KeyEvent& event) {
  bool consumed = false;

  View* v = GetFocusedView();
#if defined(OS_WIN)
  // Special case to handle right-click context menus triggered by the
  // keyboard.
  if (v && v->IsEnabled() && ((event.GetCharacter() == VK_APPS) ||
      (event.GetCharacter() == VK_F10 && event.IsShiftDown()))) {
    gfx::Point screen_loc = v->GetKeyboardContextMenuLocation();
    v->ShowContextMenu(screen_loc.x(), screen_loc.y(), false);
    return true;
  }
#else
  // TODO(port): The above block needs the VK_* refactored out.
  NOTIMPLEMENTED();
#endif

  for (; v && v != this && !consumed; v = v->GetParent()) {
    consumed = (event.GetType() == Event::ET_KEY_PRESSED) ?
        v->OnKeyPressed(event) : v->OnKeyReleased(event);
  }

  if (!consumed && default_keyboard_handler_) {
    consumed = (event.GetType() == Event::ET_KEY_PRESSED) ?
        default_keyboard_handler_->OnKeyPressed(event) :
        default_keyboard_handler_->OnKeyReleased(event);
  }

  return consumed;
}

bool RootView::ProcessMouseWheelEvent(const MouseWheelEvent& e) {
  View* v;
  bool consumed = false;
  if (GetFocusedView()) {
    for (v = GetFocusedView();
         v && v != this && !consumed; v = v->GetParent()) {
      consumed = v->OnMouseWheel(e);
    }
  }

  if (!consumed && default_keyboard_handler_) {
    consumed = default_keyboard_handler_->OnMouseWheel(e);
  }
  return consumed;
}

void RootView::SetDefaultKeyboardHandler(View* v) {
  default_keyboard_handler_ = v;
}

bool RootView::IsVisibleInRootView() const {
  return IsVisible();
}

void RootView::ViewBoundsChanged(View* view, bool size_changed,
                                 bool position_changed) {
  DCHECK(view && (size_changed || position_changed));
  if (!view->descendants_to_notify_.get())
    return;

  for (std::vector<View*>::iterator i = view->descendants_to_notify_->begin();
       i != view->descendants_to_notify_->end(); ++i) {
    (*i)->VisibleBoundsInRootChanged();
  }
}

void RootView::RegisterViewForVisibleBoundsNotification(View* view) {
  DCHECK(view);
  if (view->registered_for_visible_bounds_notification_)
    return;
  view->registered_for_visible_bounds_notification_ = true;
  View* ancestor = view->GetParent();
  while (ancestor) {
    ancestor->AddDescendantToNotify(view);
    ancestor = ancestor->GetParent();
  }
}

void RootView::UnregisterViewForVisibleBoundsNotification(View* view) {
  DCHECK(view);
  if (!view->registered_for_visible_bounds_notification_)
    return;
  view->registered_for_visible_bounds_notification_ = false;
  View* ancestor = view->GetParent();
  while (ancestor) {
    ancestor->RemoveDescendantToNotify(view);
    ancestor = ancestor->GetParent();
  }
}

void RootView::SetMouseLocationAndFlags(const MouseEvent& e) {
  last_mouse_event_flags_ = e.GetFlags();
  last_mouse_event_x_ = e.x();
  last_mouse_event_y_ = e.y();
}

std::string RootView::GetClassName() const {
  return kViewClassName;
}

void RootView::ClearPaintRect() {
  invalid_rect_.SetRect(0, 0, 0, 0);

  // This painting has been done. Reset the urgent flag.
  invalid_rect_urgent_ = false;

  // If a pending_paint_task_ does Run(), we don't need to do anything.
  paint_task_needed_ = false;
}

/////////////////////////////////////////////////////////////////////////////
//
// RootView - accessibility
//
/////////////////////////////////////////////////////////////////////////////

bool RootView::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_APPLICATION;
  return true;
}

bool RootView::GetAccessibleName(std::wstring* name) {
  if (!accessible_name_.empty()) {
    *name = accessible_name_;
    return true;
  }
  return false;
}

void RootView::SetAccessibleName(const std::wstring& name) {
  accessible_name_.assign(name);
}

View* RootView::GetDragView() {
  return drag_view_;
}

void RootView::SetActiveCursor(gfx::NativeCursor cursor) {
#if defined(OS_WIN)
  if (cursor) {
    previous_cursor_ = ::SetCursor(cursor);
  } else if (previous_cursor_) {
    ::SetCursor(previous_cursor_);
    previous_cursor_ = NULL;
  }
#elif defined(OS_LINUX)
  gdk_window_set_cursor(GetWidget()->GetNativeView()->window, cursor);
  if (cursor)
    gdk_cursor_destroy(cursor);
#endif
}

}  // namespace views
