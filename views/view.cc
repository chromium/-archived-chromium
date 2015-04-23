// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

#include <algorithm>
#ifndef NDEBUG
#include <iostream>
#endif

#include "app/drag_drop_types.h"
#include "app/gfx/canvas.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_handle.h"
#include "base/string_util.h"
#include "third_party/skia/include/core/SkShader.h"
#include "views/background.h"
#include "views/layout_manager.h"
#include "views/views_delegate.h"
#include "views/widget/root_view.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget.h"
#include "views/window/window.h"
#if defined(OS_WIN)
#include "views/accessibility/view_accessibility_wrapper.h"
#endif

namespace views {

// static
ViewsDelegate* ViewsDelegate::views_delegate = NULL;

// static
char View::kViewClassName[] = "views/View";

/////////////////////////////////////////////////////////////////////////////
//
// View - constructors, destructors, initialization
//
/////////////////////////////////////////////////////////////////////////////

View::View()
    : id_(0),
      group_(-1),
      enabled_(true),
      focusable_(false),
      bounds_(0, 0, 0, 0),
      parent_(NULL),
      is_visible_(true),
      is_parent_owned_(true),
      notify_when_visible_bounds_in_root_changes_(false),
      registered_for_visible_bounds_notification_(false),
      next_focusable_view_(NULL),
      previous_focusable_view_(NULL),
      registered_accelerator_count_(0),
      context_menu_controller_(NULL),
#if defined(OS_WIN)
      accessibility_(NULL),
#endif
      drag_controller_(NULL),
      ui_mirroring_is_enabled_for_rtl_languages_(true),
      flip_canvas_on_paint_for_rtl_ui_(false) {
}

View::~View() {
  int c = static_cast<int>(child_views_.size());
  while (--c >= 0) {
    if (child_views_[c]->IsParentOwned())
      delete child_views_[c];
    else
      child_views_[c]->SetParent(NULL);
  }
}

/////////////////////////////////////////////////////////////////////////////
//
// View - sizing
//
/////////////////////////////////////////////////////////////////////////////

gfx::Rect View::GetBounds(PositionMirroringSettings settings) const {
  gfx::Rect bounds(bounds_);

  // If the parent uses an RTL UI layout and if we are asked to transform the
  // bounds to their mirrored position if necessary, then we should shift the
  // rectangle appropriately.
  if (settings == APPLY_MIRRORING_TRANSFORMATION)
    bounds.set_x(MirroredX());

  return bounds;
}

// y(), width() and height() are agnostic to the RTL UI layout of the
// parent view. x(), on the other hand, is not.
int View::GetX(PositionMirroringSettings settings) const {
  return settings == IGNORE_MIRRORING_TRANSFORMATION ? x() : MirroredX();
}

void View::SetBounds(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;

  gfx::Rect prev = bounds_;
  bounds_ = bounds;
  DidChangeBounds(prev, bounds_);

  RootView* root = GetRootView();
  if (root) {
    bool size_changed = prev.size() != bounds_.size();
    bool position_changed = prev.origin() != bounds_.origin();
    if (size_changed || position_changed)
      root->ViewBoundsChanged(this, size_changed, position_changed);
  }
}

gfx::Rect View::GetLocalBounds(bool include_border) const {
  if (include_border || !border_.get())
    return gfx::Rect(0, 0, width(), height());

  gfx::Insets insets;
  border_->GetInsets(&insets);
  return gfx::Rect(insets.left(), insets.top(),
                   std::max(0, width() - insets.width()),
                   std::max(0, height() - insets.height()));
}

gfx::Point View::GetPosition() const {
  return gfx::Point(GetX(APPLY_MIRRORING_TRANSFORMATION), y());
}

gfx::Size View::GetPreferredSize() {
  if (layout_manager_.get())
    return layout_manager_->GetPreferredSize(this);
  return gfx::Size();
}

void View::SizeToPreferredSize() {
  gfx::Size prefsize = GetPreferredSize();
  if ((prefsize.width() != width()) || (prefsize.height() != height()))
    SetBounds(x(), y(), prefsize.width(), prefsize.height());
}

void View::PreferredSizeChanged() {
  if (parent_)
    parent_->ChildPreferredSizeChanged(this);
}

gfx::Size View::GetMinimumSize() {
  return GetPreferredSize();
}

int View::GetHeightForWidth(int w) {
  if (layout_manager_.get())
    return layout_manager_->GetPreferredHeightForWidth(this, w);
  return GetPreferredSize().height();
}

void View::DidChangeBounds(const gfx::Rect& previous,
                           const gfx::Rect& current) {
  Layout();
}

void View::ScrollRectToVisible(int x, int y, int width, int height) {
  View* parent = GetParent();

  // We must take RTL UI mirroring into account when adjusting the position of
  // the region.
  if (parent)
    parent->ScrollRectToVisible(
        GetX(APPLY_MIRRORING_TRANSFORMATION) + x, View::y() + y, width, height);
}

/////////////////////////////////////////////////////////////////////////////
//
// View - layout
//
/////////////////////////////////////////////////////////////////////////////

void View::Layout() {
  // Layout child Views
  if (layout_manager_.get()) {
    layout_manager_->Layout(this);
    SchedulePaint();
    // TODO(beng): We believe the right thing to do here is return since the
    //             layout manager should be handling things, but it causes
    //             regressions (missing options from Options dialog and a hang
    //             in interactive_ui_tests).
  }

  // Lay out contents of child Views
  int child_count = GetChildViewCount();
  for (int i = 0; i < child_count; ++i) {
    View* child = GetChildViewAt(i);
    child->Layout();
  }
}

LayoutManager* View::GetLayoutManager() const {
  return layout_manager_.get();
}

void View::SetLayoutManager(LayoutManager* layout_manager) {
  if (layout_manager_.get()) {
    layout_manager_->Uninstalled(this);
  }
  layout_manager_.reset(layout_manager);
  if (layout_manager_.get()) {
    layout_manager_->Installed(this);
  }
}

bool View::UILayoutIsRightToLeft() const {
  return (ui_mirroring_is_enabled_for_rtl_languages_ &&
          l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT);
}

////////////////////////////////////////////////////////////////////////////////
//
// View - Right-to-left UI layout
//
////////////////////////////////////////////////////////////////////////////////

inline int View::MirroredX() const {
  // TODO(beng): reimplement in terms of MirroredLeftPointForRect.
  View* parent = GetParent();
  if (parent && parent->UILayoutIsRightToLeft())
    return parent->width() - x() - width();
  return x();
}

int View::MirroredLeftPointForRect(const gfx::Rect& bounds) const {
  if (!UILayoutIsRightToLeft()) {
    return bounds.x();
  }
  return width() - bounds.x() - bounds.width();
}

////////////////////////////////////////////////////////////////////////////////
//
// View - states
//
////////////////////////////////////////////////////////////////////////////////

bool View::IsEnabled() const {
  return enabled_;
}

void View::SetEnabled(bool state) {
  if (enabled_ != state) {
    enabled_ = state;
    SchedulePaint();
  }
}

bool View::IsFocusable() const {
  return focusable_ && enabled_ && is_visible_;
}

void View::SetFocusable(bool focusable) {
  focusable_ = focusable;
}

FocusManager* View::GetFocusManager() {
  Widget* widget = GetWidget();
  return widget ? widget->GetFocusManager() : NULL;
}

bool View::HasFocus() {
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    return focus_manager->GetFocusedView() == this;
  return false;
}

void View::SetHotTracked(bool flag) {
}

/////////////////////////////////////////////////////////////////////////////
//
// View - painting
//
/////////////////////////////////////////////////////////////////////////////

void View::SchedulePaint(const gfx::Rect& r, bool urgent) {
  if (!IsVisible())
    return;

  if (parent_) {
    // Translate the requested paint rect to the parent's coordinate system
    // then pass this notification up to the parent.
    gfx::Rect paint_rect = r;
    paint_rect.Offset(GetPosition());
    parent_->SchedulePaint(paint_rect, urgent);
  }
}

void View::SchedulePaint() {
  SchedulePaint(GetLocalBounds(true), false);
}

void View::SchedulePaint(int x, int y, int w, int h) {
  SchedulePaint(gfx::Rect(x, y, w, h), false);
}

void View::Paint(gfx::Canvas* canvas) {
  PaintBackground(canvas);
  PaintFocusBorder(canvas);
  PaintBorder(canvas);
}

void View::PaintBackground(gfx::Canvas* canvas) {
  if (background_.get())
    background_->Paint(canvas, this);
}

void View::PaintBorder(gfx::Canvas* canvas) {
  if (border_.get())
    border_->Paint(*this, canvas);
}

void View::PaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && IsFocusable())
    canvas->DrawFocusRect(0, 0, width(), height());
}

