// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/tabs/tab_strip_2.h"

#include "app/gfx/canvas.h"
#include "app/slide_animation.h"
#include "app/win_util.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/common/chrome_switches.h"
#include "views/animator.h"
#include "views/screen.h"
#include "views/widget/widget.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

static const int kHorizontalMoveThreshold = 16;  // pixels

////////////////////////////////////////////////////////////////////////////////
// TabStrip2, public:

TabStrip2::TabStrip2(TabStrip2Model* model)
    : model_(model),
      last_move_screen_x_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(detach_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(drag_start_factory_(this)) {
}

TabStrip2::~TabStrip2() {
}

// static
bool TabStrip2::Enabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableTabtastic2);
}

void TabStrip2::AddTabAt(int index) {
  Tab2* tab = new Tab2(this);
  int insertion_index = GetInternalIndex(index);
  tabs_.insert(tabs_.begin() + insertion_index, tab);
  AddChildView(insertion_index, tab);
  LayoutImpl(LS_TAB_ADD);
}

void TabStrip2::RemoveTabAt(int index, Tab2Model* removing_model) {
  Tab2* tab = GetTabAt(GetInternalIndex(index));

  DCHECK(!tab->removing());
  tab->set_removing(true);

  DCHECK(removing_model);
  tab->SetRemovingModel(removing_model);

  LayoutImpl(LS_TAB_REMOVE);
}

void TabStrip2::SelectTabAt(int index) {
  LayoutImpl(LS_TAB_SELECT);
  SchedulePaint();
}

void TabStrip2::MoveTabAt(int index, int to_index) {
  int from_index = GetInternalIndex(index);
  Tab2* tab = GetTabAt(from_index);
  tabs_.erase(tabs_.begin() + from_index);
  tabs_.insert(tabs_.begin() + GetInternalIndex(to_index), tab);
  LayoutImpl(LS_TAB_DRAG_REORDER);
}

int TabStrip2::GetTabCount() const {
  return tabs_.size();
}

Tab2* TabStrip2::GetTabAt(int index) const {
  return tabs_.at(index);
}

int TabStrip2::GetTabIndex(Tab2* tab) const {
  std::vector<Tab2*>::const_iterator it = find(tabs_.begin(), tabs_.end(), tab);
  if (it != tabs_.end())
    return it - tabs_.begin();
  return -1;
}

int TabStrip2::GetInsertionIndexForPoint(const gfx::Point& point) const {
  int tab_count = GetTabCount();
  for (int i = 0; i < tab_count; ++i) {
    if (GetTabAt(i)->removing())
      continue;
    gfx::Rect tab_bounds = GetTabAt(i)->bounds();
    gfx::Rect tab_left_half = tab_bounds;
    tab_left_half.set_width(tab_left_half.width() / 2);
    if (point.x() >= tab_left_half.x() && point.x() <= tab_left_half.right())
      return i;
    gfx::Rect tab_right_half = tab_bounds;
    tab_right_half.set_x(tab_right_half.width() / 2);
    tab_right_half.set_width(tab_right_half.x());
    if (point.x() > tab_right_half.x() && point.x() <= tab_right_half.right())
    if (tab_right_half.Contains(point))
      return i + 1;
  }
  return tab_count;
}

gfx::Rect TabStrip2::GetDraggedTabScreenBounds(const gfx::Point& screen_point) {
  gfx::Point tab_screen_origin(screen_point);
  tab_screen_origin.Offset(mouse_tab_offset_.x(), mouse_tab_offset_.y());
  return gfx::Rect(tab_screen_origin, GetTabAt(0)->bounds().size());
}

void TabStrip2::SetDraggedTabBounds(int index, const gfx::Rect& tab_bounds) {
  // This function should only ever be called goats
  Tab2* dragged_tab = GetTabAt(index);
  dragged_tab->SetBounds(tab_bounds);
  SchedulePaint();
}

void TabStrip2::SendDraggedTabHome() {
  LayoutImpl(LS_TAB_DRAG_REORDER);
}

void TabStrip2::ResumeDraggingTab(int index, const gfx::Rect& tab_bounds) {
  MessageLoop::current()->PostTask(FROM_HERE,
      drag_start_factory_.NewRunnableMethod(&TabStrip2::StartDragTabImpl, index,
                                            tab_bounds));
}

