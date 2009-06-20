// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/find_bar_win.h"

#include "app/slide_animation.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/find_bar_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "views/focus/external_focus_tracker.h"
#include "views/focus/view_storage.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/widget/root_view.h"

#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#else
#include "views/widget/widget_gtk.h"
#endif

// The minimum space between the FindInPage window and the search result.
static const int kMinFindWndDistanceFromSelection = 5;

// static
bool FindBarWin::disable_animations_during_testing_ = false;

// Host is the actual widget containing FindBarView.
#if defined(OS_WIN)
class FindBarWin::Host : public views::WidgetWin {
 public:
  explicit Host(FindBarWin* find_bar) : find_bar_(find_bar) {
    // Don't let WidgetWin manage our lifetime. We want our lifetime to
    // coincide with TabContents.
    set_delete_on_destroy(false);
    set_window_style(WS_CHILD | WS_CLIPCHILDREN);
    set_window_ex_style(WS_EX_TOPMOST);
  }

  void OnFinalMessage(HWND window) {
    find_bar_->OnFinalMessage();
  }

 private:
  FindBarWin* find_bar_;

  DISALLOW_COPY_AND_ASSIGN(Host);
};
#else
class FindBarWin::Host : public views::WidgetGtk {
 public:
  explicit Host(FindBarWin* find_bar)
    : WidgetGtk(TYPE_CHILD),
      find_bar_(find_bar) {
    // Don't let WidgetWin manage our lifetime. We want our lifetime to
    // coincide with TabContents.
    set_delete_on_destroy(false);
  }

  void OnDestroy(GtkWidget* widget) {
    find_bar_->OnFinalMessage();
  }

 private:
  FindBarWin* find_bar_;

  DISALLOW_COPY_AND_ASSIGN(Host);
};
#endif

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
FindBar* CreateFindBar(BrowserView* browser_view) {
  return new FindBarWin(browser_view);
}

}  // namespace browser

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, public:

FindBarWin::FindBarWin(BrowserView* browser_view)
    : browser_view_(browser_view),
      find_dialog_animation_offset_(0),
      esc_accel_target_registered_(false),
      find_bar_controller_(NULL) {
  view_ = new FindBarView(this);

  // Initialize the host.
  host_.reset(new Host(this));
  host_->Init(browser_view->GetWidget()->GetNativeView(), gfx::Rect());
  host_->SetContentsView(view_);

  // Start listening to focus changes, so we can register and unregister our
  // own handler for Escape.
  focus_manager_ =
      views::FocusManager::GetFocusManagerForNativeView(host_->GetNativeView());
  focus_manager_->AddFocusChangeListener(this);

  // Stores the currently focused view, and tracks focus changes so that we can
  // restore focus when the find box is closed.
  focus_tracker_.reset(new views::ExternalFocusTracker(view_, focus_manager_));

  // Start the process of animating the opening of the window.
  animation_.reset(new SlideAnimation(this));
}

FindBarWin::~FindBarWin() {
}