void View::PaintChildren(gfx::Canvas* canvas) {
  int i, c;
  for (i = 0, c = GetChildViewCount(); i < c; ++i) {
    View* child = GetChildViewAt(i);
    if (!child) {
      NOTREACHED() << "Should not have a NULL child View for index in bounds";
      continue;
    }
    child->ProcessPaint(canvas);
  }
}

void View::ProcessPaint(gfx::Canvas* canvas) {
  if (!IsVisible()) {
    return;
  }

  // We're going to modify the canvas, save it's state first.
  canvas->save();

  // Paint this View and its children, setting the clip rect to the bounds
  // of this View and translating the origin to the local bounds' top left
  // point.
  //
  // Note that the X (or left) position we pass to ClipRectInt takes into
  // consideration whether or not the view uses a right-to-left layout so that
  // we paint our view in its mirrored position if need be.
  if (canvas->ClipRectInt(MirroredX(), y(), width(), height())) {
    // Non-empty clip, translate the graphics such that 0,0 corresponds to
    // where this view is located (related to its parent).
    canvas->TranslateInt(MirroredX(), y());

    // Save the state again, so that any changes don't effect PaintChildren.
    canvas->save();

    // If the View we are about to paint requested the canvas to be flipped, we
    // should change the transform appropriately.
    bool flip_canvas = FlipCanvasOnPaintForRTLUI();
    if (flip_canvas) {
      canvas->TranslateInt(width(), 0);
      canvas->ScaleInt(-1, 1);
      canvas->save();
    }

    Paint(canvas);

    // We must undo the canvas mirroring once the View is done painting so that
    // we don't pass the canvas with the mirrored transform to Views that
    // didn't request the canvas to be flipped.
    if (flip_canvas) {
      canvas->restore();
    }
    canvas->restore();
    PaintChildren(canvas);
  }

  // Restore the canvas's original transform.
  canvas->restore();
}

