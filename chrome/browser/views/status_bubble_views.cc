// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/status_bubble_views.h"

#include <algorithm>

#include "app/gfx/canvas.h"
#include "app/gfx/text_elider.h"
#include "app/l10n_util.h"
#if defined(OS_WIN)
#include "app/l10n_util_win.h"
#endif
#include "app/animation.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser_theme_provider.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "views/controls/label.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#endif

// The alpha and color of the bubble's shadow.
static const SkColor kShadowColor = SkColorSetARGB(30, 0, 0, 0);

// The roundedness of the edges of our bubble.
static const int kBubbleCornerRadius = 4;

// How close the mouse can get to the infobubble before it starts sliding
// off-screen.
static const int kMousePadding = 20;

// The color of the text
static const SkColor kTextColor = SkColorSetRGB(100, 100, 100);

// The color of the highlight text
static const SkColor kTextHighlightColor = SkColorSetRGB(242, 250, 255);

// The horizontal offset of the text within the status bubble, not including the
// outer shadow ring.
static const int kTextPositionX = 3;

// The minimum horizontal space between the (right) end of the text and the edge
// of the status bubble, not including the outer shadow ring, or a 1 px gap we
// leave so we can shit all the text by 1 px to produce a "highlight" effect.
static const int kTextHorizPadding = 1;

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
  StatusView(StatusBubble* status_bubble, views::Widget* popup,
             ThemeProvider* theme_provider)
      : Animation(kFramerate, this),
        stage_(BUBBLE_HIDDEN),
        style_(STYLE_STANDARD),
        timer_factory_(this),
        status_bubble_(status_bubble),
        popup_(popup),
        opacity_start_(0),
        opacity_end_(0),
        theme_provider_(theme_provider) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    gfx::Font font(rb.GetFont(ResourceBundle::BaseFont));
    SetFont(font);
  }

  virtual ~StatusView() {
    Stop();
    CancelTimer();
  }

  // The bubble can be in one of many stages:
  enum BubbleStage {
    BUBBLE_HIDDEN,         // Entirely BUBBLE_HIDDEN.
    BUBBLE_HIDING_FADE,    // In a fade-out transition.
    BUBBLE_HIDING_TIMER,   // Waiting before a fade-out.
    BUBBLE_SHOWING_TIMER,  // Waiting before a fade-in.
    BUBBLE_SHOWING_FADE,   // In a fade-in transition.
    BUBBLE_SHOWN           // Fully visible.
  };

  enum BubbleStyle {
    STYLE_BOTTOM,
    STYLE_FLOATING,
    STYLE_STANDARD,
    STYLE_STANDARD_RIGHT
  };

  // Set the bubble text to a certain value, hides the bubble if text is
  // an empty string.
  void SetText(const std::wstring& text);

  BubbleStage GetState() const { return stage_; }

  void SetStyle(BubbleStyle style);

  BubbleStyle GetStyle() const { return style_; }

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

  virtual void Paint(gfx::Canvas* canvas);

  BubbleStage stage_;
  BubbleStyle style_;

  ScopedRunnableMethodFactory<StatusBubbleViews::StatusView> timer_factory_;

  // Manager, owns us.
  StatusBubble* status_bubble_;

  // Handle to the widget that contains us.
  views::Widget* popup_;

  // The currently-displayed text.
  std::wstring text_;

  // Start and end opacities for the current transition - note that as a
  // fade-in can easily turn into a fade out, opacity_start_ is sometimes
  // a value between 0 and 1.
  double opacity_start_;
  double opacity_end_;

  // Holds the theme provider of the frame that created us.
  ThemeProvider* theme_provider_;
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
  popup_->Show();
  stage_ = BUBBLE_SHOWN;
  PaintNow();
}