// TODO(brettw) this should not be so complicated. The view should really be in
// charge of these regions. CustomFrameWindow will do this for us. It will also
// let us set a path for the window region which will avoid some logic here.
void FindBarWin::UpdateWindowEdges(const gfx::Rect& new_pos) {
#if defined(OS_WIN)
  // |w| is used to make it easier to create the part of the polygon that curves
  // the right side of the Find window. It essentially keeps track of the
  // x-pixel position of the right-most background image inside the view.
  // TODO(finnur): Let the view tell us how to draw the curves or convert
  // this to a CustomFrameWindow.
  int w = new_pos.width() - 6;  // -6 positions us at the left edge of the
                                // rightmost background image of the view.

  // This polygon array represents the outline of the background image for the
  // dialog. Basically, it encompasses only the visible pixels of the
  // concatenated find_dlg_LMR_bg images (where LMR = [left | middle | right]).
  static const POINT polygon[] = {
      {0, 0}, {0, 1}, {2, 3}, {2, 29}, {4, 31},
        {4, 32}, {w+0, 32},
      {w+0, 31}, {w+1, 31}, {w+3, 29}, {w+3, 3}, {w+6, 0}
  };

  // Find the largest x and y value in the polygon.
  int max_x = 0, max_y = 0;
  for (int i = 0; i < arraysize(polygon); i++) {
    max_x = std::max(max_x, static_cast<int>(polygon[i].x));
    max_y = std::max(max_y, static_cast<int>(polygon[i].y));
  }

  // We then create the polygon and use SetWindowRgn to force the window to draw
  // only within that area. This region may get reduced in size below.
  HRGN region = CreatePolygonRgn(polygon, arraysize(polygon), ALTERNATE);

  // Are we animating?
  if (find_dialog_animation_offset_ > 0) {
    // The animation happens in two steps: First, we clip the window and then in
    // GetDialogPosition we offset the window position so that it still looks
    // attached to the toolbar as it grows. We clip the window by creating a
    // rectangle region (that gradually increases as the animation progresses)
    // and find the intersection between the two regions using CombineRgn.

    // |y| shrinks as the animation progresses from the height of the view down
    // to 0 (and reverses when closing).
    int y = find_dialog_animation_offset_;
    // |y| shrinking means the animation (visible) region gets larger. In other
    // words: the rectangle grows upward (when the dialog is opening).
    HRGN animation_region = CreateRectRgn(0, y, max_x, max_y);
    // |region| will contain the intersected parts after calling this function:
    CombineRgn(region, animation_region, region, RGN_AND);
    DeleteObject(animation_region);

    // Next, we need to increase the region a little bit to account for the
    // curved edges that the view will draw to make it look like grows out of
    // the toolbar.
    POINT left_curve[] = {
      {0, y+0}, {0, y+1}, {2, y+3}, {2, y+0}, {0, y+0}
    };
    POINT right_curve[] = {
      {w+3, y+3}, {w+6, y+0}, {w+3, y+0}, {w+3, y+3}
    };

    // Combine the region for the curve on the left with our main region.
    HRGN r = CreatePolygonRgn(left_curve, arraysize(left_curve), ALTERNATE);
    CombineRgn(region, r, region, RGN_OR);
    DeleteObject(r);

    // Combine the region for the curve on the right with our main region.
    r = CreatePolygonRgn(right_curve, arraysize(right_curve), ALTERNATE);
    CombineRgn(region, r, region, RGN_OR);
    DeleteObject(r);
  }

  // Now see if we need to truncate the region because parts of it obscures
  // the main window border.
  gfx::Rect dialog_bounds;
  GetDialogBounds(&dialog_bounds);

  // Calculate how much our current position overlaps our boundaries. If we
  // overlap, it means we have too little space to draw the whole dialog and
  // we allow overwriting the scrollbar before we start truncating our dialog.
  //
  // TODO(brettw) this constant is evil. This is the amount of room we've added
  // to the window size, when we set the region, it can change the size.
  static const int kAddedWidth = 7;
  int difference = (new_pos.right() - kAddedWidth) -
                   dialog_bounds.width() -
                   views::NativeScrollBar::GetVerticalScrollBarWidth() +
                   1;
  if (difference > 0) {
    POINT exclude[4] = {0};
    exclude[0].x = max_x - difference;  // Top left corner.
    exclude[0].y = 0;

    exclude[1].x = max_x;               // Top right corner.
    exclude[1].y = 0;

    exclude[2].x = max_x;               // Bottom right corner.
    exclude[2].y = max_y;

    exclude[3].x = max_x - difference;  // Bottom left corner.
    exclude[3].y = max_y;

    // Subtract this region from the original region.
    HRGN exclude_rgn = CreatePolygonRgn(exclude, arraysize(exclude), ALTERNATE);
    int result = CombineRgn(region, region, exclude_rgn, RGN_DIFF);
    DeleteObject(exclude_rgn);
  }

  // The system now owns the region, so we do not delete it.
  host_->SetWindowRgn(region, TRUE);  // TRUE = Redraw.
#endif
}

void FindBarWin::Show() {
  if (disable_animations_during_testing_) {
    animation_->Reset(1);
    MoveWindowIfNecessary(gfx::Rect(), true);
  } else {
    animation_->Reset();
    animation_->Show();
  }
}

void FindBarWin::SetFocusAndSelection() {
  view_->SetFocusAndSelection();
}

bool FindBarWin::IsAnimating() {
  return animation_->IsAnimating();
}

void FindBarWin::Hide(bool animate) {
  if (animate && !disable_animations_during_testing_) {
    animation_->Reset(1.0);
    animation_->Hide();
  } else {
    host_->Hide();
  }
}