void View::PaintNow() {
  if (!IsVisible()) {
    return;
  }

  View* view = GetParent();
  if (view)
    view->PaintNow();
}

gfx::Insets View::GetInsets() const {
  gfx::Insets insets;
  if (border_.get())
    border_->GetInsets(&insets);
  return insets;
}

gfx::NativeCursor View::GetCursorForPoint(Event::EventType event_type, int x,
                                          int y) {
  return NULL;
}

bool View::HitTest(const gfx::Point& l) const {
  if (l.x() >= 0 && l.x() < static_cast<int>(width()) &&
      l.y() >= 0 && l.y() < static_cast<int>(height())) {
    if (HasHitTestMask()) {
      gfx::Path mask;
      GetHitTestMask(&mask);
#if defined(OS_WIN)
      ScopedHRGN rgn(mask.CreateHRGN());
      return !!PtInRegion(rgn, l.x(), l.y());
#elif defined(OS_LINUX)
      GdkRegion* region = mask.CreateGdkRegion();
      bool result = gdk_region_point_in(region, l.x(), l.y());
      gdk_region_destroy(region);
      return result;
#endif
    }
    // No mask, but inside our bounds.
    return true;
  }
  // Outside our bounds.
  return false;
}

void View::SetContextMenuController(ContextMenuController* menu_controller) {
  context_menu_controller_ = menu_controller;
}

void View::ShowContextMenu(int x, int y, bool is_mouse_gesture) {
  if (!context_menu_controller_)
    return;

  context_menu_controller_->ShowContextMenu(this, x, y, is_mouse_gesture);
}

/////////////////////////////////////////////////////////////////////////////
//
// View - tree
//
/////////////////////////////////////////////////////////////////////////////

bool View::ProcessMousePressed(const MouseEvent& e, DragInfo* drag_info) {
  const bool enabled = enabled_;
  int drag_operations;
  if (enabled && e.IsOnlyLeftMouseButton() && HitTest(e.location()))
    drag_operations = GetDragOperations(e.x(), e.y());
  else
    drag_operations = 0;
  ContextMenuController* context_menu_controller =
      e.IsRightMouseButton() ? context_menu_controller_ : 0;

  const bool result = OnMousePressed(e);
  // WARNING: we may have been deleted, don't use any View variables;

  if (!enabled)
    return result;

  if (drag_operations != DragDropTypes::DRAG_NONE) {
    drag_info->PossibleDrag(e.x(), e.y());
    return true;
  }
  return !!context_menu_controller || result;
}

bool View::ProcessMouseDragged(const MouseEvent& e, DragInfo* drag_info) {
  // Copy the field, that way if we're deleted after drag and drop no harm is
  // done.
  ContextMenuController* context_menu_controller = context_menu_controller_;
  const bool possible_drag = drag_info->possible_drag;
  if (possible_drag && ExceededDragThreshold(drag_info->start_x - e.x(),
                                             drag_info->start_y - e.y())) {
    DoDrag(e, drag_info->start_x, drag_info->start_y);
  } else {
    if (OnMouseDragged(e))
      return true;
    // Fall through to return value based on context menu controller.
  }
  // WARNING: we may have been deleted.
  return (context_menu_controller != NULL) || possible_drag;
}

void View::ProcessMouseReleased(const MouseEvent& e, bool canceled) {
  if (!canceled && context_menu_controller_ && e.IsOnlyRightMouseButton()) {
    // Assume that if there is a context menu controller we won't be deleted
    // from mouse released.
    gfx::Point location(e.location());
    OnMouseReleased(e, canceled);
    if (HitTest(location)) {
      ConvertPointToScreen(this, &location);
      ShowContextMenu(location.x(), location.y(), true);
    }
  } else {
    OnMouseReleased(e, canceled);
  }
  // WARNING: we may have been deleted.
}

void View::AddChildView(View* v) {
  AddChildView(static_cast<int>(child_views_.size()), v);
}

void View::AddChildView(int index, View* v) {
  // Remove the view from its current parent if any.
  if (v->GetParent())
    v->GetParent()->RemoveChildView(v);

  // Sets the prev/next focus views.
  InitFocusSiblings(v, index);

  // Let's insert the view.
  child_views_.insert(child_views_.begin() + index, v);
  v->SetParent(this);

  for (View* p = this; p; p = p->GetParent()) {
    p->ViewHierarchyChangedImpl(false, true, this, v);
  }
  v->PropagateAddNotifications(this, v);
  UpdateTooltip();
  RootView* root = GetRootView();
  if (root)
    RegisterChildrenForVisibleBoundsNotification(root, v);

  if (layout_manager_.get())
    layout_manager_->ViewAdded(this, v);
}

