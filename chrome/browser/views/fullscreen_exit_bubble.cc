// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/fullscreen_exit_bubble.h"

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/l10n_util_win.h"
#include "app/resource_bundle.h"
#include "chrome/app/chrome_dll_resource.h"
#include "grit/generated_resources.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_win.h"

// FullscreenExitView ----------------------------------------------------------

class FullscreenExitBubble::FullscreenExitView : public views::View {
 public:
   FullscreenExitView(FullscreenExitBubble* bubble,
                      views::WidgetWin* popup,
                      const std::wstring& accelerator);
  virtual ~FullscreenExitView();

  // views::View
  virtual gfx::Size GetPreferredSize();

 private:
  static const int kPaddingPixels;  // Number of pixels around all sides of link

  // views::View
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);

  // Handle to the HWND that contains us.
  views::WidgetWin* popup_;

  // Clickable hint text to show in the bubble.
  views::Link link_;
};

const int FullscreenExitBubble::FullscreenExitView::kPaddingPixels = 8;

FullscreenExitBubble::FullscreenExitView::FullscreenExitView(
    FullscreenExitBubble* bubble,
    views::WidgetWin* popup,
    const std::wstring& accelerator)
    : popup_(popup) {
  link_.SetParentOwned(false);
  link_.SetText(l10n_util::GetStringF(IDS_EXIT_FULLSCREEN_MODE, accelerator));
  link_.SetController(bubble);
  link_.SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::LargeFont));
  link_.SetNormalColor(SK_ColorWHITE);
  link_.SetHighlightedColor(SK_ColorWHITE);
  AddChildView(&link_);
}

FullscreenExitBubble::FullscreenExitView::~FullscreenExitView() {
}

gfx::Size FullscreenExitBubble::FullscreenExitView::GetPreferredSize() {
  gfx::Size preferred_size(link_.GetPreferredSize());
  preferred_size.Enlarge(kPaddingPixels * 2, kPaddingPixels * 2);
  return preferred_size;
}

void FullscreenExitBubble::FullscreenExitView::Layout() {
  gfx::Size link_preferred_size(link_.GetPreferredSize());
  link_.SetBounds(kPaddingPixels,
                  height() - kPaddingPixels - link_preferred_size.height(),
                  link_preferred_size.width(), link_preferred_size.height());
}

void FullscreenExitBubble::FullscreenExitView::Paint(gfx::Canvas* canvas) {
  // Create a round-bottomed rect to fill the whole View.
  SkRect rect;
  SkScalar padding = SkIntToScalar(kPaddingPixels);
  // The "-padding" top coordinate ensures that the rect is always tall enough
  // to contain the complete rounded corner radius.  If we set this to 0, as the
  // popup slides offscreen (in reality, squishes to 0 height), the corners will
  // flatten out as the height becomes less than the corner radius.
  rect.set(0, -padding, SkIntToScalar(width()), SkIntToScalar(height()));
  SkScalar rad[8] = { 0, 0, 0, 0, padding, padding, padding, padding };
  SkPath path;
  path.addRoundRect(rect, rad, SkPath::kCW_Direction);

  // Fill it black.
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setColor(SK_ColorBLACK);
  canvas->drawPath(path, paint);
}


// FullscreenExitPopup ---------------------------------------------------------

class FullscreenExitBubble::FullscreenExitPopup : public views::WidgetWin {
 public:
  FullscreenExitPopup() : views::WidgetWin() { }
  virtual ~FullscreenExitPopup() { }

  // views::WidgetWin:
  virtual LRESULT OnMouseActivate(HWND window,
                                  UINT hittest_code,
                                  UINT message) {
    // Prevent the popup from being activated, so it won't steal focus from the
    // rest of the browser, and doesn't cause problems with the FocusManager's
    // "RestoreFocusedView()" functionality.
    return MA_NOACTIVATE;
  }
};


// FullscreenExitBubble --------------------------------------------------------

const double FullscreenExitBubble::kOpacity = 0.7;
const int FullscreenExitBubble::kInitialDelayMs = 2300;
const int FullscreenExitBubble::kIdleTimeMs = 2300;
const int FullscreenExitBubble::kPositionCheckHz = 10;
const int FullscreenExitBubble::kSlideInRegionHeightPx = 4;
const int FullscreenExitBubble::kSlideInDurationMs = 350;
const int FullscreenExitBubble::kSlideOutDurationMs = 700;

FullscreenExitBubble::FullscreenExitBubble(
    views::Widget* frame,
    CommandUpdater::CommandUpdaterDelegate* delegate)
    : root_view_(frame->GetRootView()),
      delegate_(delegate),
      popup_(new FullscreenExitPopup()),
      size_animation_(new SlideAnimation(this)) {
  size_animation_->Reset(1);

  // Create the contents view.
  views::Accelerator accelerator(0, false, false, false);
  bool got_accelerator = frame->GetAccelerator(IDC_FULLSCREEN, &accelerator);
  DCHECK(got_accelerator);
  view_ = new FullscreenExitView(this, popup_, accelerator.GetShortcutText());

  // Initialize the popup.
  popup_->set_delete_on_destroy(false);
  popup_->set_window_style(WS_POPUP);
  popup_->set_window_ex_style(WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                              l10n_util::GetExtendedTooltipStyles());
  popup_->SetOpacity(static_cast<unsigned char>(0xff * kOpacity));
  popup_->Init(frame->GetNativeView(), GetPopupRect(false));
  popup_->SetContentsView(view_);
  popup_->Show();  // This does not activate the popup.

  // Start the initial delay timer and begin watching the mouse.
  initial_delay_.Start(base::TimeDelta::FromMilliseconds(kInitialDelayMs), this,
                       &FullscreenExitBubble::CheckMousePosition);
  POINT cursor_pos;
  GetCursorPos(&cursor_pos);
  last_mouse_pos_ = cursor_pos;
  views::View::ConvertPointToView(NULL, root_view_, &last_mouse_pos_);
  mouse_position_checker_.Start(
      base::TimeDelta::FromMilliseconds(1000 / kPositionCheckHz), this,
      &FullscreenExitBubble::CheckMousePosition);
}

