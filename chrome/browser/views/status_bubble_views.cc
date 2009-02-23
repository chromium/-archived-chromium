// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/status_bubble_views.h"

#include <algorithm>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/common/animation.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/label.h"
#include "chrome/views/root_view.h"
#include "chrome/views/widget_win.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkRect.h"

// The color of the background bubble.
static const SkColor kBubbleColor = SkColorSetRGB(222, 234, 248);

// The alpha and color of the bubble's shadow.
static const SkColor kShadowColor = SkColorSetARGB(30, 0, 0, 0);

// How wide the bubble's shadow is.
static const int kShadowSize = 1;

// The roundedness of the edges of our bubble.
static const int kBubbleCornerRadius = 4;

// How close the mouse can get to the infobubble before it starts sliding
// off-screen.
static const int kMousePadding = 20;

// The color of the text
static const SkColor kTextColor = SkColorSetRGB(100, 100, 100);

// The color of the highlight text
static const SkColor kTextHighlightColor = SkColorSetRGB(242, 250, 255);

static const int kTextPadding = 3;
static const int kTextPositionX = 4;
static const int kTextPositionY = 1;

// Delays before we start hiding or showing the bubble after we receive a
// show or hide request.
static const int kShowDelay = 80;
static const int kHideDelay = 250;

// How long each fade should last for.
static const int kShowFadeDurationMS = 120;
static const int kHideFadeDurationMS = 200;
static const int kFramerate = 25;

// View -----------------------------------------------------------------------
// StatusView manages the display of the bubble, applying text changes and
// fading in or out the bubble as required.
class StatusBubbleViews::StatusView : public views::Label,
                                      public Animation,
                                      public AnimationDelegate {
 public:
  StatusView(StatusBubble* status_bubble, views::WidgetWin* popup)
      : Animation(kFramerate, this),
        status_bubble_(status_bubble),
        popup_(popup),
        stage_(BUBBLE_HIDDEN),
        style_(STYLE_STANDARD),
        timer_factory_(this),
        opacity_start_(0),
        opacity_end_(0) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    ChromeFont font(rb.GetFont(ResourceBundle::BaseFont));
    SetFont(font);
  }

  ~StatusView() {
    Stop();
    CancelTimer();
  }

  // The bubble can be in one of many stages:
  typedef enum BubbleStage {
    BUBBLE_HIDDEN,         // Entirely BUBBLE_HIDDEN.
    BUBBLE_HIDING_FADE,    // In a fade-out transition.
    BUBBLE_HIDING_TIMER,   // Waiting before a fade-out.
    BUBBLE_SHOWING_TIMER,  // Waiting before a fade-in.
    BUBBLE_SHOWING_FADE,   // In a fade-in transition.
    BUBBLE_SHOWN           // Fully visible.
  };

  typedef enum BubbleStyle {
    STYLE_BOTTOM,
    STYLE_FLOATING,
    STYLE_STANDARD
  };

  // Set the bubble text to a certain value, hides the bubble if text is
  // an empty string.
  void SetText(const std::wstring& text);

  BubbleStage GetState() const { return stage_; }

  void SetStyle(BubbleStyle style);

  // Show the bubble instantly.
  void Show();

  // Hide the bubble instantly.
  void Hide();

  // Resets any timers we have. Typically called when the user moves a
  // mouse.
  void ResetTimer();

 private:
  class InitialTimer;

  // Manage the timers that control the delay before a fade begins or ends.
  void StartTimer(int time);
  void OnTimer();
  void CancelTimer();
  void RestartTimer(int delay);

  // Manage the fades and starting and stopping the animations correctly.
  void StartFade(double start, double end, int duration);
  void StartHiding();
  void StartShowing();

  // Animation functions.
  double GetCurrentOpacity();
  void SetOpacity(double opacity);
  void AnimateToState(double state);
  void AnimationEnded(const Animation* animation);

  virtual void Paint(ChromeCanvas* canvas);

  BubbleStage stage_;
  BubbleStyle style_;

  ScopedRunnableMethodFactory<StatusBubbleViews::StatusView> timer_factory_;

  // Manager, owns us.
  StatusBubble* status_bubble_;

  // Handle to the HWND that contains us.
  views::WidgetWin* popup_;

  // The currently-displayed text.
  std::wstring text_;

  // Start and end opacities for the current transition - note that as a
  // fade-in can easily turn into a fade out, opacity_start_ is sometimes
  // a value between 0 and 1.
  double opacity_start_;
  double opacity_end_;
};