View* View::GetChildViewAt(int index) const {
  return index < GetChildViewCount() ? child_views_[index] : NULL;
}

int View::GetChildViewCount() const {
  return static_cast<int>(child_views_.size());
}

void View::RemoveChildView(View* a_view) {
  DoRemoveChildView(a_view, true, true, false);
}

void View::RemoveAllChildViews(bool delete_views) {
  ViewList::iterator iter;
  while ((iter = child_views_.begin()) != child_views_.end()) {
    DoRemoveChildView(*iter, false, false, delete_views);
  }
  UpdateTooltip();
}

void View::DoRemoveChildView(View* a_view,
                             bool update_focus_cycle,
                             bool update_tool_tip,
                             bool delete_removed_view) {
#ifndef NDEBUG
  DCHECK(!IsProcessingPaint()) << "Should not be removing a child view " <<
                                  "during a paint, this will seriously " <<
                                  "mess things up!";
#endif
  DCHECK(a_view);
  const ViewList::iterator i =  find(child_views_.begin(),
                                     child_views_.end(),
                                     a_view);
  if (i != child_views_.end()) {
    if (update_focus_cycle) {
      // Let's remove the view from the focus traversal.
      View* next_focusable = a_view->next_focusable_view_;
      View* prev_focusable = a_view->previous_focusable_view_;
      if (prev_focusable)
        prev_focusable->next_focusable_view_ = next_focusable;
      if (next_focusable)
        next_focusable->previous_focusable_view_ = prev_focusable;
    }

    RootView* root = GetRootView();
    if (root)
      UnregisterChildrenForVisibleBoundsNotification(root, a_view);
    a_view->PropagateRemoveNotifications(this);
    a_view->SetParent(NULL);

    if (delete_removed_view && a_view->IsParentOwned())
      delete a_view;

    child_views_.erase(i);
  }

  if (update_tool_tip)
    UpdateTooltip();

  if (layout_manager_.get())
    layout_manager_->ViewRemoved(this, a_view);
}

void View::PropagateRemoveNotifications(View* parent) {
  int i, c;
  for (i = 0, c = GetChildViewCount(); i < c; ++i) {
    GetChildViewAt(i)->PropagateRemoveNotifications(parent);
  }

  View *t;
  for (t = this; t; t = t->GetParent()) {
    t->ViewHierarchyChangedImpl(true, false, parent, this);
  }
}

void View::PropagateAddNotifications(View* parent, View* child) {
  int i, c;
  for (i = 0, c = GetChildViewCount(); i < c; ++i) {
    GetChildViewAt(i)->PropagateAddNotifications(parent, child);
  }
  ViewHierarchyChangedImpl(true, true, parent, child);
}

void View::ThemeChanged() {
  int c = GetChildViewCount();
  for (int i = c - 1; i >= 0; --i)
    GetChildViewAt(i)->ThemeChanged();
}

#ifndef NDEBUG
bool View::IsProcessingPaint() const {
  return GetParent() && GetParent()->IsProcessingPaint();
}
#endif

gfx::Point View::GetKeyboardContextMenuLocation() {
  gfx::Rect vis_bounds = GetVisibleBounds();
  gfx::Point screen_point(vis_bounds.x() + vis_bounds.width() / 2,
                          vis_bounds.y() + vis_bounds.height() / 2);
  ConvertPointToScreen(this, &screen_point);
  return screen_point;
}

bool View::HasHitTestMask() const {
  return false;
}

void View::GetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
}

void View::ViewHierarchyChanged(bool is_add,
                                View* parent,
                                View* child) {
}

void View::ViewHierarchyChangedImpl(bool register_accelerators,
                                    bool is_add,
                                    View* parent,
                                    View* child) {
  if (register_accelerators) {
    if (is_add) {
      // If you get this registration, you are part of a subtree that has been
      // added to the view hierarchy.
      RegisterPendingAccelerators();
    } else {
      if (child == this)
        UnregisterAccelerators();
    }
  }

  ViewHierarchyChanged(is_add, parent, child);
}

void View::PropagateVisibilityNotifications(View* start, bool is_visible) {
  int i, c;
  for (i = 0, c = GetChildViewCount(); i < c; ++i) {
    GetChildViewAt(i)->PropagateVisibilityNotifications(start, is_visible);
  }
  VisibilityChanged(start, is_visible);
}

void View::VisibilityChanged(View* starting_from, bool is_visible) {
}

void View::SetNotifyWhenVisibleBoundsInRootChanges(bool value) {
  if (notify_when_visible_bounds_in_root_changes_ == value)
    return;
  notify_when_visible_bounds_in_root_changes_ = value;
  RootView* root = GetRootView();
  if (root) {
    if (value)
      root->RegisterViewForVisibleBoundsNotification(this);
    else
      root->UnregisterViewForVisibleBoundsNotification(this);
  }
}

