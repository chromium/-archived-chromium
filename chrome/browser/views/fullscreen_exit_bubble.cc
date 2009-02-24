// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/fullscreen_exit_bubble.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/l10n_util_win.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/root_view.h"
#include "grit/generated_resources.h"


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
  virtual void Paint(ChromeCanvas* canvas);

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

void FullscreenExitBubble::FullscreenExitView::Paint(ChromeCanvas* canvas) {
  // Create a round-bottomed rect to fill the whole View.
  CRect parent_rect;
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


// FullscreenExitBubble --------------------------------------------------------

const double FullscreenExitBubble::kOpacity = 0.7;
const int FullscreenExitBubble::kInitialDelayMs = 2300;
const int FullscreenExitBubble::kPositionCheckHz = 10;
const int FullscreenExitBubble::kSlideInRegionHeightPx = 4;
const int FullscreenExitBubble::kSlideInDurationMs = 350;
const int FullscreenExitBubble::kSlideOutDurationMs = 700;

FullscreenExitBubble::FullscreenExitBubble(
    views::Widget* frame,
    CommandUpdater::CommandUpdaterDelegate* delegate)
    : root_view_(frame->GetRootView()),
      delegate_(delegate),
      popup_(new views::WidgetWin()),
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
  popup_->SetLayeredAlpha(static_cast<int>(0xff * kOpacity));
  popup_->Init(frame->GetHWND(), GetPopupRect(false), false);
  popup_->SetContentsView(view_);
  popup_->Show();

  // Start the initial delay timer.
  initial_delay_.Start(base::TimeDelta::FromMilliseconds(kInitialDelayMs), this,
                       &FullscreenExitBubble::AfterInitialDelay);
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

void FullscreenExitBubble::AfterInitialDelay() {
  // Check the mouse position immediately and every 50 ms afterwards.
  CheckMousePosition();
  mouse_position_checker_.Start(
      base::TimeDelta::FromMilliseconds(1000 / kPositionCheckHz), this,
      &FullscreenExitBubble::CheckMousePosition);
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
  // * If the mouse is in the slide-in region, we show the popup.
  // * If the mouse is in the slide-out region, we hide the popup.
  // * If the mouse is in the neutral region, we do nothing, except if the popup
  //   is currently sliding out, in which case we show it again.  This
  //   facilitates users correcting us if they try to mouse horizontally towards
  //   the popup and unintentionally drop too low.

  POINT cursor_pos;
  GetCursorPos(&cursor_pos);
  gfx::Point transformed_pos(cursor_pos);
  views::View::ConvertPointToView(NULL, root_view_, &transformed_pos);
  gfx::Rect trigger_rect(GetPopupRect(true));
  if (!root_view_->HitTest(transformed_pos) ||
      (cursor_pos.y >= trigger_rect.bottom())) {
    size_animation_->SetSlideDuration(kSlideOutDurationMs);
    size_animation_->Hide();
  } else if ((cursor_pos.y < kSlideInRegionHeightPx) ||
             (size_animation_->GetCurrentValue() != 0)) {
    size_animation_->SetSlideDuration(kSlideInDurationMs);
    size_animation_->Show();
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