void StatusBubbleViews::StatusView::SetText(const std::wstring& text) {
  if (text.empty()) {
    // The string was empty.
    StartHiding();
  } else {
    // We want to show the string.
    text_ = text;
    StartShowing();
  }

  SchedulePaint();
}

void StatusBubbleViews::StatusView::Show() {
  Stop();
  CancelTimer();
  SetOpacity(1.0);
  stage_ = BUBBLE_SHOWN;
  PaintNow();
}

void StatusBubbleViews::StatusView::Hide() {
  Stop();
  CancelTimer();
  SetOpacity(0.0);
  text_.clear();
  stage_ = BUBBLE_HIDDEN;
}

void StatusBubbleViews::StatusView::StartTimer(int time) {
  if (!timer_factory_.empty())
    timer_factory_.RevokeAll();

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      timer_factory_.NewRunnableMethod(&StatusBubbleViews::StatusView::OnTimer),
      time);
}

void StatusBubbleViews::StatusView::OnTimer() {
  if (stage_ == BUBBLE_HIDING_TIMER) {
    stage_ = BUBBLE_HIDING_FADE;
    StartFade(1.0, 0.0, kHideFadeDurationMS);
  } else if (stage_ == BUBBLE_SHOWING_TIMER) {
    stage_ = BUBBLE_SHOWING_FADE;
    StartFade(0.0, 1.0, kShowFadeDurationMS);
  }
}

void StatusBubbleViews::StatusView::CancelTimer() {
  if (!timer_factory_.empty()) {
    timer_factory_.RevokeAll();
  }
}

void StatusBubbleViews::StatusView::RestartTimer(int delay) {
  CancelTimer();
  StartTimer(delay);
}

void StatusBubbleViews::StatusView::ResetTimer() {
  if (stage_ == BUBBLE_SHOWING_TIMER) {
    // We hadn't yet begun showing anything when we received a new request
    // for something to show, so we start from scratch.
    RestartTimer(kShowDelay);
  }
}

void StatusBubbleViews::StatusView::StartFade(double start,
                                              double end,
                                              int duration) {
  opacity_start_ = start;
  opacity_end_ = end;

  // This will also reset the currently-occurring animation.
  SetDuration(duration);
  Start();
}

void StatusBubbleViews::StatusView::StartHiding() {
  if (stage_ == BUBBLE_SHOWN) {
    stage_ = BUBBLE_HIDING_TIMER;
    StartTimer(kHideDelay);
  } else if (stage_ == BUBBLE_SHOWING_TIMER) {
    stage_ = BUBBLE_HIDDEN;
    CancelTimer();
  } else if (stage_ == BUBBLE_SHOWING_FADE) {
    stage_ = BUBBLE_HIDING_FADE;
    // Figure out where we are in the current fade.
    double current_opacity = GetCurrentOpacity();

    // Start a fade in the opposite direction.
    StartFade(current_opacity, 0.0,
              static_cast<int>(kHideFadeDurationMS * current_opacity));
  }
}

void StatusBubbleViews::StatusView::StartShowing() {
  if (stage_ == BUBBLE_HIDDEN) {
    stage_ = BUBBLE_SHOWING_TIMER;
    StartTimer(kShowDelay);
  } else if (stage_ == BUBBLE_HIDING_TIMER) {
    stage_ = BUBBLE_SHOWN;
    CancelTimer();
  } else if (stage_ == BUBBLE_HIDING_FADE) {
    // We're partway through a fade.
    stage_ = BUBBLE_SHOWING_FADE;

    // Figure out where we are in the current fade.
    double current_opacity = GetCurrentOpacity();

    // Start a fade in the opposite direction.
    StartFade(current_opacity, 1.0,
              static_cast<int>(kShowFadeDurationMS * current_opacity));
  } else if (stage_ == BUBBLE_SHOWING_TIMER) {
    // We hadn't yet begun showing anything when we received a new request
    // for something to show, so we start from scratch.
    ResetTimer();
  }
}