bool View::GetNotifyWhenVisibleBoundsInRootChanges() {
  return notify_when_visible_bounds_in_root_changes_;
}

View* View::GetViewForPoint(const gfx::Point& point) {
  // Walk the child Views recursively looking for the View that most
  // tightly encloses the specified point.
  for (int i = GetChildViewCount() - 1 ; i >= 0 ; --i) {
    View* child = GetChildViewAt(i);
    if (!child->IsVisible())
      continue;

    gfx::Point point_in_child_coords(point);
    View::ConvertPointToView(this, child, &point_in_child_coords);
    if (child->HitTest(point_in_child_coords))
      return child->GetViewForPoint(point_in_child_coords);
  }
  return this;
}

Widget* View::GetWidget() const {
  // The root view holds a reference to this view hierarchy's Widget.
  return parent_ ? parent_->GetWidget() : NULL;
}

Window* View::GetWindow() const {
  Widget* widget = GetWidget();
  return widget ? widget->GetWindow() : NULL;
}

// Get the containing RootView
RootView* View::GetRootView() {
  Widget* widget = GetWidget();
  return widget ? widget->GetRootView() : NULL;
}

View* View::GetViewByID(int id) const {
  if (id == id_)
    return const_cast<View*>(this);

  int view_count = GetChildViewCount();
  for (int i = 0; i < view_count; ++i) {
    View* child = GetChildViewAt(i);
    View* view = child->GetViewByID(id);
    if (view)
      return view;
  }
  return NULL;
}

void View::GetViewsWithGroup(int group_id, std::vector<View*>* out) {
  if (group_ == group_id)
    out->push_back(this);

  int view_count = GetChildViewCount();
  for (int i = 0; i < view_count; ++i)
    GetChildViewAt(i)->GetViewsWithGroup(group_id, out);
}

View* View::GetSelectedViewForGroup(int group_id) {
  std::vector<View*> views;
  GetRootView()->GetViewsWithGroup(group_id, &views);
  if (views.size() > 0)
    return views[0];
  else
    return NULL;
}

void View::SetID(int id) {
  id_ = id;
}

int View::GetID() const {
  return id_;
}

void View::SetGroup(int gid) {
  group_ = gid;
}

int View::GetGroup() const {
  return group_;
}

void View::SetParent(View* parent) {
  if (parent != parent_) {
    parent_ = parent;
  }
}

bool View::IsParentOf(View* v) const {
  DCHECK(v);
  View* parent = v->GetParent();
  while (parent) {
    if (this == parent)
      return true;
    parent = parent->GetParent();
  }
  return false;
}

int View::GetChildIndex(View* v) const {
  for (int i = 0; i < GetChildViewCount(); i++) {
    if (v == GetChildViewAt(i))
      return i;
  }
  return -1;
}

///////////////////////////////////////////////////////////////////////////////
//
// View - focus
//
///////////////////////////////////////////////////////////////////////////////

View* View::GetNextFocusableView() {
  return next_focusable_view_;
}

View* View::GetPreviousFocusableView() {
  return previous_focusable_view_;
}

void View::SetNextFocusableView(View* view) {
  view->previous_focusable_view_ = this;
  next_focusable_view_ = view;
}

void View::InitFocusSiblings(View* v, int index) {
  int child_count = static_cast<int>(child_views_.size());

  if (child_count == 0) {
    v->next_focusable_view_ = NULL;
    v->previous_focusable_view_ = NULL;
  } else {
    if (index == child_count) {
      // We are inserting at the end, but the end of the child list may not be
      // the last focusable element. Let's try to find an element with no next
      // focusable element to link to.
      View* last_focusable_view = NULL;
      for (std::vector<View*>::iterator iter = child_views_.begin();
           iter != child_views_.end(); ++iter) {
          if (!(*iter)->next_focusable_view_) {
            last_focusable_view = *iter;
            break;
          }
      }
      if (last_focusable_view == NULL) {
        // Hum... there is a cycle in the focus list. Let's just insert ourself
        // after the last child.
        View* prev = child_views_[index - 1];
        v->previous_focusable_view_ = prev;
        v->next_focusable_view_ = prev->next_focusable_view_;
        prev->next_focusable_view_->previous_focusable_view_ = v;
        prev->next_focusable_view_ = v;
      } else {
        last_focusable_view->next_focusable_view_ = v;
        v->next_focusable_view_ = NULL;
        v->previous_focusable_view_ = last_focusable_view;
      }
    } else {
      View* prev = child_views_[index]->GetPreviousFocusableView();
      v->previous_focusable_view_ = prev;
      v->next_focusable_view_ = child_views_[index];
      if (prev)
        prev->next_focusable_view_ = v;
      child_views_[index]->previous_focusable_view_ = v;
    }
  }
}

#ifndef NDEBUG
void View::PrintViewHierarchy() {
  PrintViewHierarchyImp(0);
}

