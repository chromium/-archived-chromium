// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/find_bar_win.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/find_notification_details.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/find_bar_view.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/tab_contents/web_contents_view.h"
#include "chrome/views/external_focus_tracker.h"
#include "chrome/views/native_scroll_bar.h"
#include "chrome/views/root_view.h"
#include "chrome/views/view_storage.h"
#include "chrome/views/widget_win.h"

int FindBarWin::request_id_counter_ = 0;

// The minimum space between the FindInPage window and the search result.
static const int kMinFindWndDistanceFromSelection = 5;

// The amount of space we expect the window border to take up.
static const int kWindowBorderWidth = 3;

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, public:

FindBarWin::FindBarWin(WebContentsView* parent_tab, HWND parent_hwnd)
    : parent_tab_(parent_tab),
      current_request_id_(request_id_counter_++),
      parent_hwnd_(parent_hwnd),
      find_dialog_animation_offset_(0),
      show_on_tab_selection_(false),
      focus_manager_(NULL),
      old_accel_target_for_esc_(NULL) {
  // Start listening to focus changes, so we can register and unregister our
  // own handler for Escape.
  SetFocusChangeListener(parent_hwnd);

  // Don't let WidgetWin manage our lifetime. We want our lifetime to
  // coincide with WebContents.
  WidgetWin::set_delete_on_destroy(false);

  view_ = new FindBarView(this);

  views::FocusManager* focus_manager;
  focus_manager = views::FocusManager::GetFocusManager(parent_hwnd_);
  DCHECK(focus_manager);

  // Stores the currently focused view, and tracks focus changes so that we can
  // restore focus when the find box is closed.
  focus_tracker_.reset(new views::ExternalFocusTracker(view_, focus_manager));

  // Figure out where to place the dialog, initialize and set the position.
  gfx::Rect find_dlg_rect = GetDialogPosition(gfx::Rect());
  set_window_style(WS_CHILD | WS_CLIPCHILDREN);
  set_window_ex_style(WS_EX_TOPMOST);
  WidgetWin::Init(parent_hwnd, find_dlg_rect, false);
  SetContentsView(view_);

  // Start the process of animating the opening of the window.
  animation_.reset(new SlideAnimation(this));
  animation_->Show();
}

FindBarWin::~FindBarWin() {
  Close();
}