// Animation functions.
double StatusBubbleViews::StatusView::GetCurrentOpacity() {
  return opacity_start_ + (opacity_end_ - opacity_start_) *
         Animation::GetCurrentValue();
}

void StatusBubbleViews::StatusView::SetOpacity(double opacity) {
  popup_->SetLayeredAlpha(static_cast<BYTE>(opacity * 255));
  SchedulePaint();
}

void StatusBubbleViews::StatusView::AnimateToState(double state) {
  SetOpacity(GetCurrentOpacity());
}

void StatusBubbleViews::StatusView::AnimationEnded(
    const Animation* animation) {
  SetOpacity(opacity_end_);

  if (stage_ == BUBBLE_HIDING_FADE) {
    stage_ = BUBBLE_HIDDEN;
  } else if (stage_ == BUBBLE_SHOWING_FADE) {
    stage_ = BUBBLE_SHOWN;
  }
}

void StatusBubbleViews::StatusView::SetStyle(BubbleStyle style) {
  if (style_ != style) {
    style_ = style;
    SchedulePaint();
  }
}

void StatusBubbleViews::StatusView::Paint(ChromeCanvas* canvas) {
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setColor(kBubbleColor);

  RECT parent_rect;
  ::GetWindowRect(popup_->GetHWND(), &parent_rect);

  // Draw our background.
  SkRect rect;
  int width = parent_rect.right - parent_rect.left;
  int height = parent_rect.bottom - parent_rect.top;

  // Figure out how to round the bubble's four corners.
  SkScalar rad[8];

  // Top Edges - if the bubble is in its bottom position (sticking downwards),
  // then we square the top edges. Otherwise, we square the edges based on the
  // position of the bubble within the window (the bubble is positioned in the
  // southeast corner in RTL and in the southwest corner in LTR).
  if (style_ == STYLE_BOTTOM) {
    // Top Left corner.
    rad[0] = 0;
    rad[1] = 0;

    // Top Right corner.
    rad[2] = 0;
    rad[3] = 0;
  } else {
    if (UILayoutIsRightToLeft()) {
      // Top Left corner.
      rad[0] = SkIntToScalar(kBubbleCornerRadius);
      rad[1] = SkIntToScalar(kBubbleCornerRadius);

      // Top Right corner.
      rad[2] = 0;
      rad[3] = 0;
    } else {
      // Top Left corner.
      rad[0] = 0;
      rad[1] = 0;

      // Top Right corner.
      rad[2] = SkIntToScalar(kBubbleCornerRadius);
      rad[3] = SkIntToScalar(kBubbleCornerRadius);
    }
  }

  // Bottom edges - square these off if the bubble is in its standard position
  // (sticking upward).
  if (style_ == STYLE_STANDARD) {
    // Bottom Right Corner.
    rad[4] = 0;
    rad[5] = 0;

    // Bottom Left Corner.
    rad[6] = 0;
    rad[7] = 0;
  } else {
    // Bottom Right Corner.
    rad[4] = SkIntToScalar(kBubbleCornerRadius);
    rad[5] = SkIntToScalar(kBubbleCornerRadius);

    // Bottom Left Corner.
    rad[6] = SkIntToScalar(kBubbleCornerRadius);
    rad[7] = SkIntToScalar(kBubbleCornerRadius);
  }

  // Draw the bubble's shadow.
  SkPaint shadow_paint;
  shadow_paint.setFlags(SkPaint::kAntiAlias_Flag);
  shadow_paint.setColor(kShadowColor);

  rect.set(0, 0,
           SkIntToScalar(width),
           SkIntToScalar(height));

  SkPath shadow_path;
  shadow_path.addRoundRect(rect, rad, SkPath::kCW_Direction);
  canvas->drawPath(shadow_path, shadow_paint);

  // Draw the bubble.
  SkPath path;
  rect.set(SkIntToScalar(kShadowSize),
           SkIntToScalar(kShadowSize),
           SkIntToScalar(width - kShadowSize),
           SkIntToScalar(height - kShadowSize));

  path.addRoundRect(rect, rad, SkPath::kCW_Direction);
  canvas->drawPath(path, paint);


  int text_width = std::min(static_cast<int>(parent_rect.right -
                                parent_rect.left - kTextPositionX -
                                kTextPadding),
                            static_cast<int>(views::Label::GetFont()
                                .GetStringWidth(text_)));

  // Draw highlight text and then the text body. In order to make sure the text
  // is aligned to the right on RTL UIs, we mirror the text bounds if the
  // locale is RTL.
  gfx::Rect body_bounds(kTextPositionX,
                        kTextPositionY,
                        text_width,
                        parent_rect.bottom - parent_rect.top);
  body_bounds.set_x(MirroredLeftPointForRect(body_bounds));
  canvas->DrawStringInt(text_,
                        views::Label::GetFont(),
                        kTextHighlightColor,
                        body_bounds.x() + 1,
                        body_bounds.y() + 1,
                        body_bounds.width(),
                        body_bounds.height());

  canvas->DrawStringInt(text_,
                        views::Label::GetFont(),
                        kTextColor,
                        body_bounds.x(),
                        body_bounds.y(),
                        body_bounds.width(),
                        body_bounds.height());
}