void View::PrintViewHierarchyImp(int indent) {
  std::wostringstream buf;
  int ind = indent;
  while (ind-- > 0)
    buf << L' ';
  buf << UTF8ToWide(GetClassName());
  buf << L' ';
  buf << GetID();
  buf << L' ';
  buf << bounds_.x() << L"," << bounds_.y() << L",";
  buf << bounds_.right() << L"," << bounds_.bottom();
  buf << L' ';
  buf << this;

  LOG(INFO) << buf.str();
  std::cout << buf.str() << std::endl;

  for (int i = 0; i < GetChildViewCount(); ++i) {
    GetChildViewAt(i)->PrintViewHierarchyImp(indent + 2);
  }
}


void View::PrintFocusHierarchy() {
  PrintFocusHierarchyImp(0);
}

void View::PrintFocusHierarchyImp(int indent) {
  std::wostringstream buf;
  int ind = indent;
  while (ind-- > 0)
    buf << L' ';
  buf << UTF8ToWide(GetClassName());
  buf << L' ';
  buf << GetID();
  buf << L' ';
  buf << GetClassName().c_str();
  buf << L' ';
  buf << this;

  LOG(INFO) << buf.str();
  std::cout << buf.str() << std::endl;

  if (GetChildViewCount() > 0)
    GetChildViewAt(0)->PrintFocusHierarchyImp(indent + 2);

  View* v = GetNextFocusableView();
  if (v)
    v->PrintFocusHierarchyImp(indent);
}
#endif

////////////////////////////////////////////////////////////////////////////////
//
// View - accelerators
//
////////////////////////////////////////////////////////////////////////////////

void View::AddAccelerator(const Accelerator& accelerator) {
  if (!accelerators_.get())
    accelerators_.reset(new std::vector<Accelerator>());

  std::vector<Accelerator>::iterator iter =
      std::find(accelerators_->begin(), accelerators_->end(), accelerator);
  DCHECK(iter == accelerators_->end())
      << "Registering the same accelerator multiple times";

  accelerators_->push_back(accelerator);
  RegisterPendingAccelerators();
}

void View::RemoveAccelerator(const Accelerator& accelerator) {
  std::vector<Accelerator>::iterator iter;
  if (!accelerators_.get() ||
      ((iter = std::find(accelerators_->begin(), accelerators_->end(),
          accelerator)) == accelerators_->end())) {
    NOTREACHED() << "Removing non-existing accelerator";
    return;
  }

  size_t index = iter - accelerators_->begin();
  accelerators_->erase(iter);
  if (index >= registered_accelerator_count_) {
    // The accelerator is not registered to FocusManager.
    return;
  }
  --registered_accelerator_count_;

  RootView* root_view = GetRootView();
  if (!root_view) {
    // We are not part of a view hierarchy, so there is nothing to do as we
    // removed ourselves from accelerators_, we won't be registered when added
    // to one.
    return;
  }

  // TODO(port): Fix this once we have a FocusManger for Linux.
#if defined(OS_WIN)
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager) {
    // We may not have a FocusManager if the window containing us is being
    // closed, in which case the FocusManager is being deleted so there is
    // nothing to unregister.
    focus_manager->UnregisterAccelerator(accelerator, this);
  }
#endif
}

void View::ResetAccelerators() {
  if (accelerators_.get())
    UnregisterAccelerators();
}

void View::RegisterPendingAccelerators() {
  if (!accelerators_.get() ||
      registered_accelerator_count_ == accelerators_->size()) {
    // No accelerators are waiting for registration.
    return;
  }

  RootView* root_view = GetRootView();
  if (!root_view) {
    // We are not yet part of a view hierarchy, we'll register ourselves once
    // added to one.
    return;
  }

  // TODO(port): Fix this once we have a FocusManger for Linux.
#if defined(OS_WIN)
  FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    // Some crash reports seem to show that we may get cases where we have no
    // focus manager (see bug #1291225).  This should never be the case, just
    // making sure we don't crash.
    NOTREACHED();
    return;
  }
  std::vector<Accelerator>::const_iterator iter;
  for (iter = accelerators_->begin() + registered_accelerator_count_;
       iter != accelerators_->end(); ++iter) {
    focus_manager->RegisterAccelerator(*iter, this);
  }
  registered_accelerator_count_ = accelerators_->size();
#endif
}

void View::UnregisterAccelerators() {
  if (!accelerators_.get())
    return;

  RootView* root_view = GetRootView();
  if (root_view) {
    // TODO(port): Fix this once we have a FocusManger for Linux.
#if defined(OS_WIN)
    FocusManager* focus_manager = GetFocusManager();
    if (focus_manager) {
      // We may not have a FocusManager if the window containing us is being
      // closed, in which case the FocusManager is being deleted so there is
      // nothing to unregister.
      focus_manager->UnregisterAccelerators(this);
    }
    accelerators_->clear();
    accelerators_.reset();
    registered_accelerator_count_ = 0;
#endif
  }
}

int View::GetDragOperations(int press_x, int press_y) {
  if (!drag_controller_)
    return DragDropTypes::DRAG_NONE;
  return drag_controller_->GetDragOperations(this, press_x, press_y);
}

void View::WriteDragData(int press_x, int press_y, OSExchangeData* data) {
  DCHECK(drag_controller_);
  drag_controller_->WriteDragData(this, press_x, press_y, data);
}