void FindBarWin::ClearResults(const FindNotificationDetails& results) {
  view_->UpdateForResult(results, string16());
}

void FindBarWin::StopAnimation() {
  if (animation_->IsAnimating())
    animation_->End();
}

void FindBarWin::SetFindText(const string16& find_text) {
  view_->SetFindText(find_text);
}

bool FindBarWin::IsFindBarVisible() {
  return host_->IsVisible();
}

void FindBarWin::MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                       bool no_redraw) {
  // We only move the window if one is active for the current TabContents. If we
  // don't check this, then SetDialogPosition below will end up making the Find
  // Bar visible.
  if (!find_bar_controller_->tab_contents() ||
      !find_bar_controller_->tab_contents()->find_ui_active()) {
    return;
  }

  gfx::Rect new_pos = GetDialogPosition(selection_rect);
  SetDialogPosition(new_pos, no_redraw);

  // May need to redraw our frame to accommodate bookmark bar styles.
  view_->SchedulePaint();
}

#if defined(OS_WIN)
bool FindBarWin::MaybeForwardKeystrokeToWebpage(
    UINT message, TCHAR key, UINT flags) {
  // We specifically ignore WM_CHAR. See http://crbug.com/10509.
  if (message != WM_KEYDOWN && message != WM_KEYUP)
    return false;

  switch (key) {
    case VK_HOME:
    case VK_END:
      // Ctrl+Home and Ctrl+End should be forwarded to the page.
      if (GetKeyState(VK_CONTROL) >= 0)
        return false;  // Ctrl not pressed: Abort. Otherwise fall through.
    case VK_UP:
    case VK_DOWN:
    case VK_PRIOR:  // Page up
    case VK_NEXT:   // Page down
      break;  // The keys above are the ones we want to forward to the page.
    default:
      return false;
  }

  TabContents* contents = find_bar_controller_->tab_contents();
  if (!contents)
    return false;

  RenderViewHost* render_view_host = contents->render_view_host();

  // Make sure we don't have a text field element interfering with keyboard
  // input. Otherwise Up and Down arrow key strokes get eaten. "Nom Nom Nom".
  render_view_host->ClearFocusedNode();

  HWND hwnd = contents->GetContentNativeView();
  render_view_host->ForwardKeyboardEvent(
      NativeWebKeyboardEvent(hwnd, message, key, 0));
  return true;
}
#endif

void FindBarWin::OnFinalMessage() {
  // TODO(beng): Destroy the RootView before destroying the Focus Manager will
  //             allow us to remove this method.

  // We are exiting, so we no longer need to monitor focus changes.
  focus_manager_->RemoveFocusChangeListener(this);

  // Destroy the focus tracker now, otherwise by the time we're destroyed the
  // focus manager the focus tracker is referencing may have already been
  // destroyed resulting in the focus tracker trying to reference a deleted
  // focus manager.
  focus_tracker_.reset(NULL);
};

bool FindBarWin::IsVisible() {
  return host_->IsVisible();
}

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, views::FocusChangeListener implementation:

void FindBarWin::FocusWillChange(views::View* focused_before,
                                 views::View* focused_now) {
  // First we need to determine if one or both of the views passed in are child
  // views of our view.
  bool our_view_before = focused_before && view_->IsParentOf(focused_before);
  bool our_view_now = focused_now && view_->IsParentOf(focused_now);

  // When both our_view_before and our_view_now are false, it means focus is
  // changing hands elsewhere in the application (and we shouldn't do anything).
  // Similarly, when both are true, focus is changing hands within the Find
  // window (and again, we should not do anything). We therefore only need to
  // look at when we gain initial focus and when we loose it.
  if (!our_view_before && our_view_now) {
    // We are gaining focus from outside the Find window so we must register
    // a handler for Escape.
    RegisterEscAccelerator();
  } else if (our_view_before && !our_view_now) {
    // We are losing focus to something outside our window so we restore the
    // original handler for Escape.
    UnregisterEscAccelerator();
  }
}

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, views::AcceleratorTarget implementation:

bool FindBarWin::AcceleratorPressed(const views::Accelerator& accelerator) {
#if defined(OS_WIN)
  DCHECK(accelerator.GetKeyCode() == VK_ESCAPE);  // We only expect Escape key.
#endif
  // This will end the Find session and hide the window, causing it to loose
  // focus and in the process unregister us as the handler for the Escape
  // accelerator through the FocusWillChange event.
  find_bar_controller_->EndFindSession();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, AnimationDelegate implementation:

void FindBarWin::AnimationProgressed(const Animation* animation) {
  // First, we calculate how many pixels to slide the window.
  gfx::Size pref_size = view_->GetPreferredSize();
  find_dialog_animation_offset_ =
      static_cast<int>((1.0 - animation_->GetCurrentValue()) *
      pref_size.height());

  // This call makes sure it appears in the right location, the size and shape
  // is correct and that it slides in the right direction.
  gfx::Rect find_dlg_rect = GetDialogPosition(gfx::Rect());
  SetDialogPosition(find_dlg_rect, false);

  // Let the view know if we are animating, and at which offset to draw the
  // edges.
  view_->animation_offset(find_dialog_animation_offset_);
  view_->SchedulePaint();
}

void FindBarWin::AnimationEnded(const Animation* animation) {
  // Place the find bar in its fully opened state.
  find_dialog_animation_offset_ = 0;

  if (!animation_->IsShowing()) {
    // Animation has finished closing.
    host_->Hide();
  } else {
    // Animation has finished opening.
  }
}

void FindBarWin::GetThemePosition(gfx::Rect* bounds) {
  *bounds = GetDialogPosition(gfx::Rect());
  gfx::Rect tab_strip_bounds = browser_view_->GetTabStripBounds();
  bounds->Offset(-tab_strip_bounds.x(), -tab_strip_bounds.y());
}

////////////////////////////////////////////////////////////////////////////////
// FindBarTesting implementation:

bool FindBarWin::GetFindBarWindowInfo(gfx::Point* position,
                                      bool* fully_visible) {
  if (!find_bar_controller_ ||
#if defined(OS_WIN)
      !::IsWindow(host_->GetNativeView())) {
#else
      false) {
      // TODO(sky): figure out linux side.
#endif
    *position = gfx::Point();
    *fully_visible = false;
    return false;
  }

  gfx::Rect window_rect;
  host_->GetBounds(&window_rect, true);
  *position = window_rect.origin();
  *fully_visible = host_->IsVisible() && !IsAnimating();
  return true;
}

void FindBarWin::GetDialogBounds(gfx::Rect* bounds) {
  DCHECK(bounds);
  // The BrowserView does Layout for the components that we care about
  // positioning relative to, so we ask it to tell us where we should go.
  *bounds = browser_view_->GetFindBarBoundingBox();
}

gfx::Rect FindBarWin::GetDialogPosition(gfx::Rect avoid_overlapping_rect) {
  // Find the area we have to work with (after accounting for scrollbars, etc).
  gfx::Rect dialog_bounds;
  GetDialogBounds(&dialog_bounds);
  if (dialog_bounds.IsEmpty())
    return gfx::Rect();

  // Ask the view how large an area it needs to draw on.
  gfx::Size prefsize = view_->GetPreferredSize();

  // Place the view in the top right corner of the dialog boundaries (top left
  // for RTL languages).
  gfx::Rect view_location;
  int x = view_->UILayoutIsRightToLeft() ?
              dialog_bounds.x() : dialog_bounds.width() - prefsize.width();
  int y = dialog_bounds.y();
  view_location.SetRect(x, y, prefsize.width(), prefsize.height());

  // Make sure we don't go out of bounds to the left (right in RTL) if the
  // window is too small to fit our dialog.
  if (view_->UILayoutIsRightToLeft()) {
    int boundary = dialog_bounds.width() - prefsize.width();
    view_location.set_x(std::min(view_location.x(), boundary));
  } else {
    view_location.set_x(std::max(view_location.x(), dialog_bounds.x()));
  }

  gfx::Rect new_pos = view_location;

  // When we get Find results back, we specify a selection rect, which we
  // should strive to avoid overlapping. But first, we need to offset the
  // selection rect (if one was provided).
  if (!avoid_overlapping_rect.IsEmpty()) {
    // For comparison (with the Intersects function below) we need to account
    // for the fact that we draw the Find dialog relative to the window,
    // whereas the selection rect is relative to the page.
#if defined(OS_WIN)
    RECT frame_rect = {0}, webcontents_rect = {0};
    ::GetWindowRect(host_->GetParent(), &frame_rect);
    ::GetWindowRect(
        find_bar_controller_->tab_contents()->view()->GetNativeView(),
        &webcontents_rect);
    avoid_overlapping_rect.Offset(0, webcontents_rect.top - frame_rect.top);
#else
    NOTIMPLEMENTED();
#endif
  }

  // If the selection rectangle intersects the current position on screen then
  // we try to move our dialog to the left (right for RTL) of the selection
  // rectangle.
  if (!avoid_overlapping_rect.IsEmpty() &&
      avoid_overlapping_rect.Intersects(new_pos)) {
    if (view_->UILayoutIsRightToLeft()) {
      new_pos.set_x(avoid_overlapping_rect.x() +
                    avoid_overlapping_rect.width() +
                    (2 * kMinFindWndDistanceFromSelection));

      // If we moved it off-screen to the right, we won't move it at all.
      if (new_pos.x() + new_pos.width() > dialog_bounds.width())
        new_pos = view_location;  // Reset.
    } else {
      new_pos.set_x(avoid_overlapping_rect.x() - new_pos.width() -
        kMinFindWndDistanceFromSelection);

      // If we moved it off-screen to the left, we won't move it at all.
      if (new_pos.x() < 0)
        new_pos = view_location;  // Reset.
    }
  }

  // While we are animating, the Find window will grow bottoms up so we need to
  // re-position the dialog so that it appears to grow out of the toolbar.
  if (find_dialog_animation_offset_ > 0)
    new_pos.Offset(0, std::min(0, -find_dialog_animation_offset_));

  return new_pos;
}

void FindBarWin::SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw) {
  if (new_pos.IsEmpty())
    return;

  // Make sure the window edges are clipped to just the visible region. We need
  // to do this before changing position, so that when we animate the closure
  // of it it doesn't look like the window crumbles into the toolbar.
  UpdateWindowEdges(new_pos);

#if defined(OS_WIN)
  gfx::Rect window_rect;
  host_->GetBounds(&window_rect, true);
  DWORD swp_flags = SWP_NOOWNERZORDER;
  if (!window_rect.IsEmpty())
    swp_flags |= SWP_NOSIZE;
  if (no_redraw)
    swp_flags |= SWP_NOREDRAW;
  if (!host_->IsVisible())
    swp_flags |= SWP_SHOWWINDOW;

  ::SetWindowPos(host_->GetNativeView(), HWND_TOP, new_pos.x(), new_pos.y(),
                 new_pos.width(), new_pos.height(), swp_flags);
#else
  host_->SetBounds(new_pos);
#endif
}