void StatusBubbleViews::StatusView::Hide() {
  Stop();
  CancelTimer();
  SetOpacity(0.0);
  text_.clear();
  popup_->Hide();
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
  if (!timer_factory_.empty())
    timer_factory_.RevokeAll();
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
    popup_->Show();
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
  popup_->SetOpacity(static_cast<unsigned char>(opacity * 255));
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
    popup_->Hide();
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

void StatusBubbleViews::StatusView::Paint(gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setColor(
      theme_provider_->GetColor(BrowserThemeProvider::COLOR_TOOLBAR));

  gfx::Rect popup_bounds;
  popup_->GetBounds(&popup_bounds, true);

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
    if (UILayoutIsRightToLeft() ^ (style_ == STYLE_STANDARD_RIGHT)) {
      // The text is RtL or the bubble is on the right side (but not both).

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
  if (style_ == STYLE_STANDARD || style_ == STYLE_STANDARD_RIGHT) {
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
  int width = popup_bounds.width();
  int height = popup_bounds.height();
  SkRect rect;
  rect.set(0, 0,
           SkIntToScalar(width),
           SkIntToScalar(height));
  SkPath shadow_path;
  shadow_path.addRoundRect(rect, rad, SkPath::kCW_Direction);
  SkPaint shadow_paint;
  shadow_paint.setFlags(SkPaint::kAntiAlias_Flag);
  shadow_paint.setColor(kShadowColor);
  canvas->drawPath(shadow_path, shadow_paint);

  // Draw the bubble.
  rect.set(SkIntToScalar(kShadowThickness),
           SkIntToScalar(kShadowThickness),
           SkIntToScalar(width - kShadowThickness),
           SkIntToScalar(height - kShadowThickness));
  SkPath path;
  path.addRoundRect(rect, rad, SkPath::kCW_Direction);
  canvas->drawPath(path, paint);

  // Draw highlight text and then the text body. In order to make sure the text
  // is aligned to the right on RTL UIs, we mirror the text bounds if the
  // locale is RTL.
  // The "- 1" on the end of the width and height ensures that when we add one
  // to x() and y() for the highlight text, we still won't overlap the shadow.
  int text_width = std::min(views::Label::GetFont().GetStringWidth(text_),
      width - (kShadowThickness * 2) - kTextPositionX - kTextHorizPadding - 1);
  int text_height = height - (kShadowThickness * 2) - 1;
  gfx::Rect body_bounds(kShadowThickness + kTextPositionX,
                        kShadowThickness,
                        std::max(0, text_width),
                        std::max(0, text_height));
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

const int StatusBubbleViews::kShadowThickness = 1;

StatusBubbleViews::StatusBubbleViews(views::Widget* frame)
    : offset_(0),
      popup_(NULL),
      opacity_(0),
      frame_(frame),
      view_(NULL),
      download_shelf_is_visible_(false) {
}

StatusBubbleViews::~StatusBubbleViews() {
  if (popup_.get())
    popup_->CloseNow();
}

void StatusBubbleViews::Init() {
  if (!popup_.get()) {
#if defined(OS_WIN)
    views::WidgetWin* popup = new views::WidgetWin;
    popup->set_delete_on_destroy(false);

    if (!view_)
      view_ = new StatusView(this, popup, frame_->GetThemeProvider());

    popup->set_window_style(WS_POPUP);
    popup->set_window_ex_style(WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                               WS_EX_TRANSPARENT |
                               l10n_util::GetExtendedTooltipStyles());
    popup->SetOpacity(0x00);
    popup->Init(frame_->GetNativeView(), gfx::Rect());
    popup->SetContentsView(view_);
    Reposition();
    popup->Show();
    popup_.reset(popup);
#else
    NOTIMPLEMENTED();
#endif
  }
}

void StatusBubbleViews::Reposition() {
  if (popup_.get()) {
    gfx::Point top_left;
    views::View::ConvertPointToScreen(frame_->GetRootView(), &top_left);

    popup_->SetBounds(gfx::Rect(top_left.x() + position_.x(),
                                top_left.y() + position_.y(),
                                size_.width(), size_.height()));
  }
}

gfx::Size StatusBubbleViews::GetPreferredSize() {
  return gfx::Size(0, ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).height() + kTotalVerticalPadding);
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
  gfx::Rect popup_bounds;
  popup_->GetBounds(&popup_bounds, true);
  int text_width = static_cast<int>(popup_bounds.width() -
      (kShadowThickness * 2) - kTextPositionX - kTextHorizPadding - 1);
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

void StatusBubbleViews::UpdateDownloadShelfVisibility(bool visible) {
  download_shelf_is_visible_ = visible;
}

void StatusBubbleViews::AvoidMouse() {
  // Our status bubble is located in screen coordinates, so we should get
  // those rather than attempting to reverse decode the web contents
  // coordinates.
  gfx::Point cursor_location;
#if defined(OS_WIN)
  POINT tmp = { 0, 0 };
  GetCursorPos(&tmp);
  cursor_location = tmp;
#else
  NOTIMPLEMENTED();
#endif

  // Get the position of the frame.
  gfx::Point top_left;
  views::RootView* root = frame_->GetRootView();
  views::View::ConvertPointToScreen(root, &top_left);
  int window_width = root->GetLocalBounds(true).width();  // border included.

  // Get the cursor position relative to the popup.
  if (view_->UILayoutIsRightToLeft()) {
    int top_right_x = top_left.x() + window_width;
    cursor_location.set_x(top_right_x - cursor_location.x());
  } else {
    cursor_location.set_x(cursor_location.x() - (top_left.x() + position_.x()));
  }
  cursor_location.set_y(cursor_location.y() - (top_left.y() + position_.y()));

  // If the mouse is in a position where we think it would move the
  // status bubble, figure out where and how the bubble should be moved.
  if (cursor_location.y() > -kMousePadding &&
      cursor_location.x() < size_.width() + kMousePadding) {
    int offset = kMousePadding + cursor_location.y();

    // Make the movement non-linear.
    offset = offset * offset / kMousePadding;

    // When the mouse is entering from the right, we want the offset to be
    // scaled by how horizontally far away the cursor is from the bubble.
    if (cursor_location.x() > size_.width()) {
      offset = static_cast<int>(static_cast<float>(offset) * (
          static_cast<float>(kMousePadding -
              (cursor_location.x() - size_.width())) /
          static_cast<float>(kMousePadding)));
    }

    // Cap the offset and change the visual presentation of the bubble
    // depending on where it ends up (so that rounded corners square off
    // and mate to the edges of the tab content).
    if (offset >= size_.height() - kShadowThickness * 2) {
      offset = size_.height() - kShadowThickness * 2;
      view_->SetStyle(StatusView::STYLE_BOTTOM);
    } else if (offset > kBubbleCornerRadius / 2 - kShadowThickness) {
      view_->SetStyle(StatusView::STYLE_FLOATING);
    } else {
      view_->SetStyle(StatusView::STYLE_STANDARD);
    }

    // Check if the bubble sticks out from the monitor or will obscure
    // download shelf.
#if defined(OS_WIN)
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfo(MonitorFromWindow(frame_->GetNativeView(),
                                     MONITOR_DEFAULTTONEAREST), &monitor_info);
    gfx::Rect monitor_rect(monitor_info.rcWork);
#else
    gfx::Rect monitor_rect;
    NOTIMPLEMENTED();
#endif
    const int bubble_bottom_y = top_left.y() + position_.y() + size_.height();

    if (bubble_bottom_y + offset > monitor_rect.height() ||
        (download_shelf_is_visible_ &&
         view_->GetStyle() == StatusView::STYLE_FLOATING)) {
      // The offset is still too large. Move the bubble to the right and reset
      // Y offset_ to zero.
      view_->SetStyle(StatusView::STYLE_STANDARD_RIGHT);
      offset_ = 0;

      // Substract border width + bubble width.
      int right_position_x = window_width - (position_.x() + size_.width());
      popup_->SetBounds(gfx::Rect(top_left.x() + right_position_x,
                                  top_left.y() + position_.y(),
                                  size_.width(), size_.height()));
    } else {
      offset_ = offset;
      popup_->SetBounds(gfx::Rect(top_left.x() + position_.x(),
                                  top_left.y() + position_.y() + offset_,
                                  size_.width(), size_.height()));
    }
  } else if (offset_ != 0 ||
      view_->GetStyle() == StatusView::STYLE_STANDARD_RIGHT) {
    offset_ = 0;
    view_->SetStyle(StatusView::STYLE_STANDARD);
    popup_->SetBounds(gfx::Rect(top_left.x() + position_.x(),
                                top_left.y() + position_.y(),
                                size_.width(), size_.height()));
  }
}