void View::OnDragDone() {
}

bool View::InDrag() {
  RootView* root_view = GetRootView();
  return root_view ? (root_view->GetDragView() == this) : false;
}

// static
void View::ConvertPointToView(const View* src,
                              const View* dst,
                              gfx::Point* point) {
  ConvertPointToView(src, dst, point, true);
}

// static
void View::ConvertPointToView(const View* src,
                              const View* dst,
                              gfx::Point* point,
                              bool try_other_direction) {
  // src can be NULL
  DCHECK(dst);
  DCHECK(point);

  const View* v;
  gfx::Point offset;

  for (v = dst; v && v != src; v = v->GetParent()) {
    offset.SetPoint(offset.x() + v->GetX(APPLY_MIRRORING_TRANSFORMATION),
                    offset.y() + v->y());
  }

  // The source was not found. The caller wants a conversion
  // from a view to a transitive parent.
  if (src && v == NULL && try_other_direction) {
    gfx::Point p;
    // note: try_other_direction is force to FALSE so we don't
    // end up in an infinite recursion should both src and dst
    // are not parented.
    ConvertPointToView(dst, src, &p, false);
    // since the src and dst are inverted, p should also be negated
    point->SetPoint(point->x() - p.x(), point->y() - p.y());
  } else {
    point->SetPoint(point->x() - offset.x(), point->y() - offset.y());

    // If src is NULL, sp is in the screen coordinate system
    if (src == NULL) {
      Widget* widget = dst->GetWidget();
      if (widget) {
        gfx::Rect b;
        widget->GetBounds(&b, false);
        point->SetPoint(point->x() - b.x(), point->y() - b.y());
      }
    }
  }
}

// static
void View::ConvertPointToWidget(const View* src, gfx::Point* p) {
  DCHECK(src);
  DCHECK(p);

  gfx::Point offset;
  for (const View* v = src; v; v = v->GetParent()) {
    offset.set_x(offset.x() + v->GetX(APPLY_MIRRORING_TRANSFORMATION));
    offset.set_y(offset.y() + v->y());
  }
  p->SetPoint(p->x() + offset.x(), p->y() + offset.y());
}

// static
void View::ConvertPointFromWidget(const View* dest, gfx::Point* p) {
  gfx::Point t;
  ConvertPointToWidget(dest, &t);
  p->SetPoint(p->x() - t.x(), p->y() - t.y());
}

// static
void View::ConvertPointToScreen(const View* src, gfx::Point* p) {
  DCHECK(src);
  DCHECK(p);

  // If the view is not connected to a tree, there's nothing we can do.
  Widget* widget = src->GetWidget();
  if (widget) {
    ConvertPointToWidget(src, p);
    gfx::Rect r;
    widget->GetBounds(&r, false);
    p->SetPoint(p->x() + r.x(), p->y() + r.y());
  }
}

/////////////////////////////////////////////////////////////////////////////
//
// View - event handlers
//
/////////////////////////////////////////////////////////////////////////////

bool View::OnMousePressed(const MouseEvent& e) {
  return false;
}

bool View::OnMouseDragged(const MouseEvent& e) {
  return false;
}

void View::OnMouseReleased(const MouseEvent& e, bool canceled) {
}

void View::OnMouseMoved(const MouseEvent& e) {
}

void View::OnMouseEntered(const MouseEvent& e) {
}

void View::OnMouseExited(const MouseEvent& e) {
}

void View::SetMouseHandler(View *new_mouse_handler) {
  // It is valid for new_mouse_handler to be NULL
  if (parent_) {
    parent_->SetMouseHandler(new_mouse_handler);
  }
}

void View::SetVisible(bool flag) {
  if (flag != is_visible_) {
    // If the tab is currently visible, schedule paint to
    // refresh parent
    if (IsVisible()) {
      SchedulePaint();
    }

    is_visible_ = flag;

    // This notifies all subviews recursively.
    PropagateVisibilityNotifications(this, flag);

    // If we are newly visible, schedule paint.
    if (IsVisible()) {
      SchedulePaint();
    }
  }
}