FullscreenExitBubble::~FullscreenExitBubble() {
  // This is tricky.  We may be in an ATL message handler stack, in which case
  // the popup cannot be deleted yet.  We also can't blindly use
  // set_delete_on_destroy(true) on the popup to delete it when it closes,
  // because if the user closed the last tab while in fullscreen mode, Windows
  // has already destroyed the popup HWND by the time we get here, and thus
  // either the popup will already have been deleted (if we set this in our
  // constructor) or the popup will never get another OnFinalMessage() call (if
  // not, as currently).  So instead, we tell the popup to synchronously hide,
  // and then asynchronously close and delete itself.
  popup_->Close();
  MessageLoop::current()->DeleteSoon(FROM_HERE, popup_);
}

void FullscreenExitBubble::LinkActivated(views::Link* source, int event_flags) {
  delegate_->ExecuteCommand(IDC_FULLSCREEN);
}

void FullscreenExitBubble::AnimationProgressed(
    const Animation* animation) {
  gfx::Rect popup_rect(GetPopupRect(false));
  if (popup_rect.IsEmpty()) {
    popup_->Hide();
  } else {
    popup_->MoveWindow(popup_rect.x(), popup_rect.y(), popup_rect.width(),
                       popup_rect.height());
    popup_->Show();
  }
}
void FullscreenExitBubble::AnimationEnded(
    const Animation* animation) {
  AnimationProgressed(animation);
}

void FullscreenExitBubble::CheckMousePosition() {
  // Desired behavior:
  //
  // +------------+-----------------------------+------------+
  // | _  _  _  _ | Exit full screen mode (F11) | _  _  _  _ |  Slide-in region
  // | _  _  _  _ \_____________________________/ _  _  _  _ |  Neutral region
  // |                                                       |  Slide-out region
  // :                                                       :
  //
  // * If app is not active, we hide the popup.
  // * If the mouse is offscreen or in the slide-out region, we hide the popup.
  // * If the mouse goes idle, we hide the popup.
  // * If the mouse is in the slide-in-region and not idle, we show the popup.
  // * If the mouse is in the neutral region and not idle, and the popup is
  //   currently sliding out, we show it again.  This facilitates users
  //   correcting us if they try to mouse horizontally towards the popup and
  //   unintentionally drop too low.
  // * Otherwise, we do nothing, because the mouse is in the neutral region and
  //   either the popup is hidden or the mouse is not idle, so we don't want to
  //   change anything's state.

  POINT cursor_pos;
  GetCursorPos(&cursor_pos);
  gfx::Point transformed_pos(cursor_pos);
  views::View::ConvertPointToView(NULL, root_view_, &transformed_pos);

  // Check to see whether the mouse is idle.
  if (transformed_pos != last_mouse_pos_) {
    // The mouse moved; reset the idle timer.
    idle_timeout_.Stop();  // If the timer isn't running, this is a no-op.
    idle_timeout_.Start(base::TimeDelta::FromMilliseconds(kIdleTimeMs), this,
                        &FullscreenExitBubble::CheckMousePosition);
  }
  last_mouse_pos_ = transformed_pos;

  if ((GetActiveWindow() != root_view_->GetWidget()->GetNativeView()) ||
      !root_view_->HitTest(transformed_pos) ||
      (cursor_pos.y >= GetPopupRect(true).bottom()) ||
      !idle_timeout_.IsRunning()) {
    // The cursor is offscreen, in the slide-out region, or idle.
    Hide();
  } else if ((cursor_pos.y < kSlideInRegionHeightPx) ||
             (size_animation_->GetCurrentValue() != 0)) {
    // The cursor is not idle, and either it's in the slide-in region or it's in
    // the neutral region and we're sliding out.
    size_animation_->SetSlideDuration(kSlideInDurationMs);
    size_animation_->Show();
  }
}

void FullscreenExitBubble::Hide() {
  // Allow the bubble to hide if the window is deactivated or our initial delay
  // finishes.
  if ((GetActiveWindow() != root_view_->GetWidget()->GetNativeView()) ||
      !initial_delay_.IsRunning()) {
    size_animation_->SetSlideDuration(kSlideOutDurationMs);
    size_animation_->Hide();
  }
}

gfx::Rect FullscreenExitBubble::GetPopupRect(
    bool ignore_animation_state) const {
  gfx::Size size(view_->GetPreferredSize());
  if (!ignore_animation_state) {
    size.set_height(static_cast<int>(static_cast<double>(size.height()) *
        size_animation_->GetCurrentValue()));
  }
  gfx::Point origin((root_view_->width() - size.width()) / 2, 0);
  views::View::ConvertPointToScreen(root_view_, &origin);
  return gfx::Rect(origin, size);
}