// TODO(brettw) this should not be so complicated. The view should really be in
// charge of these regions. CustomFrameWindow will do this for us. It will also
// let us set a path for the window region which will avoid some logic here.
void FindBarWin::UpdateWindowEdges(const gfx::Rect& new_pos) {
  int w = new_pos.width();
  int h = new_pos.height();

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
  static const int kAddedWidth = 14;
  int difference = (curr_pos_relative_.right() - kAddedWidth) -
                   dialog_bounds.width() -
                   views::NativeScrollBar::GetVerticalScrollBarWidth() +
                   1;
  if (difference > 0) {
    POINT exclude[4];
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
  SetWindowRgn(region, TRUE);  // TRUE = Redraw.
}

void FindBarWin::Show() {
  // Note: This function is called when the user presses Ctrl+F or switches back
  // to the parent tab of the Find window (assuming the Find window has been
  // opened at least once). If the Find window is already visible, we should
  // just forward the command to the view so that it will select all text and
  // grab focus. If the window is not visible, however, there are two scenarios:
  if (!IsVisible() && !animation_->IsAnimating()) {
    if (show_on_tab_selection_) {
      // The tab just got re-selected and we need to show the window again
      // (without animation). We also want to reset the window location so that
      // we don't surprise the user by popping up to the left for no apparent
      // reason.
      gfx::Rect new_pos = GetDialogPosition(gfx::Rect());
      SetDialogPosition(new_pos);
    } else {
      // The Find window was dismissed and we need to start the animation again.
      animation_->Show();
    }
  }

  view_->OnShow();
}

bool FindBarWin::IsAnimating() {
  return animation_->IsAnimating();
}

void FindBarWin::EndFindSession() {
  if (IsVisible()) {
    show_on_tab_selection_ = false;
    animation_->Hide();

    // We reset the match count here so that we don't show old results when the
    // user has navigated to another page. We could alternatively achieve the
    // same effect by nulling the search string, but then the user looses the
    // last search that was entered, which can be frustrating if searching for
    // the same string on multiple pages.
    view_->ResetMatchCount();

    // When we hide the window, we need to notify the renderer that we are done
    // for now, so that we can abort the scoping effort and clear all the
    // tick-marks and highlighting.
    StopFinding(false);  // false = don't clear selection on page.

    // When we get dismissed we restore the focus to where it belongs.
    RestoreSavedFocus();
  }
}

void FindBarWin::Close() {
  // We may already have been destroyed if the selection resulted in a tab
  // switch which will have reactivated the browser window and closed us, so
  // we need to check to see if we're still a window before trying to destroy
  // ourself.
  if (IsWindow())
    DestroyWindow();
}

void FindBarWin::DidBecomeSelected() {
  if (!IsVisible() && show_on_tab_selection_) {
    Show();
    show_on_tab_selection_ = false;
  }
}

void FindBarWin::DidBecomeUnselected() {
  if (::IsWindow(GetHWND()) && IsVisible()) {
    // Finish any existing animations.
    if (animation_->IsAnimating()) {
      show_on_tab_selection_ = animation_->IsShowing();
      animation_->End();
    } else {
      show_on_tab_selection_ = true;
    }

    ShowWindow(SW_HIDE);
  }
}

void FindBarWin::StartFinding(bool forward_direction) {
  if (find_string_.empty())
    return;

  bool find_next = last_find_string_ == find_string_;
  if (!find_next)
    current_request_id_ = request_id_counter_++;

  last_find_string_ = find_string_;

  GetRenderViewHost()->StartFinding(current_request_id_,
                                    find_string_,
                                    forward_direction,
                                    false,  // case sensitive
                                    find_next);
}

void FindBarWin::StopFinding(bool clear_selection) {
  last_find_string_.clear();
  GetRenderViewHost()->StopFinding(clear_selection);
}

void FindBarWin::MoveWindowIfNecessary(
    const gfx::Rect& selection_rect) {
  gfx::Rect new_pos = GetDialogPosition(selection_rect);
  SetDialogPosition(new_pos);

  // May need to redraw our frame to accomodate bookmark bar
  // styles.
  view_->SchedulePaint();
}

void FindBarWin::RespondToResize(const gfx::Size& new_size) {
  if (!IsVisible())
    return;

  // We are only interested in changes to width.
  if (window_size_.width() == new_size.width())
    return;

  // Save the new size so we can compare later and ignore future invocations
  // of RespondToResize.
  window_size_ = new_size;

  gfx::Rect new_pos = GetDialogPosition(gfx::Rect());
  SetDialogPosition(new_pos);
}

void FindBarWin::SetParent(HWND new_parent) {
  DCHECK(new_parent);
  if (parent_hwnd_ != new_parent) {
    // Sync up the focus listener with the new focus manager.
    SetFocusChangeListener(new_parent);

    parent_hwnd_ = new_parent;
    ::SetParent(GetHWND(), new_parent);

    // The MSDN documentation specifies that you need to manually update the
    // UI state after changing the parent.
    ::SendMessage(new_parent,
                  WM_CHANGEUISTATE, MAKEWPARAM(UIS_INITIALIZE, 0), 0);

    // We have a new focus manager now, so start tracking with that.
    focus_tracker_.reset(new views::ExternalFocusTracker(view_,
                                                         focus_manager_));
  }
}

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, views::WidgetWin implementation:

void FindBarWin::OnFinalMessage(HWND window) {
  // We are exiting, so we no longer need to monitor focus changes.
  focus_manager_->RemoveFocusChangeListener(this);

  // Destroy the focus tracker now, otherwise by the time we're destroyed the
  // focus manager the focus tracker is referencing may have already been
  // destroyed resulting in the focus tracker trying to reference a deleted
  // focus manager.
  focus_tracker_.reset(NULL);
};

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
  DCHECK(accelerator.GetKeyCode() == VK_ESCAPE);  // We only expect Escape key.
  // This will end the Find session and hide the window, causing it to loose
  // focus and in the process unregister us as the handler for the Escape
  // accelerator through the FocusWillChange event.
  EndFindSession();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, AnimationDelegate implementation:

void FindBarWin::AnimationProgressed(const Animation* animation) {
  // First, we calculate how many pixels to slide the window.
  find_dialog_animation_offset_ =
      static_cast<int>((1.0 - animation_->GetCurrentValue()) *
      view_->height());

  // This call makes sure it appears in the right location, the size and shape
  // is correct and that it slides in the right direction.
  gfx::Rect find_dlg_rect = GetDialogPosition(gfx::Rect());
  SetDialogPosition(find_dlg_rect);

  // Let the view know if we are animating, and at which offset to draw the
  // edges.
  view_->animation_offset(find_dialog_animation_offset_);
  view_->SchedulePaint();
}

void FindBarWin::AnimationEnded(const Animation* animation) {
  if (!animation_->IsShowing()) {
    // Animation has finished closing.
    find_dialog_animation_offset_ = 0;
    ShowWindow(SW_HIDE);
  } else {
    // Animation has finished opening.
  }
}

void FindBarWin::OnFindReply(int request_id,
                             int number_of_matches,
                             const gfx::Rect& selection_rect,
                             int active_match_ordinal,
                             bool final_update) {
  // Ignore responses for requests other than the one we have most recently
  // issued. That way we won't act on stale results when the user has
  // already typed in another query.
  if (view_ && request_id == current_request_id_) {
    view_->UpdateMatchCount(number_of_matches, final_update);
    view_->UpdateActiveMatchOrdinal(active_match_ordinal);
    view_->UpdateResultLabel();

    // We now need to check if the window is obscuring the search results.
    if (!selection_rect.IsEmpty())
      MoveWindowIfNecessary(selection_rect);

    // Once we find a match we no longer want to keep track of what had
    // focus. EndFindSession will then set the focus to the page content.
    if (number_of_matches > 0)
      focus_tracker_.reset(NULL);
  }

  // Notify all observers of this notification, such as the automation
  // providers which do UI tests for find in page.
  FindNotificationDetails detail(request_id,
                                 number_of_matches,
                                 selection_rect,
                                 active_match_ordinal,
                                 final_update);
  NotificationService::current()->Notify(
      NOTIFY_FIND_RESULT_AVAILABLE,
      Source<TabContents>(parent_tab_->GetWebContents()),
      Details<FindNotificationDetails>(&detail));
}

RenderViewHost* FindBarWin::GetRenderViewHost() const {
  return parent_tab_->GetWebContents()->render_view_host();
}

void FindBarWin::GetDialogBounds(gfx::Rect* bounds) {
  DCHECK(bounds);

  // We need to find the View for the toolbar because we want to visually
  // extend it (draw our dialog slightly overlapping its border).
  views::View* root_view = views::GetRootViewForHWND(parent_hwnd_);
  views::View* toolbar = NULL;
  BookmarkBarView* bookmark_bar = NULL;
  if (root_view) {
    toolbar = root_view->GetViewByID(VIEW_ID_TOOLBAR);
    bookmark_bar = static_cast<BookmarkBarView*>(
        root_view->GetViewByID(VIEW_ID_BOOKMARK_BAR));
  }

  // To figure out what area we have to work with we need to know the rect for
  // the browser window and the page content area starts and get a pointer to
  // the toolbar to see where to draw the FindInPage dialog. If any of this
  // fails, we return an empty rect.
  CRect browser_client_rect, browser_window_rect, content_window_rect;
  if (!::IsWindow(parent_hwnd_) ||
      !::GetWindowRect(parent_tab_->GetContentHWND(), &content_window_rect) ||
      !::GetWindowRect(parent_hwnd_, &browser_window_rect) ||
      !::GetClientRect(parent_hwnd_, &browser_client_rect) ||
      !toolbar) {
    *bounds = gfx::Rect();
    return;
  }

  // Start with browser's client rect, then change it below.
  *bounds = gfx::Rect(browser_client_rect);

  // Find the dimensions of the toolbar and the BookmarkBar.
  gfx::Rect toolbar_bounds, bookmark_bar_bounds;
  if (toolbar) {
    toolbar_bounds = toolbar->GetLocalBounds(false);
    // Need to convert toolbar bounds into Widget coords because the toolbar
    // is the child of another view that isn't the top level view. This is
    // required to ensure correct positioning relative to the top,left of the
    // window.
    gfx::Point topleft;
    views::View::ConvertPointToWidget(toolbar, &topleft);
    toolbar_bounds.Offset(topleft.x(), topleft.y());
  }

  // If the bookmarks bar is available, we need to update our
  // position and paint accordingly
  if (bookmark_bar) {
    if (bookmark_bar->IsAlwaysShown()) {
      // If it's always on, don't try to blend with the toolbar.
      view_->SetToolbarBlend(false);
    } else {
      // Else it's on, but hidden (in which case we should try
      // to blend with the toolbar.
      view_->SetToolbarBlend(true);
    }

    // If we're not in the New Tab page style, align ourselves with
    // the bookmarks bar (this works even if the bar is hidden).
    if (!bookmark_bar->OnNewTabPage() ||
        bookmark_bar->IsAlwaysShown()) {
      bookmark_bar_bounds = bookmark_bar->bounds();
    }
  } else {
    view_->SetToolbarBlend(true);
  }

  // Figure out at which y coordinate to draw the FindInPage window. If we have
  // a toolbar (chrome) we want to overlap it by one pixel so that we look like
  // we are part of the chrome (which will also draw our window on top of any
  // info-bars, if present). If there is no chrome, then we have a constrained
  // window or a Chrome application so we want to draw at the top of the page
  // content (right beneath the title bar).
  int y_pos_offset = 0;
  if (!toolbar_bounds.IsEmpty()) {
    // We have a toolbar (chrome), so overlap it by one pixel.
    y_pos_offset = toolbar_bounds.bottom() - 1;
    // If there is a bookmark bar attached to the toolbar we should appear
    // attached to it instead of the toolbar.
    if (!bookmark_bar_bounds.IsEmpty()) {
      y_pos_offset += bookmark_bar_bounds.height() - 1 - 
          bookmark_bar->GetToolbarOverlap();
    }
  } else {
    // There is no toolbar, so this is probably a constrained window or a Chrome
    // Application. This means we draw the Find window at the top of the page
    // content window. We subtract 1 to overlap the light-blue line that is part
    // of the title bar (so that we don't look detached by 1 pixel).
    WINDOWINFO wi;
    wi.cbSize = sizeof(WINDOWINFO);
    GetWindowInfo(parent_hwnd_, &wi);
    y_pos_offset = content_window_rect.TopLeft().y - wi.rcClient.top - 1;
  }

  bounds->Offset(0, y_pos_offset);

  // We also want to stay well within limits of the vertical scrollbar and not
  // draw on the window border (frame) itself either.
  int width = views::NativeScrollBar::GetVerticalScrollBarWidth();
  width += kWindowBorderWidth;
  bounds->set_x(bounds->x() + width);
  bounds->set_width(bounds->width() - (2 * width));
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
    RECT frame_rect = {0}, webcontents_rect = {0};
    ::GetWindowRect(parent_hwnd_, &frame_rect);
    ::GetWindowRect(parent_tab_->GetContainerHWND(), &webcontents_rect);
    avoid_overlapping_rect.Offset(0, webcontents_rect.top - frame_rect.top);
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

void FindBarWin::SetDialogPosition(const gfx::Rect& new_pos) {
  if (new_pos.IsEmpty())
    return;

  // Make sure the window edges are clipped to just the visible region. We need
  // to do this before changing position, so that when we animate the closure
  // of it it doesn't look like the window crumbles into the toolbar.
  UpdateWindowEdges(new_pos);

  ::SetWindowPos(GetHWND(), HWND_TOP,
                 new_pos.x(), new_pos.y(), new_pos.width(), new_pos.height(),
                 SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);

  curr_pos_relative_ = new_pos;
}

void FindBarWin::SetFocusChangeListener(HWND parent_hwnd) {
  // When tabs get torn off the tab-strip they get a new window with a new
  // FocusManager, which means we need to clean up old listener and start a new
  // one with the new FocusManager.
  if (focus_manager_) {
    if (old_accel_target_for_esc_)
      UnregisterEscAccelerator();
    focus_manager_->RemoveFocusChangeListener(this);
  }

  // Register as a listener with the new focus manager.
  focus_manager_ = views::FocusManager::GetFocusManager(parent_hwnd);
  DCHECK(focus_manager_);
  focus_manager_->AddFocusChangeListener(this);
}

void FindBarWin::RestoreSavedFocus() {
  if (focus_tracker_.get() == NULL) {
    // TODO(brettw) Focus() should be on WebContentsView.
    parent_tab_->GetWebContents()->Focus();
  } else {
    focus_tracker_->FocusLastFocusedExternalView();
  }
}

void FindBarWin::RegisterEscAccelerator() {
  views::Accelerator escape(VK_ESCAPE, false, false, false);

  // TODO(finnur): Once we fix issue 1307173 we should not remember any old
  // accelerator targets and just Register and Unregister when needed.
  views::AcceleratorTarget* old_target =
      focus_manager_->RegisterAccelerator(escape, this);

  if (!old_accel_target_for_esc_)
    old_accel_target_for_esc_ = old_target;
}

void FindBarWin::UnregisterEscAccelerator() {
  // TODO(finnur): Once we fix issue 1307173 we should not remember any old
  // accelerator targets and just Register and Unregister when needed.
  DCHECK(old_accel_target_for_esc_ != NULL);
  views::Accelerator escape(VK_ESCAPE, false, false, false);
  views::AcceleratorTarget* current_target =
      focus_manager_->GetTargetForAccelerator(escape);
  if (current_target == this)
    focus_manager_->RegisterAccelerator(escape, old_accel_target_for_esc_);
}