// StatusBubble ---------------------------------------------------------------

StatusBubbleViews::StatusBubbleViews(views::Widget* frame)
    : popup_(NULL),
      frame_(frame),
      view_(NULL),
      opacity_(0),
      position_(0, 0),
      size_(0, 0),
      offset_(0) {
}

StatusBubbleViews::~StatusBubbleViews() {
  if (popup_.get())
    popup_->CloseNow();

  position_ = NULL;
  size_ = NULL;
}

void StatusBubbleViews::Init() {
  if (!popup_.get()) {
    popup_.reset(new views::WidgetWin());
    popup_->set_delete_on_destroy(false);

    if (!view_)
      view_ = new StatusView(this, popup_.get());

    gfx::Rect rc(0, 0, 0, 0);

    popup_->set_window_style(WS_POPUP);
    popup_->set_window_ex_style(WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                                WS_EX_TRANSPARENT |
                                l10n_util::GetExtendedTooltipStyles());
    popup_->SetLayeredAlpha(0x00);
    popup_->Init(frame_->GetHWND(), rc, false);
    popup_->SetContentsView(view_);
    Reposition();
    popup_->Show();
  }
}

void StatusBubbleViews::SetStatus(const std::wstring& status_text) {
  if (status_text_ == status_text)
    return;

  Init();
  status_text_ = status_text;
  if (!status_text_.empty()) {
    view_->SetText(status_text);
    view_->Show();
  } else if (!url_text_.empty()) {
    view_->SetText(url_text_);
  } else {
    view_->SetText(std::wstring());
  }
}

void StatusBubbleViews::SetURL(const GURL& url, const std::wstring& languages) {
  Init();

  // If we want to clear a displayed URL but there is a status still to
  // display, display that status instead.
  if (url.is_empty() && !status_text_.empty()) {
    url_text_ = std::wstring();
    view_->SetText(status_text_);
    return;
  }

  // Set Elided Text corresponding to the GURL object.
  RECT parent_rect;
  ::GetWindowRect(popup_->GetHWND(), &parent_rect);
  int text_width = static_cast<int>(parent_rect.right -
                                    parent_rect.left - kTextPositionX -
                                    kTextPadding);
  url_text_ = gfx::ElideUrl(url, view_->Label::GetFont(), text_width,
                            languages);

  // An URL is always treated as a left-to-right string. On right-to-left UIs
  // we need to explicitly mark the URL as LTR to make sure it is displayed
  // correctly.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT &&
      !url_text_.empty())
    l10n_util::WrapStringWithLTRFormatting(&url_text_);
  view_->SetText(url_text_);
}

