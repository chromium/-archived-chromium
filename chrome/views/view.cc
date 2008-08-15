// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/views/view.h"

#include <algorithm>

#ifndef NDEBUG
#include <iostream>
#endif

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/views/accessibility/accessible_wrapper.h"
#include "chrome/views/background.h"
#include "chrome/views/border.h"
#include "chrome/views/layout_manager.h"
#include "chrome/views/root_view.h"
#include "chrome/views/tooltip_manager.h"
#include "chrome/views/view_container.h"
#include "SkShader.h"

namespace ChromeViews {

// static
char View::kViewClassName[] = "chrome/views/View";

////////////////////////////////////////////////////////////////////////////////
//
// A task used to automatically restore focus on the last focused floating view
//
////////////////////////////////////////////////////////////////////////////////

class RestoreFocusTask : public Task {
 public:
  explicit RestoreFocusTask(View* target) : view_(target) {
  }

  ~RestoreFocusTask() {}

  void Cancel() {
    view_ = NULL;
  }

  void Run() {
    if (view_)
      view_->RestoreFloatingViewFocus();
  }
 private:
  // The target view.
  View* view_;

  DISALLOW_EVIL_CONSTRUCTORS(RestoreFocusTask);
};

/////////////////////////////////////////////////////////////////////////////
//
// View - constructors, destructors, initialization
//
/////////////////////////////////////////////////////////////////////////////

View::View()
    : id_(0),
      group_(-1),
      bounds_(0,0,0,0),
      parent_(NULL),
      enabled_(true),
      is_visible_(true),
      focusable_(false),
      background_(NULL),
      accessibility_(NULL),
      border_(NULL),
      is_parent_owned_(true),
      notify_when_visible_bounds_in_root_changes_(false),
      registered_for_visible_bounds_notification_(false),
      next_focusable_view_(NULL),
      previous_focusable_view_(NULL),
      should_restore_focus_(false),
      restore_focus_view_task_(NULL),
      context_menu_controller_(NULL),
      drag_controller_(NULL),
      ui_mirroring_is_enabled_for_rtl_languages_(true),
      flip_canvas_on_paint_for_rtl_ui_(false) {
}

View::~View() {
  if (restore_focus_view_task_)
    restore_focus_view_task_->Cancel();

  int c = static_cast<int>(child_views_.size());
  while (--c >= 0) {
    if (child_views_[c]->IsParentOwned())
      delete child_views_[c];
    else
      child_views_[c]->SetParent(NULL);
  }
  if (background_)
    delete background_;
  if (border_)
    delete border_;
}

/////////////////////////////////////////////////////////////////////////////
//
// View - sizing
//
/////////////////////////////////////////////////////////////////////////////

void View::GetBounds(CRect* out, PositionMirroringSettings settings) const {
  *out = bounds_;

  // If the parent uses an RTL UI layout and if we are asked to transform the
  // bounds to their mirrored position if necessary, then we should shift the
  // rectangle appropriately.
  if (settings == APPLY_MIRRORING_TRANSFORMATION) {
    out->MoveToX(MirroredX());
  }
}

// GetY(), GetWidth() and GetHeight() are agnostic to the RTL UI layout of the
// parent view. GetX(), on the other hand, is not.
int View::GetX(PositionMirroringSettings settings) const {
  if (settings == IGNORE_MIRRORING_TRANSFORMATION) {
    return bounds_.left;
  }
  return MirroredX();
}

void View::SetBounds(const CRect& bounds) {
  if (bounds.left == bounds_.left &&
      bounds.top == bounds_.top &&
      bounds.Width() == bounds_.Width() &&
      bounds.Height() == bounds_.Height()) {
    return;
  }

  CRect prev = bounds_;
  bounds_ = bounds;
  if (bounds_.right < bounds_.left)
    bounds_.right = bounds_.left;

  if (bounds_.bottom < bounds_.top)
    bounds_.bottom = bounds_.top;

  DidChangeBounds(prev, bounds_);

  RootView* root = GetRootView();
  if (root) {
    bool size_changed = (prev.Width() != bounds_.Width() ||
                         prev.Height() != bounds_.Height());
    bool position_changed = (prev.left != bounds_.left ||
                             prev.top != bounds_.top);
    if (size_changed || position_changed)
      root->ViewBoundsChanged(this, size_changed, position_changed);
  }
}

void View::SetBounds(int x, int y, int width, int height) {
  CRect tmp(x, y, x + width, y + height);
  SetBounds(tmp);
}

void View::GetLocalBounds(CRect* out, bool include_border) const {
  if (include_border || border_ == NULL) {
    out->left = 0;
    out->top = 0;
    out->right = GetWidth();
    out->bottom = GetHeight();
  } else {
    gfx::Insets insets;
    border_->GetInsets(&insets);
    out->left = insets.left();
    out->top = insets.top();
    out->right = GetWidth() - insets.left();
    out->bottom = GetHeight() - insets.top();
  }
}

void View::GetSize(CSize* sz) const {
  sz->cx = GetWidth();
  sz->cy = GetHeight();
}

void View::GetPosition(CPoint* p) const {
  p->x = GetX(APPLY_MIRRORING_TRANSFORMATION);
  p->y = GetY();
}

void View::GetPreferredSize(CSize* out) {
  if (layout_manager_.get()) {
    layout_manager_->GetPreferredSize(this, out);
  } else {
    out->cx = out->cy = 0;
  }
}

void View::SizeToPreferredSize() {
  CSize size;
  GetPreferredSize(&size);
  if ((size.cx != GetWidth()) || (size.cy != GetHeight()))
    SetBounds(GetX(), GetY(), size.cx, size.cy);
}

void View::GetMinimumSize(CSize* out) {
  GetPreferredSize(out);
}

int View::GetHeightForWidth(int w) {
  if (layout_manager_.get())
    return layout_manager_->GetPreferredHeightForWidth(this, w);

  CSize size;
  GetPreferredSize(&size);
  return size.cy;
}

void View::DidChangeBounds(const CRect& previous, const CRect& current) {
}

void View::ScrollRectToVisible(int x, int y, int width, int height) {
  View* parent = GetParent();

  // We must take RTL UI mirroring into account when adjusting the position of
  // the region.
  if (parent)
    parent->ScrollRectToVisible(
        GetX(APPLY_MIRRORING_TRANSFORMATION) + x, GetY() + y, width, height);
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
  View* parent = GetParent();
  if (parent && parent->UILayoutIsRightToLeft()) {
    return parent->GetWidth() - bounds_.left - GetWidth();
  }
  return bounds_.left;
}

int View::MirroredLeftPointForRect(const gfx::Rect& bounds) const {
  if (!UILayoutIsRightToLeft()) {
    return bounds.x();
  }
  return GetWidth() - bounds.x() - bounds.width();
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
  ViewContainer* container = GetViewContainer();
  if (!container)
    return NULL;

  HWND hwnd = container->GetHWND();
  if (!hwnd)
    return NULL;

  return ChromeViews::FocusManager::GetFocusManager(hwnd);
}

bool View::HasFocus() {
  RootView* root_view = GetRootView();
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

void View::SchedulePaint(const CRect& r, bool urgent) {
  if (!IsVisible()) {
    return;
  }

  if (parent_) {
    // Translate the requested paint rect to the parent's coordinate system
    // then pass this notification up to the parent.
    CRect paint_rect(r);
    CPoint p;
    GetPosition(&p);
    paint_rect.OffsetRect(p);
    parent_->SchedulePaint(paint_rect, urgent);
  }
}

void View::SchedulePaint() {
  CRect lb;
  GetLocalBounds(&lb, true);
  SchedulePaint(lb, false);
}

void View::SchedulePaint(int x, int y, int w, int h) {
  CRect r(x, y, x + w, y + h);
  SchedulePaint(&r, false);
}

void View::Paint(ChromeCanvas* canvas) {
  PaintBackground(canvas);
  PaintFocusBorder(canvas);
  PaintBorder(canvas);
}

void View::PaintBackground(ChromeCanvas* canvas) {
  if (background_)
    background_->Paint(canvas, this);
}

void View::PaintBorder(ChromeCanvas* canvas) {
  if (border_)
    border_->Paint(*this, canvas);
}

void View::PaintFocusBorder(ChromeCanvas* canvas) {
  if (HasFocus() && IsFocusable())
    canvas->DrawFocusRect(0, 0, GetWidth(), GetHeight());
}

void View::PaintChildren(ChromeCanvas* canvas) {
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

void View::ProcessPaint(ChromeCanvas* canvas) {
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
  if (canvas->ClipRectInt(MirroredX(), bounds_.top, bounds_.Width(),
                          bounds_.Height())) {
    // Non-empty clip, translate the graphics such that 0,0 corresponds to
    // where this view is located (related to its parent).
    canvas->TranslateInt(MirroredX(), bounds_.top);

    // Save the state again, so that any changes don't effect PaintChildren.
    canvas->save();

    // If the View we are about to paint requested the canvas to be flipped, we
    // should change the transform appropriately.
    bool flip_canvas = FlipCanvasOnPaintForRTLUI();
    if (flip_canvas) {
      canvas->TranslateInt(GetWidth(), 0);
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

void View::PaintFloatingView(ChromeCanvas* canvas, View* view,
                             int x, int y, int w, int h) {
  if (should_restore_focus_ && ShouldRestoreFloatingViewFocus()) {
    // We are painting again a floating view, this is a good time to restore the
    // focus to the last focused floating view if any.
    should_restore_focus_ = false;
    restore_focus_view_task_ = new RestoreFocusTask(this);
    MessageLoop::current()->PostTask(FROM_HERE, restore_focus_view_task_);
  }
  View* saved_parent = view->GetParent();
  view->SetParent(this);
  view->SetBounds(x, y, w, h);
  view->Layout();
  view->ProcessPaint(canvas);
  view->SetParent(saved_parent);
}

void View::SetBackground(Background* b) {
  if (background_ != b)
    delete background_;
  background_ = b;
}

const Background* View::GetBackground() const {
  return background_;
}

void View::SetBorder(Border* b) {
  if (border_ != b)
    delete border_;
  border_ = b;
}

const Border* View::GetBorder() const {
  return border_;
}

gfx::Insets View::GetInsets() const {
  const Border* border = GetBorder();
  gfx::Insets insets;
  if (border)
    border->GetInsets(&insets);
  return insets;
}

void View::SetContextMenuController(ContextMenuController* menu_controller) {
  context_menu_controller_ = menu_controller;
}

/////////////////////////////////////////////////////////////////////////////
//
// View - tree
//
/////////////////////////////////////////////////////////////////////////////

bool View::ProcessMousePressed(const MouseEvent& e, DragInfo* drag_info) {
  const bool enabled = enabled_;
  int drag_operations;
  if (enabled && e.IsOnlyLeftMouseButton() && HitTest(WTL::CPoint(e.GetX(), e.GetY())))
    drag_operations = GetDragOperations(e.GetX(), e.GetY());
  else
    drag_operations = 0;
  ContextMenuController* context_menu_controller = context_menu_controller_;

  const bool result = OnMousePressed(e);
  // WARNING: we may have been deleted, don't use any View variables;

  if (!enabled)
    return result;

  if (drag_operations != DragDropTypes::DRAG_NONE) {
    drag_info->PossibleDrag(e.GetX(), e.GetY());
    return true;
  }
  return !!context_menu_controller || result;
}

bool View::ProcessMouseDragged(const MouseEvent& e, DragInfo* drag_info) {
  // Copy the field, that way if we're deleted after drag and drop no harm is
  // done.
  ContextMenuController* context_menu_controller = context_menu_controller_;
  const bool possible_drag = drag_info->possible_drag;
  if (possible_drag && ExceededDragThreshold(drag_info->start_x - e.GetX(),
                                             drag_info->start_y - e.GetY())) {
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
    CPoint location(e.GetX(), e.GetY());
    ConvertPointToScreen(this, &location);
    ContextMenuController* context_menu_controller = context_menu_controller_;
    OnMouseReleased(e, canceled);
    context_menu_controller_->ShowContextMenu(this, location.x, location.y,
                                              true);
  } else {
    OnMouseReleased(e, canceled);
  }
  // WARNING: we may have been deleted.
}

void View::DoDrag(const MouseEvent& e, int press_x, int press_y) {
  scoped_refptr<OSExchangeData> data = new OSExchangeData;
  WriteDragData(press_x, press_y, data.get());

  // Message the RootView to do the drag and drop. That way if we're removed
  // the RootView can detect it and avoid callins us back.
  RootView* root_view = GetRootView();
  root_view->StartDragForViewFromMouseEvent(
      this, data, GetDragOperations(press_x, press_y));
}

void View::AddChildView(View* v) {
  AddChildView(static_cast<int>(child_views_.size()), v, false);
}

void View::AddChildView(int index, View* v) {
  AddChildView(index, v, false);
}

void View::AddChildView(int index, View* v, bool floating_view) {
  // Remove the view from its current parent if any.
  if (v->GetParent())
    v->GetParent()->RemoveChildView(v);

  if (!floating_view) {
    // Sets the prev/next focus views.
    InitFocusSiblings(v, index);
  }

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
    if (update_focus_cycle && !a_view->IsFloatingView()) {
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

#ifndef NDEBUG
bool View::IsProcessingPaint() const {
  return GetParent() && GetParent()->IsProcessingPaint();
}
#endif

void View::ViewHierarchyChanged(bool is_add, View *parent, View *child) {
}

void View::ViewHierarchyChangedImpl(bool register_accelerators,
                                    bool is_add, View *parent, View *child) {
  if (register_accelerators) {
    if (is_add) {
      // If you get this registration, you are part of a subtree that has been
      // added to the view hierarchy.
      RegisterAccelerators();
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

View* View::GetViewForPoint(const CPoint& point) {
  return GetViewForPoint(point, true);
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

View* View::GetViewForPoint(const CPoint& point, bool can_create_floating) {
  // Walk the child Views recursively looking for the View that most
  // tightly encloses the specified point.
  for (int i = GetChildViewCount() - 1 ; i >= 0 ; --i) {
    View* child = GetChildViewAt(i);
    if (!child->IsVisible()) {
      continue;
    }
    CRect bounds;
    child->GetBounds(&bounds, APPLY_MIRRORING_TRANSFORMATION);
    if (bounds.PtInRect(point)) {
      CPoint cl(point);
      cl.Offset(-bounds.left, -bounds.top);
      return child->GetViewForPoint(cl, true);
    }
  }

  // We haven't found a view for the point. Try to create floating views
  // and try again if one was created.
  // can_create_floating makes sure we don't try forever even if
  // GetFloatingViewIDForPoint lies or if RetrieveFloatingViewForID creates a
  // view which doesn't contain the provided point
  int id;
  if (can_create_floating && GetFloatingViewIDForPoint(point.x, point.y, &id)) {
    RetrieveFloatingViewForID(id);  // This creates the floating view.
    return GetViewForPoint(point, false);
  }
  return this;
}

ViewContainer* View::GetViewContainer() const {
  // The root view holds a reference to this view hierarchy's container.
  return parent_ ? parent_->GetViewContainer() : NULL;
}

// Get the containing RootView
RootView* View::GetRootView() {
  ViewContainer* vc = GetViewContainer();
  if (vc) {
    return vc->GetRootView();
  } else {
    return NULL;
  }
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
  buf << bounds_.left << L"," << bounds_.top << L",";
  buf << bounds_.right << L"," << bounds_.bottom;
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
  accelerators_->push_back(accelerator);
  RegisterAccelerators();
}

void View::ResetAccelerators() {
  if (accelerators_.get()) {
    UnregisterAccelerators();
    accelerators_->clear();
    accelerators_.reset();
  }
}

void View::RegisterAccelerators() {
  if (!accelerators_.get())
    return;

  RootView* root_view = GetRootView();
  if (!root_view) {
    // We are not yet part of a view hierarchy, we'll register ourselves once
    // added to one.
    return;
  }
  FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    // Some crash reports seem to show that we may get cases where we have no
    // focus manager (see bug #1291225).  This should never be the case, just
    // making sure we don't crash.
    NOTREACHED();
    return;
  }
  for (std::vector<Accelerator>::const_iterator iter = accelerators_->begin();
       iter != accelerators_->end(); ++iter) {
    focus_manager->RegisterAccelerator(*iter, this);
  }
}

void View::UnregisterAccelerators() {
  if (!accelerators_.get())
    return;

  RootView* root_view = GetRootView();
  if (root_view) {
    FocusManager* focus_manager = GetFocusManager();
    if (focus_manager) {
      // We may not have a FocusManager if the window containing us is being
      // closed, in which case the FocusManager is being deleted so there is
      // nothing to unregister.
      focus_manager->UnregisterAccelerators(this);
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
//
// View - accessibility
//
/////////////////////////////////////////////////////////////////////////////

AccessibleWrapper* View::GetAccessibleWrapper() {
  if (accessibility_.get() == NULL) {
    accessibility_.reset(new AccessibleWrapper(this));
  }
  return accessibility_.get();
}

/////////////////////////////////////////////////////////////////////////////
//
// View - floating views
//
/////////////////////////////////////////////////////////////////////////////

bool View::IsFloatingView() {
  if (!parent_)
    return false;

  return parent_->floating_views_ids_.find(this) !=
      parent_->floating_views_ids_.end();
}

// default implementation does nothing
bool View::GetFloatingViewIDForPoint(int x, int y, int* id) {
  return false;
}

int View::GetFloatingViewCount() const {
  return static_cast<int>(floating_views_.size());
}

View* View::RetrieveFloatingViewParent() {
  View* v = this;
  while (v) {
    if (v->IsFloatingView())
      return v;
    v = v->GetParent();
  }
  return NULL;
}

bool View::EnumerateFloatingViews(FloatingViewPosition position,
                                  int starting_id, int* id) {
    return false;
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

View* View::ValidateFloatingViewForID(int id) {
  return NULL;
}

bool View::ShouldRestoreFloatingViewFocus() {
  return true;
}

void View::AttachFloatingView(View* v, int id) {
  floating_views_.push_back(v);
  floating_views_ids_[v] = id;
  AddChildView(static_cast<int>(child_views_.size()), v, true);
}

bool View::HasFloatingViewForPoint(int x, int y) {
  int i, c;
  View* v;
  gfx::Rect r;

  for (i = 0, c = static_cast<int>(floating_views_.size()); i < c; ++i) {
    v = floating_views_[i];
    r.SetRect(v->GetX(APPLY_MIRRORING_TRANSFORMATION), v->GetY(),
              v->GetWidth(), v->GetHeight());
    if (r.Contains(x, y))
      return true;
  }
  return false;
}

void View::DetachAllFloatingViews() {
  RootView* root_view = GetRootView();
  View* focused_view = NULL;
  FocusManager* focus_manager = NULL;
  if (root_view) {
    // We may be called when we are not attached to a root view in which case
    // there is nothing to do for focus.
    focus_manager = GetFocusManager();
    if (focus_manager) {
      // We may not have a focus manager (if we are detached from a top window).
      focused_view = focus_manager->GetFocusedView();
    }
  }

  int c = static_cast<int>(floating_views_.size());
  while (--c >= 0) {
    // If the focused view is a floating view or a floating view's children,
    // use the focus manager to store it.
    int tmp_id;
    if (focused_view &&
        ((focused_view == floating_views_[c]) ||
          floating_views_[c]->IsParentOf(focused_view))) {
      // We call EnumerateFloatingView to make sure the floating view is still
      // valid: the model may have changed and could not know anything about
      // that floating view anymore.
      if (EnumerateFloatingViews(CURRENT,
                                 floating_views_[c]->GetFloatingViewID(),
                                 &tmp_id)) {
        focus_manager->StoreFocusedView();
        should_restore_focus_ = true;
      }
      focused_view = NULL;
    }

    RemoveChildView(floating_views_[c]);
    delete floating_views_[c];
  }
  floating_views_.clear();
  floating_views_ids_.clear();
}

int View::GetFloatingViewID() {
  DCHECK(IsFloatingView());
  std::map<View*, int>::iterator iter = parent_->floating_views_ids_.find(this);
  DCHECK(iter != parent_->floating_views_ids_.end());
  return iter->second;
}

View* View::RetrieveFloatingViewForID(int id) {
  for (ViewList::const_iterator iter = floating_views_.begin();
       iter != floating_views_.end(); ++iter) {
    if ((*iter)->GetFloatingViewID() == id)
      return *iter;
  }
  return ValidateFloatingViewForID(id);
}

void View::RestoreFloatingViewFocus() {
  // Clear the reference to the task as if we have been triggered by it, it will
  // soon be invalid.
  restore_focus_view_task_ = NULL;
  should_restore_focus_ = false;

  GetFocusManager()->RestoreFocusedView();
}

// static
bool View::EnumerateFloatingViewsForInterval(int low_bound, int high_bound,
                                             bool ascending_order,
                                             FloatingViewPosition position,
                                             int starting_id,
                                             int* id) {
  DCHECK(low_bound <= high_bound);
  if (low_bound >= high_bound)
    return false;

  switch (position) {
    case CURRENT:
      if ((starting_id >= low_bound) && (starting_id < high_bound)) {
        *id = starting_id;
        return true;
      }
      return false;
    case FIRST:
      *id = ascending_order ? low_bound : high_bound - 1;
      return true;
    case LAST:
      *id = ascending_order ? high_bound - 1 : low_bound;
      return true;
    case NEXT:
    case PREVIOUS:
      if (((position == NEXT) && ascending_order) ||
          ((position == PREVIOUS) && !ascending_order)) {
        starting_id++;
        if (starting_id < high_bound) {
          *id = starting_id;
          return true;
        }
        return false;
      }
      DCHECK(((position == NEXT) && !ascending_order) ||
             ((position == PREVIOUS) && ascending_order));
      starting_id--;
      if (starting_id >= low_bound) {
        *id = starting_id;
        return true;
      }
      return false;
    default:
      NOTREACHED();
  }
  return false;
}

// static
void View::ConvertPointToView(View* src,
                              View* dst,
                              gfx::Point* point) {
  ConvertPointToView(src, dst, point, true);
}

// static
void View::ConvertPointToView(View* src,
                              View* dst,
                              CPoint* point) {
  gfx::Point tmp_point(point->x, point->y);
  ConvertPointToView(src, dst, &tmp_point, true);
  point->x = tmp_point.x();
  point->y = tmp_point.y();
}

// static
void View::ConvertPointToView(View* src,
                              View* dst,
                              gfx::Point* point,
                              bool try_other_direction) {
  // src can be NULL
  DCHECK(dst);
  DCHECK(point);

  View* v;
  gfx::Point offset;

  for (v = dst; v && v != src; v = v->GetParent()) {
    offset.SetPoint(offset.x() + v->GetX(APPLY_MIRRORING_TRANSFORMATION),
                    offset.y() + v->GetY());
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
      ViewContainer* vc = dst->GetViewContainer();
      if (vc) {
        CRect b;
        vc->GetBounds(&b, false);
        point->SetPoint(point->x() - b.left, point->y() - b.top);
      }
    }
  }
}

// static
void View::ConvertPointToViewContainer(View* src, CPoint* p) {
  DCHECK(src);
  DCHECK(p);
  View *v;
  CPoint offset(0, 0);

  for (v = src; v; v = v->GetParent()) {
    offset.x += v->GetX(APPLY_MIRRORING_TRANSFORMATION);
    offset.y += v->GetY();
  }
  p->x += offset.x;
  p->y += offset.y;
}

// static
void View::ConvertPointFromViewContainer(View *source, CPoint *p) {
  CPoint t(0, 0);
  ConvertPointToViewContainer(source, &t);
  p->x -= t.x;
  p->y -= t.y;
}

// static
void View::ConvertPointToScreen(View* src, CPoint* p) {
  DCHECK(src);
  DCHECK(p);

  // If the view is not connected to a tree, do nothing
  if (src->GetViewContainer() == NULL) {
    return;
  }

  ConvertPointToViewContainer(src, p);
  ViewContainer* vc = src->GetViewContainer();
  if (vc) {
    CRect r;
    vc->GetBounds(&r, false);
    p->x += r.left;
    p->y += r.top;
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

bool View::HitTest(const CPoint &l) const {
  if (l.x >= 0 && l.x < static_cast<int>(GetWidth()) &&
      l.y >= 0 && l.y < static_cast<int>(GetHeight())) {
    return true;
  } else {
    return false;
  }
}

HCURSOR View::GetCursorForPoint(Event::EventType event_type, int x, int y) {
  return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
// View - keyboard and focus
//
/////////////////////////////////////////////////////////////////////////////

void View::RequestFocus() {
  RootView* rv = GetRootView();
  if (rv) {
    rv->FocusView(this);
  }
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

static int GetHorizontalDragThreshold() {
  static int threshold = -1;
  if (threshold == -1)
    threshold = GetSystemMetrics(SM_CXDRAG) / 2;
  return threshold;
}

static int GetVerticalDragThreshold() {
  static int threshold = -1;
  if (threshold == -1)
    threshold = GetSystemMetrics(SM_CYDRAG) / 2;
  return threshold;
}

// static
bool View::ExceededDragThreshold(int delta_x, int delta_y) {
  return (abs(delta_x) > GetHorizontalDragThreshold() ||
          abs(delta_y) > GetVerticalDragThreshold());
}

void View::Focus() {
  // Set the native focus to the root view window so it receives the keyboard
  // messages.
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->FocusHWND(GetRootView()->GetViewContainer()->GetHWND());
}

bool View::CanProcessTabKeyEvents() {
  return false;
}

// Tooltips -----------------------------------------------------------------
bool View::GetTooltipText(int x, int y, std::wstring* tooltip) {
  return false;
}

bool View::GetTooltipTextOrigin(int x, int y, CPoint* loc) {
  return false;
}

void View::TooltipTextChanged() {
  ViewContainer* view_container = GetViewContainer();
  if (view_container != NULL && view_container->GetTooltipManager())
    view_container->GetTooltipManager()->TooltipTextChanged(this);
}

void View::UpdateTooltip() {
  ViewContainer* view_container = GetViewContainer();
  if (view_container != NULL && view_container->GetTooltipManager())
    view_container->GetTooltipManager()->UpdateTooltip();
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

gfx::Rect View::GetVisibleBounds() {
  gfx::Rect vis_bounds(0, 0, GetWidth(), GetHeight());
  gfx::Rect ancestor_bounds;
  View* view = this;
  int root_x = 0;
  int root_y = 0;
  bool has_view_container = false;
  while (view != NULL && !vis_bounds.IsEmpty()) {
    root_x += view->GetX(APPLY_MIRRORING_TRANSFORMATION);
    root_y += view->GetY();
    vis_bounds.Offset(view->GetX(APPLY_MIRRORING_TRANSFORMATION), view->GetY());
    View* ancestor = view->GetParent();
    if (ancestor != NULL) {
      ancestor_bounds.SetRect(0, 0, ancestor->GetWidth(),
                              ancestor->GetHeight());
      vis_bounds = vis_bounds.Intersect(ancestor_bounds);
    } else if (!view->GetViewContainer()) {
      // If the view has no ViewContainer, we're not visible. Return an empty
      // rect.
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

// static
bool View::GetViewPath(View* start, View* end, std::vector<int>* path) {
  while (end && (end != start)) {
    View* parent = end->GetParent();
    if (!parent)
      return false;
    path->insert(path->begin(), parent->GetChildIndex(end));
    end = parent;
  }
  return end == start;
}

// static
View* View::GetViewForPath(View* start, const std::vector<int>& path) {
  View* v = start;
  for (std::vector<int>::const_iterator iter = path.begin();
       iter != path.end(); ++iter) {
    int index = *iter;
    if (index >= v->GetChildViewCount())
      return NULL;
    v = v->GetChildViewAt(index);
  }
  return v;
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