// static
bool TabStrip2::IsDragRearrange(TabStrip2* tabstrip,
                                const gfx::Point& screen_point) {
  gfx::Point origin;
  View::ConvertPointToScreen(tabstrip, &origin);
  gfx::Rect tabstrip_bounds_in_screen_coords(origin, tabstrip->bounds().size());
  if (tabstrip_bounds_in_screen_coords.Contains(screen_point))
    return true;

  // The tab is only detached if the tab is moved outside the bounds of the
  // TabStrip to the left or right, or a certain distance above or below the
  // TabStrip defined by the vertical detach magnetism below. This is to
  // prevent accidental detaches when rearranging horizontally.
  static const int kVerticalDetachMagnetism = 45;

  bool rearrange = true;
  if (screen_point.x() < tabstrip_bounds_in_screen_coords.right() &&
      screen_point.x() >= tabstrip_bounds_in_screen_coords.x()) {
    int lower_threshold =
        tabstrip_bounds_in_screen_coords.bottom() + kVerticalDetachMagnetism;
    int upper_threshold =
        tabstrip_bounds_in_screen_coords.y() - kVerticalDetachMagnetism;
    return screen_point.y() >= upper_threshold &&
           screen_point.y() <= lower_threshold;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// TabStrip2, Tab2Model implementation:

string16 TabStrip2::GetTitle(Tab2* tab) const {
  return model_->GetTitle(GetTabIndex(tab));
}

bool TabStrip2::IsSelected(Tab2* tab) const {
  return model_->IsSelected(GetTabIndex(tab));
}

void TabStrip2::SelectTab(Tab2* tab) {
  model_->SelectTabAt(GetTabIndex(tab));
}

void TabStrip2::CaptureDragInfo(Tab2* tab,
                                const views::MouseEvent& drag_event) {
  mouse_tab_offset_ = drag_event.location();
}

bool TabStrip2::DragTab(Tab2* tab, const views::MouseEvent& drag_event) {
  if (!model_->CanDragTabs())
    return false;

  int tab_x = tab->x() + drag_event.location().x() - mouse_tab_offset_.x();
  if (tab_x < 0)
    tab_x = 0;
  if ((tab_x + tab->width()) > bounds().right())
    tab_x = bounds().right() - tab_x - tab->width();
  tab->SetBounds(tab_x, tab->y(), tab->width(), tab->height());
  SchedulePaint();

  int tab_index = GetTabIndex(tab);
  int dest_index = tab_index;

  Tab2* next_tab = NULL;
  Tab2* prev_tab = NULL;
  int next_tab_index = tab_index + 1;
  if (next_tab_index < GetTabCount())
    next_tab = GetTabAt(next_tab_index);
  int prev_tab_index = tab_index - 1;
  if (prev_tab_index >= 0)
    prev_tab = GetTabAt(prev_tab_index);

  if (next_tab) {
    int next_tab_middle_x = next_tab->x() + next_tab->bounds().width() / 2;
    if (!next_tab->IsAnimating() && tab->bounds().right() > next_tab_middle_x)
      ++dest_index;
  }
  if (prev_tab) {
    int prev_tab_middle_x = prev_tab->x() + prev_tab->bounds().width() / 2;
    if (!prev_tab->IsAnimating() && tab->bounds().x() < prev_tab_middle_x)
      --dest_index;
  }

  gfx::Point screen_point = views::Screen::GetCursorScreenPoint();
  if (IsDragRearrange(this, screen_point)) {
    if (abs(screen_point.x() - last_move_screen_x_) >
        kHorizontalMoveThreshold) {
      if (dest_index != tab_index) {
        last_move_screen_x_ = screen_point.x();
        model_->MoveTabAt(tab_index, dest_index);
      }
    }
  } else {
    // We're going to detach. We need to release mouse capture so that further
    // mouse events will be sent to the appropriate window (the detached window)
    // and so that we don't recursively create nested message loops (dragging
    // is done by windows in a nested message loop).
    ReleaseCapture();
    MessageLoop::current()->PostTask(FROM_HERE,
        detach_factory_.NewRunnableMethod(&TabStrip2::DragDetachTabImpl,
                                          tab, tab_index));
  }
  return true;
}

void TabStrip2::DragEnded(Tab2* tab) {
  LayoutImpl(LS_TAB_DRAG_NORMALIZE);
}

views::AnimatorDelegate* TabStrip2::AsAnimatorDelegate() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// TabStrip2, views::View overrides:

gfx::Size TabStrip2::GetPreferredSize() {
  return gfx::Size(0, 27);
}

void TabStrip2::Layout() {
  LayoutImpl(LS_OTHER);
}

void TabStrip2::Paint(gfx::Canvas* canvas) {
  canvas->FillRectInt(SK_ColorBLUE, 0, 0, width(), height());
}

void TabStrip2::PaintChildren(gfx::Canvas* canvas) {
  // Paint the tabs in reverse order, so they stack to the left.
  Tab2* selected_tab = NULL;
  for (int i = GetTabCount() - 1; i >= 0; --i) {
    Tab2* tab = GetTabAt(i);
    // We must ask the _Tab's_ model, not ourselves, because in some situations
    // the model will be different to this object, e.g. when a Tab is being
    // removed after its TabContents has been destroyed.
    if (!IsSelected(tab)) {
      tab->ProcessPaint(canvas);
    } else {
      selected_tab = tab;
    }
  }

  if (GetWindow()->GetNonClientView()->UseNativeFrame()) {
    // Make sure unselected tabs are somewhat transparent.
    SkPaint paint;
    paint.setColor(SkColorSetARGB(200, 255, 255, 255));
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->FillRectInt(
        0, 0, width(),
        height() - 2,  // Visible region that overlaps the toolbar.
        paint);
  }

  // Paint the selected tab last, so it overlaps all the others.
  if (selected_tab)
    selected_tab->ProcessPaint(canvas);
}

////////////////////////////////////////////////////////////////////////////////
// TabStrip2, views::AnimatorDelegate implementation:

views::View* TabStrip2::GetClampedView(views::View* host) {
  int tab_count = GetTabCount();
  for (int i = 0; i < tab_count; ++i) {
    Tab2* tab = GetTabAt(i);
    if (tab == host && i > 0)
      return GetTabAt(i - 1);
  }
  return NULL;
}

void TabStrip2::AnimationCompletedForHost(View* host) {
  Tab2* tab = static_cast<Tab2*>(host);
  if (tab->removing()) {
    tabs_.erase(find(tabs_.begin(), tabs_.end(), tab));
    RemoveChildView(tab);
    delete tab;
  }
}

////////////////////////////////////////////////////////////////////////////////
// TabStrip2, private:

int TabStrip2::GetAnimateFlagsForLayoutSource(LayoutSource source) const {
  switch (source) {
    case LS_TAB_ADD:
    case LS_TAB_SELECT:
    case LS_TAB_REMOVE:
      return views::Animator::ANIMATE_WIDTH | views::Animator::ANIMATE_X |
             views::Animator::ANIMATE_CLAMP;
    case LS_TAB_DRAG_REORDER:
    case LS_TAB_DRAG_NORMALIZE:
      return views::Animator::ANIMATE_X;
  }
  DCHECK(source == LS_OTHER);
  return views::Animator::ANIMATE_NONE;
}

void TabStrip2::LayoutImpl(LayoutSource source) {
  int child_count = GetTabCount();
  if (child_count > 0) {
    int child_width = width() / child_count;
    child_width = std::min(child_width, Tab2::GetStandardSize().width());

    int animate_flags = GetAnimateFlagsForLayoutSource(source);
    int removing_count = 0;
    for (int i = 0; i < child_count; ++i) {
      Tab2* tab = GetTabAt(i);
      if (tab->removing())
        ++removing_count;
      if (!tab->dragging()) {
        int tab_x = i * child_width - removing_count * child_width;
        int tab_width = tab->removing() ? 0 : child_width;
        gfx::Rect new_bounds(tab_x, 0, tab_width, height());

        // Tabs that are currently being removed can have their bounds reset
        // when another tab in the tabstrip is removed before their remove
        // animation completes. Before they are given a new target bounds to
        // animate to, we need to unset the removing property so that they are
        // not pre-emptively deleted.
        bool removing = tab->removing();
        tab->set_removing(false);
        tab->GetAnimator()->AnimateToBounds(new_bounds, animate_flags);
        // Now restore the removing property.
        tab->set_removing(removing);
      }
    }
  }
}

void TabStrip2::DragDetachTabImpl(Tab2* tab, int index) {
  gfx::Rect tab_bounds = tab->bounds();

  // Determine the origin of the new window. We start with the current mouse
  // position:
  gfx::Point new_window_origin(views::Screen::GetCursorScreenPoint());
  // Subtract the offset of the mouse pointer from the tab top left when the
  // drag action began.
  new_window_origin.Offset(-mouse_tab_offset_.x(), -mouse_tab_offset_.y());
  // Subtract the offset of the tab's current position from the window.
  gfx::Point tab_window_origin;
  View::ConvertPointToWidget(tab, &tab_window_origin);
  new_window_origin.Offset(-tab_window_origin.x(), -tab_window_origin.y());

  // The new window is created with the same size as the source window but at
  // the origin calculated above.
  gfx::Rect new_window_bounds = GetWindow()->GetBounds();
  new_window_bounds.set_origin(new_window_origin);

  model_->DetachTabAt(index, new_window_bounds, tab_bounds);
}

void TabStrip2::StartDragTabImpl(int index, const gfx::Rect& tab_bounds) {
  SetDraggedTabBounds(index, tab_bounds);
  gfx::Rect tab_local_bounds(tab_bounds);
  tab_local_bounds.set_origin(gfx::Point());
  GetWidget()->GenerateMousePressedForView(GetTabAt(index),
                                           tab_local_bounds.CenterPoint());
}

int TabStrip2::GetInternalIndex(int public_index) const {
  std::vector<Tab2*>::const_iterator it;
  int internal_index = public_index;
  int valid_tab_count = 0;
  for (it = tabs_.begin(); it != tabs_.end(); ++it) {
    if (public_index >= valid_tab_count)
      break;
    if ((*it)->removing())
      ++internal_index;
    else
      ++valid_tab_count;
  }
  return internal_index;
}