void StatusBubbleViews::Hide() {
  status_text_ = std::wstring();
  url_text_ = std::wstring();
  if (view_)
    view_->Hide();
}

void StatusBubbleViews::MouseMoved() {
  if (view_) {
    view_->ResetTimer();

    if (view_->GetState() != StatusView::BUBBLE_HIDDEN &&
        view_->GetState() != StatusView::BUBBLE_HIDING_FADE &&
        view_->GetState() != StatusView::BUBBLE_HIDING_TIMER) {
      AvoidMouse();
    }
  }
}

void StatusBubbleViews::AvoidMouse() {
  // Our status bubble is located in screen coordinates, so we should get
  // those rather than attempting to reverse decode the web contents
  // coordinates.
  CPoint cursor_location;
  GetCursorPos(&cursor_location);

  // Get the position of the frame.
  gfx::Point top_left;
  views::View::ConvertPointToScreen(frame_->GetRootView(), &top_left);

  // Get the cursor position relative to the popup.
  cursor_location.x -= (top_left.x() + position_.x);
  cursor_location.y -= (top_left.y() + position_.y);

  // If the mouse is in a position where we think it would move the
  // status bubble, figure out where and how the bubble should be moved.
  if (cursor_location.y > -kMousePadding &&
      cursor_location.x < size_.cx + kMousePadding) {
    int offset = kMousePadding + cursor_location.y;

    // Make the movement non-linear.
    offset = offset * offset / kMousePadding;

    // When the mouse is entering from the right, we want the offset to be
    // scaled by how horizontally far away the cursor is from the bubble.
    if (cursor_location.x > size_.cx) {
      offset = static_cast<int>(static_cast<float>(offset) * (
          static_cast<float>(kMousePadding -
              (cursor_location.x - size_.cx)) /
          static_cast<float>(kMousePadding)));
    }

    // Cap the offset and change the visual presentation of the bubble
    // depending on where it ends up (so that rounded corners square off
    // and mate to the edges of the tab content).
    if (offset >= size_.cy - kShadowSize * 2) {
      offset = size_.cy - kShadowSize * 2;
      view_->SetStyle(StatusView::STYLE_BOTTOM);
    } else if (offset > kBubbleCornerRadius / 2 - kShadowSize) {
      view_->SetStyle(StatusView::STYLE_FLOATING);
    } else {
      view_->SetStyle(StatusView::STYLE_STANDARD);
    }

    offset_ = offset;
    popup_->MoveWindow(top_left.x() + position_.x,
                       top_left.y() + position_.y + offset_,
                       size_.cx,
                       size_.cy);
  } else if (offset_ != 0) {
    offset_ = 0;
    view_->SetStyle(StatusView::STYLE_STANDARD);
    popup_->MoveWindow(top_left.x() + position_.x,
                       top_left.y() + position_.y,
                       size_.cx,
                       size_.cy);
  }
}

void StatusBubbleViews::Reposition() {
  if (popup_.get()) {
    gfx::Point top_left;
    views::View::ConvertPointToScreen(frame_->GetRootView(), &top_left);

    popup_->MoveWindow(top_left.x() + position_.x,
                       top_left.y() + position_.y,
                       size_.cx,
                       size_.cy);
  }
}

void StatusBubbleViews::SetBounds(int x, int y, int w, int h) {
  // If the UI layout is RTL, we need to mirror the position of the bubble
  // relative to the parent.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    gfx::Rect frame_bounds;
    frame_->GetBounds(&frame_bounds, false);
    int mirrored_x = frame_bounds.width() - x - w;
    position_.SetPoint(mirrored_x, y);
  } else {
    position_.SetPoint(x, y);
  }

  size_.SetSize(w, h);
  Reposition();
}