bool View::IsVisibleInRootView() const {
  View* parent = GetParent();
  if (IsVisible() && parent)
    return parent->IsVisibleInRootView();
  else
    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
// View - keyboard and focus
//
/////////////////////////////////////////////////////////////////////////////

void View::RequestFocus() {
  RootView* rv = GetRootView();
  if (rv && IsFocusable())
    rv->FocusView(this);
}

void View::WillGainFocus() {
}

void View::DidGainFocus() {
}

void View::WillLoseFocus() {
}

bool View::OnKeyPressed(const KeyEvent& e) {
  return false;
}

bool View::OnKeyReleased(const KeyEvent& e) {
  return false;
}

bool View::OnMouseWheel(const MouseWheelEvent& e) {
  return false;
}

void View::SetDragController(DragController* drag_controller) {
    drag_controller_ = drag_controller;
}

DragController* View::GetDragController() {
  return drag_controller_;
}

bool View::CanDrop(const OSExchangeData& data) {
  return false;
}

void View::OnDragEntered(const DropTargetEvent& event) {
}

int View::OnDragUpdated(const DropTargetEvent& event) {
  return DragDropTypes::DRAG_NONE;
}

void View::OnDragExited() {
}

int View::OnPerformDrop(const DropTargetEvent& event) {
  return DragDropTypes::DRAG_NONE;
}

// static
bool View::ExceededDragThreshold(int delta_x, int delta_y) {
  return (abs(delta_x) > GetHorizontalDragThreshold() ||
          abs(delta_y) > GetVerticalDragThreshold());
}

// Tooltips -----------------------------------------------------------------
bool View::GetTooltipText(int x, int y, std::wstring* tooltip) {
  return false;
}

bool View::GetTooltipTextOrigin(int x, int y, gfx::Point* loc) {
  return false;
}

void View::TooltipTextChanged() {
  Widget* widget = GetWidget();
  if (widget && widget->GetTooltipManager())
    widget->GetTooltipManager()->TooltipTextChanged(this);
}

void View::UpdateTooltip() {
  Widget* widget = GetWidget();
  if (widget && widget->GetTooltipManager())
    widget->GetTooltipManager()->UpdateTooltip();
}

void View::SetParentOwned(bool f) {
  is_parent_owned_ = f;
}

bool View::IsParentOwned() const {
  return is_parent_owned_;
}

std::string View::GetClassName() const {
  return kViewClassName;
}

View* View::GetAncestorWithClassName(const std::string& name) {
  for (View* view = this; view; view = view->GetParent()) {
    if (view->GetClassName() == name)
      return view;
  }
  return NULL;
}

gfx::Rect View::GetVisibleBounds() {
  if (!IsVisibleInRootView())
    return gfx::Rect();
  gfx::Rect vis_bounds(0, 0, width(), height());
  gfx::Rect ancestor_bounds;
  View* view = this;
  int root_x = 0;
  int root_y = 0;
  while (view != NULL && !vis_bounds.IsEmpty()) {
    root_x += view->GetX(APPLY_MIRRORING_TRANSFORMATION);
    root_y += view->y();
    vis_bounds.Offset(view->GetX(APPLY_MIRRORING_TRANSFORMATION), view->y());
    View* ancestor = view->GetParent();
    if (ancestor != NULL) {
      ancestor_bounds.SetRect(0, 0, ancestor->width(),
                              ancestor->height());
      vis_bounds = vis_bounds.Intersect(ancestor_bounds);
    } else if (!view->GetWidget()) {
      // If the view has no Widget, we're not visible. Return an empty rect.
      return gfx::Rect();
    }
    view = ancestor;
  }
  if (vis_bounds.IsEmpty())
    return vis_bounds;
  // Convert back to this views coordinate system.
  vis_bounds.Offset(-root_x, -root_y);
  return vis_bounds;
}

int View::GetPageScrollIncrement(ScrollView* scroll_view,
                                 bool is_horizontal, bool is_positive) {
  return 0;
}

int View::GetLineScrollIncrement(ScrollView* scroll_view,
                                 bool is_horizontal, bool is_positive) {
  return 0;
}

ThemeProvider* View::GetThemeProvider() {
  Widget* widget = GetWidget();
  return widget ? widget->GetThemeProvider() : NULL;
}

// static
void View::RegisterChildrenForVisibleBoundsNotification(
    RootView* root, View* view) {
  DCHECK(root && view);
  if (view->GetNotifyWhenVisibleBoundsInRootChanges())
    root->RegisterViewForVisibleBoundsNotification(view);
  for (int i = 0; i < view->GetChildViewCount(); ++i)
    RegisterChildrenForVisibleBoundsNotification(root, view->GetChildViewAt(i));
}

// static
void View::UnregisterChildrenForVisibleBoundsNotification(
    RootView* root, View* view) {
  DCHECK(root && view);
  if (view->GetNotifyWhenVisibleBoundsInRootChanges())
    root->UnregisterViewForVisibleBoundsNotification(view);
  for (int i = 0; i < view->GetChildViewCount(); ++i)
    UnregisterChildrenForVisibleBoundsNotification(root,
                                                   view->GetChildViewAt(i));
}

void View::AddDescendantToNotify(View* view) {
  DCHECK(view);
  if (!descendants_to_notify_.get())
    descendants_to_notify_.reset(new ViewList());
  descendants_to_notify_->push_back(view);
}

void View::RemoveDescendantToNotify(View* view) {
  DCHECK(view && descendants_to_notify_.get());
  ViewList::iterator i = find(descendants_to_notify_->begin(),
                              descendants_to_notify_->end(),
                              view);
  DCHECK(i != descendants_to_notify_->end());
  descendants_to_notify_->erase(i);
  if (descendants_to_notify_->empty())
    descendants_to_notify_.reset();
}

// DropInfo --------------------------------------------------------------------

void View::DragInfo::Reset() {
  possible_drag = false;
  start_x = start_y = 0;
}

void View::DragInfo::PossibleDrag(int x, int y) {
  possible_drag = true;
  start_x = x;
  start_y = y;
}

}  // namespace