void FindBarWin::RestoreSavedFocus() {
  if (focus_tracker_.get() == NULL) {
    // TODO(brettw) Focus() should be on TabContentsView.
    find_bar_controller_->tab_contents()->Focus();
  } else {
    focus_tracker_->FocusLastFocusedExternalView();
  }
}

FindBarTesting* FindBarWin::GetFindBarTesting() {
  return this;
}

void FindBarWin::RegisterEscAccelerator() {
#if defined(OS_WIN)
  DCHECK(!esc_accel_target_registered_);
  views::Accelerator escape(VK_ESCAPE, false, false, false);
  focus_manager_->RegisterAccelerator(escape, this);
  esc_accel_target_registered_ = true;
#else
  NOTIMPLEMENTED();
#endif
}

void FindBarWin::UnregisterEscAccelerator() {
#if defined(OS_WIN)
  DCHECK(esc_accel_target_registered_);
  views::Accelerator escape(VK_ESCAPE, false, false, false);
  focus_manager_->UnregisterAccelerator(escape, this);
  esc_accel_target_registered_ = false;
#else
  NOTIMPLEMENTED();
#endif
}

void FindBarWin::UpdateUIForFindResult(const FindNotificationDetails& result,
                                       const string16& find_text) {
  view_->UpdateForResult(result, find_text);

  // We now need to check if the window is obscuring the search results.
  if (!result.selection_rect().IsEmpty())
    MoveWindowIfNecessary(result.selection_rect(), false);

  // Once we find a match we no longer want to keep track of what had
  // focus. EndFindSession will then set the focus to the page content.
  if (result.number_of_matches() > 0)
    focus_tracker_.reset(NULL);
}

void FindBarWin::AudibleAlert() {
#if defined(OS_WIN)
  MessageBeep(MB_OK);
#else
  NOTIMPLEMENTED();
#endif
}
