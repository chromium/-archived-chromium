// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "chrome/browser/views/tabs/tab_renderer.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/widget.h"
#include "chrome/views/window.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"

static const int kLeftPadding = 16;
static const int kTopPadding = 6;
static const int kRightPadding = 15;
static const int kBottomPadding = 5;
static const int kFavIconTitleSpacing = 4;
static const int kTitleCloseButtonSpacing = 5;
static const int kStandardTitleWidth = 175;
static const int kCloseButtonVertFuzz = 0;
static const int kCloseButtonHorzFuzz = 5;
static const int kFaviconSize = 16;
static const int kSelectedTitleColor = SK_ColorBLACK;
static const int kUnselectedTitleColor = SkColorSetRGB(64, 64, 64);

// How long the hover state takes.
static const int kHoverDurationMs = 90;

// How long the pulse throb takes.
static const int kPulseDurationMs = 200;

// How opaque to make the hover state (out of 1).
static const double kHoverOpacity = 0.33;
static const double kHoverOpacityVista = 0.7;

// TODO(beng): (Cleanup) This stuff should move onto the class.
static ChromeFont title_font;
static int title_font_height = 0;
static SkBitmap* close_button_n = NULL;
static SkBitmap* close_button_h = NULL;
static SkBitmap* close_button_p = NULL;
static int close_button_height = 0;
static int close_button_width = 0;
static SkBitmap* waiting_animation_frames = NULL;
static SkBitmap* loading_animation_frames = NULL;
static SkBitmap* crashed_fav_icon = NULL;
static int loading_animation_frame_count = 0;
static int waiting_animation_frame_count = 0;
static int waiting_to_loading_frame_count_ratio = 0;
static SkBitmap* download_icon = NULL;
static int download_icon_width = 0;
static int download_icon_height = 0;

TabRenderer::TabImage TabRenderer::tab_active = {0};
TabRenderer::TabImage TabRenderer::tab_inactive = {0};
TabRenderer::TabImage TabRenderer::tab_inactive_otr = {0};
TabRenderer::TabImage TabRenderer::tab_hover = {0};

namespace {

void InitResources() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    title_font = rb.GetFont(ResourceBundle::BaseFont);
    title_font_height = title_font.height();

    close_button_n = rb.GetBitmapNamed(IDR_TAB_CLOSE);
    close_button_h = rb.GetBitmapNamed(IDR_TAB_CLOSE_H);
    close_button_p = rb.GetBitmapNamed(IDR_TAB_CLOSE_P);
    close_button_width = close_button_n->width();
    close_button_height = close_button_n->height();

    TabRenderer::LoadTabImages(win_util::ShouldUseVistaFrame());

    // The loading animation image is a strip of states. Each state must be
    // square, so the height must divide the width evenly.
    loading_animation_frames = rb.GetBitmapNamed(IDR_THROBBER);
    DCHECK(loading_animation_frames);
    DCHECK(loading_animation_frames->width() %
           loading_animation_frames->height() == 0);
    loading_animation_frame_count =
        loading_animation_frames->width() / loading_animation_frames->height();

    waiting_animation_frames = rb.GetBitmapNamed(IDR_THROBBER_WAITING);
    DCHECK(waiting_animation_frames);
    DCHECK(waiting_animation_frames->width() %
           waiting_animation_frames->height() == 0);
    waiting_animation_frame_count =
        waiting_animation_frames->width() / waiting_animation_frames->height();

    waiting_to_loading_frame_count_ratio =
        waiting_animation_frame_count / loading_animation_frame_count;
    // TODO(beng): eventually remove this when we have a proper themeing system.
    //             themes not supporting IDR_THROBBER_WAITING are causing this
    //             value to be 0 which causes DIV0 crashes. The value of 5
    //             matches the current bitmaps in our source.
    if (waiting_to_loading_frame_count_ratio == 0)
      waiting_to_loading_frame_count_ratio = 5;

    crashed_fav_icon = rb.GetBitmapNamed(IDR_SAD_FAVICON);

    download_icon = rb.GetBitmapNamed(IDR_DOWNLOAD_ICON);
    download_icon_width = download_icon->width();
    download_icon_height = download_icon->height();

    initialized = true;
  }
}

int GetContentHeight() {
  // The height of the content of the Tab is the largest of the favicon,
  // the title text and the close button graphic.
  int content_height = std::max(kFaviconSize, title_font_height);
  return std::max(content_height, close_button_height);
}

////////////////////////////////////////////////////////////////////////////////
// TabCloseButton
//
//  This is a Button subclass that causes middle clicks to be forwarded to the
//  parent View by explicitly not handling them in OnMousePressed.
class TabCloseButton : public views::ImageButton {
 public:
  explicit TabCloseButton(views::ButtonListener* listener)
      : views::ImageButton(listener) {
  }
  virtual ~TabCloseButton() {}

  virtual bool OnMousePressed(const views::MouseEvent& event) {
    bool handled = ImageButton::OnMousePressed(event);
    // Explicitly mark midle-mouse clicks as non-handled to ensure the tab
    // sees them.
    return event.IsOnlyMiddleMouseButton() ? false : handled;
  }

  // We need to let the parent know about mouse state so that it
  // can highlight itself appropriately. Note that Exit events
  // fire before Enter events, so this works.
  virtual void OnMouseEntered(const views::MouseEvent& event) {
    CustomButton::OnMouseEntered(event);
    GetParent()->OnMouseEntered(event);
  }

  virtual void OnMouseExited(const views::MouseEvent& event) {
    CustomButton::OnMouseExited(event);
    GetParent()->OnMouseExited(event);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(TabCloseButton);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FaviconCrashAnimation
//
//  A custom animation subclass to manage the favicon crash animation.
class TabRenderer::FavIconCrashAnimation : public Animation,
                                           public AnimationDelegate {
 public:
  explicit FavIconCrashAnimation(TabRenderer* target)
      : Animation(1000, 25, this),
        target_(target) {
  }
  virtual ~FavIconCrashAnimation() {}

  // Animation overrides:
  virtual void AnimateToState(double state) {
    const double kHidingOffset = 27;

    if (state < .5) {
      target_->SetFavIconHidingOffset(
          static_cast<int>(floor(kHidingOffset * 2.0 * state)));
    } else {
      target_->DisplayCrashedFavIcon();
      target_->SetFavIconHidingOffset(
          static_cast<int>(
              floor(kHidingOffset - ((state - .5) * 2.0 * kHidingOffset))));
    }
  }

  // AnimationDelegate overrides:
  virtual void AnimationCanceled(const Animation* animation) {
    target_->SetFavIconHidingOffset(0);
  }

 private:
  TabRenderer* target_;

  DISALLOW_EVIL_CONSTRUCTORS(FavIconCrashAnimation);
};

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, public:

TabRenderer::TabRenderer()
    : animation_state_(ANIMATION_NONE),
      animation_frame_(0),
      showing_icon_(false),
      showing_download_icon_(false),
      showing_close_button_(false),
      crash_animation_(NULL),
      fav_icon_hiding_offset_(0),
      should_display_crashed_favicon_(false) {
  InitResources();

  // Add the Close Button.
  close_button_ = new TabCloseButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL, close_button_n);
  close_button_->SetImage(views::CustomButton::BS_HOT, close_button_h);
  close_button_->SetImage(views::CustomButton::BS_PUSHED, close_button_p);
  AddChildView(close_button_);

  hover_animation_.reset(new SlideAnimation(this));
  hover_animation_->SetSlideDuration(kHoverDurationMs);

  pulse_animation_.reset(new ThrobAnimation(this));
  pulse_animation_->SetSlideDuration(kPulseDurationMs);
}

TabRenderer::~TabRenderer() {
  delete crash_animation_;
}

void TabRenderer::UpdateData(TabContents* contents) {
  DCHECK(contents);
  data_.favicon = contents->GetFavIcon();
  data_.title = UTF16ToWideHack(contents->GetTitle());
  data_.loading = contents->is_loading();
  data_.off_the_record = contents->profile()->IsOffTheRecord();
  data_.show_icon = contents->ShouldDisplayFavIcon();
  data_.show_download_icon = contents->IsDownloadShelfVisible();
  data_.crashed = contents->is_crashed();
}

void TabRenderer::UpdateFromModel() {
  // Force a layout, since the tab may have grown a favicon.
  Layout();
  SchedulePaint();

  if (data_.crashed) {
    if (!should_display_crashed_favicon_ && !IsPerformingCrashAnimation())
      StartCrashAnimation();
  } else {
    if (IsPerformingCrashAnimation())
      StopCrashAnimation();
    ResetCrashedFavIcon();
  }
}

bool TabRenderer::IsSelected() const {
  return true;
}

void TabRenderer::ValidateLoadingAnimation(AnimationState animation_state) {
  if (animation_state_ != animation_state) {
    // The waiting animation is the reverse of the loading animation, but at a
    // different rate - the following reverses and scales the animation_frame_
    // so that the frame is at an equivalent position when going from one
    // animation to the other.
    if (animation_state_ == ANIMATION_WAITING &&
        animation_state == ANIMATION_LOADING) {
      animation_frame_ = loading_animation_frame_count -
          (animation_frame_ / waiting_to_loading_frame_count_ratio);
    }
    animation_state_ = animation_state;
  }

  if (animation_state_ != ANIMATION_NONE) {
    animation_frame_ = ++animation_frame_ %
                       ((animation_state_ == ANIMATION_WAITING) ?
                         waiting_animation_frame_count :
                         loading_animation_frame_count);
  } else {
    animation_frame_ = 0;
  }

  SchedulePaint();
}

void TabRenderer::StartPulse() {
  pulse_animation_->Reset();
  pulse_animation_->StartThrobbing(std::numeric_limits<int>::max());
}

void TabRenderer::StopPulse() {
  if (pulse_animation_->IsAnimating())
    pulse_animation_->Stop();
}

// static
gfx::Size TabRenderer::GetMinimumUnselectedSize() {
  InitResources();

  gfx::Size minimum_size;
  minimum_size.set_width(kLeftPadding + kRightPadding);
  // Since we use bitmap images, the real minimum height of the image is
  // defined most accurately by the height of the end cap images.
  minimum_size.set_height(tab_active.image_l->height());
  return minimum_size;
}

// static
gfx::Size TabRenderer::GetMinimumSelectedSize() {
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  minimum_size.set_width(kLeftPadding + kFaviconSize + kRightPadding);
  return minimum_size;
}

// static
gfx::Size TabRenderer::GetStandardSize() {
  gfx::Size standard_size = GetMinimumUnselectedSize();
  standard_size.set_width(
      standard_size.width() + kFavIconTitleSpacing + kStandardTitleWidth);
  return standard_size;
}

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, protected:

std::wstring TabRenderer::GetTitle() const {
  return data_.title;
}

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, views::View overrides:

void TabRenderer::Paint(ChromeCanvas* canvas) {
  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (width() < GetMinimumUnselectedSize().width())
    return;

  // See if the model changes whether the icons should be painted.
  const bool show_icon = ShouldShowIcon();
  const bool show_download_icon = data_.show_download_icon;
  const bool show_close_button = ShouldShowCloseBox();
  if (show_icon != showing_icon_ ||
      show_download_icon != showing_download_icon_ ||
      show_close_button != showing_close_button_)
    Layout();

  PaintTabBackground(canvas);

  // Paint the loading animation if the page is currently loading, otherwise
  // show the page's favicon.
  if (show_icon) {
    if (animation_state_ != ANIMATION_NONE) {
      PaintLoadingAnimation(canvas);
    } else {
      canvas->save();
      canvas->ClipRectInt(0, 0, width(), height() - 4);
      if (should_display_crashed_favicon_) {
        canvas->DrawBitmapInt(*crashed_fav_icon, 0, 0,
                              crashed_fav_icon->width(),
                              crashed_fav_icon->height(),
                              favicon_bounds_.x(),
                              favicon_bounds_.y() + fav_icon_hiding_offset_,
                              kFaviconSize, kFaviconSize,
                              true);
      } else {
        if (!data_.favicon.isNull()) {
          // TODO(pkasting): Use code in tab_icon_view.cc:PaintIcon() (or switch
          // to using that class to render the favicon).
          canvas->DrawBitmapInt(data_.favicon, 0, 0,
                                data_.favicon.width(),
                                data_.favicon.height(),
                                favicon_bounds_.x(),
                                favicon_bounds_.y() + fav_icon_hiding_offset_,
                                kFaviconSize, kFaviconSize,
                                true);
        }
      }
      canvas->restore();
    }
  }

  if (show_download_icon) {
    canvas->DrawBitmapInt(*download_icon,
                          download_icon_bounds_.x(), download_icon_bounds_.y());
  }

  // Paint the Title.
  std::wstring title = data_.title;
  if (title.empty()) {
    if (data_.loading) {
      title = l10n_util::GetString(IDS_TAB_LOADING_TITLE);
    } else {
      title = l10n_util::GetString(IDS_TAB_UNTITLED_TITLE);
    }
  } else {
    Browser::FormatTitleForDisplay(&title);
  }

  SkColor title_color = IsSelected() ? kSelectedTitleColor
                                     : kUnselectedTitleColor;
  canvas->DrawStringInt(title, title_font, title_color, title_bounds_.x(),
                        title_bounds_.y(), title_bounds_.width(),
                        title_bounds_.height());
}

void TabRenderer::Layout() {
  gfx::Rect lb = GetLocalBounds(false);
  if (lb.IsEmpty())
    return;
  lb.Inset(kLeftPadding, kTopPadding, kRightPadding, kBottomPadding);

  // First of all, figure out who is tallest.
  int content_height = GetContentHeight();

  // Size the Favicon.
  showing_icon_ = ShouldShowIcon();
  if (showing_icon_) {
    int favicon_top = kTopPadding + (content_height - kFaviconSize) / 2;
    favicon_bounds_.SetRect(lb.x(), favicon_top, kFaviconSize, kFaviconSize);
  } else {
    favicon_bounds_.SetRect(lb.x(), lb.y(), 0, 0);
  }

  // Size the download icon.
  showing_download_icon_ = data_.show_download_icon;
  if (showing_download_icon_) {
    int icon_top = kTopPadding + (content_height - download_icon_height) / 2;
    download_icon_bounds_.SetRect(lb.width() - download_icon_width, icon_top,
                                  download_icon_width, download_icon_height);
  }

  // Size the Close button.
  showing_close_button_ = ShouldShowCloseBox();
  if (showing_close_button_) {
    int close_button_top =
        kTopPadding + kCloseButtonVertFuzz +
        (content_height - close_button_height) / 2;
    // If the ratio of the close button size to tab width exceeds the maximum.
    close_button_->SetBounds(lb.width() + kCloseButtonHorzFuzz,
                             close_button_top, close_button_width,
                             close_button_height);
    close_button_->SetVisible(true);
  } else {
    close_button_->SetBounds(0, 0, 0, 0);
    close_button_->SetVisible(false);
  }

  // Size the Title text to fill the remaining space.
  int title_left = favicon_bounds_.right() + kFavIconTitleSpacing;
  int title_top = kTopPadding + (content_height - title_font_height) / 2;

  // If the user has big fonts, the title will appear rendered too far down on
  // the y-axis if we use the regular top padding, so we need to adjust it so
  // that the text appears centered.
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  int text_height = title_top + title_font_height + kBottomPadding;
  if (text_height > minimum_size.height())
    title_top -= (text_height - minimum_size.height()) / 2;

  int title_width;
  if (close_button_->IsVisible()) {
    title_width = std::max(close_button_->x() -
                           kTitleCloseButtonSpacing - title_left, 0);
  } else {
    title_width = std::max(lb.width() - title_left, 0);
  }
  if (data_.show_download_icon)
    title_width = std::max(title_width - download_icon_width, 0);
  title_bounds_.SetRect(title_left, title_top, title_width, title_font_height);

  // Certain UI elements within the Tab (the favicon, the download icon, etc.)
  // are not represented as child Views (which is the preferred method).
  // Instead, these UI elements are drawn directly on the canvas from within
  // Tab::Paint(). The Tab's child Views (for example, the Tab's close button
  // which is a views::Button instance) are automatically mirrored by the
  // mirroring infrastructure in views. The elements Tab draws directly
  // on the canvas need to be manually mirrored if the View's layout is
  // right-to-left.
  favicon_bounds_.set_x(MirroredLeftPointForRect(favicon_bounds_));
  title_bounds_.set_x(MirroredLeftPointForRect(title_bounds_));
  download_icon_bounds_.set_x(MirroredLeftPointForRect(download_icon_bounds_));
}

void TabRenderer::OnMouseEntered(const views::MouseEvent& e) {
  hover_animation_->SetTweenType(SlideAnimation::EASE_OUT);
  hover_animation_->Show();
}

void TabRenderer::OnMouseExited(const views::MouseEvent& e) {
  hover_animation_->SetTweenType(SlideAnimation::EASE_IN);
  hover_animation_->Hide();
}

void TabRenderer::ThemeChanged() {
  if (GetWidget() && GetWidget()->AsWindow())
    LoadTabImages(GetWidget()->AsWindow()->GetNonClientView()->UseNativeFrame());
  View::ThemeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// TabRenderer, AnimationDelegate implementation:

void TabRenderer::AnimationProgressed(const Animation* animation) {
  SchedulePaint();
}

void TabRenderer::AnimationCanceled(const Animation* animation) {
  AnimationEnded(animation);
}

void TabRenderer::AnimationEnded(const Animation* animation) {
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, private

void TabRenderer::PaintTabBackground(ChromeCanvas* canvas) {
  if (IsSelected()) {
    // Sometimes detaching a tab quickly can result in the model reporting it
    // as not being selected, so is_drag_clone_ ensures that we always paint
    // the active representation for the dragged tab.
    PaintActiveTabBackground(canvas);
  } else {
    // Draw our hover state.
    Animation* animation = hover_animation_.get();
    if (pulse_animation_->IsAnimating())
      animation = pulse_animation_.get();
    if (animation->GetCurrentValue() > 0) {
      PaintHoverTabBackground(canvas, animation->GetCurrentValue() *
          (GetWidget()->AsWindow()->GetNonClientView()->UseNativeFrame() ?
          kHoverOpacityVista : kHoverOpacity));
    } else {
      PaintInactiveTabBackground(canvas);
    }
  }
}

void TabRenderer::PaintInactiveTabBackground(ChromeCanvas* canvas) {
  bool is_otr = data_.off_the_record;
  canvas->DrawBitmapInt(is_otr ? *tab_inactive_otr.image_l :
                        *tab_inactive.image_l, 0, 0);
  canvas->TileImageInt(is_otr ? *tab_inactive_otr.image_c :
                       *tab_inactive.image_c, tab_inactive.l_width, 0,
                       width() - tab_inactive.l_width -
                       tab_inactive.r_width, height());
  canvas->DrawBitmapInt(is_otr ? *tab_inactive_otr.image_r :
                        *tab_inactive.image_r,
                        width() - tab_inactive.r_width, 0);
}

void TabRenderer::PaintActiveTabBackground(ChromeCanvas* canvas) {
  canvas->DrawBitmapInt(*tab_active.image_l, 0, 0);
  canvas->TileImageInt(*tab_active.image_c, tab_active.l_width, 0,
    width() - tab_active.l_width - tab_active.r_width, height());
  canvas->DrawBitmapInt(*tab_active.image_r, width() - tab_active.r_width, 0);
}

void TabRenderer::PaintHoverTabBackground(ChromeCanvas* canvas,
                                          double opacity) {
  bool is_otr = data_.off_the_record;
  SkBitmap left = skia::ImageOperations::CreateBlendedBitmap(
                  (is_otr ? *tab_inactive_otr.image_l :
                  *tab_inactive.image_l), *tab_hover.image_l, opacity);
  SkBitmap center = skia::ImageOperations::CreateBlendedBitmap(
                   (is_otr ? *tab_inactive_otr.image_c :
                   *tab_inactive.image_c), *tab_hover.image_c, opacity);
  SkBitmap right = skia::ImageOperations::CreateBlendedBitmap(
                   (is_otr ? *tab_inactive_otr.image_r :
                   *tab_inactive.image_r),
                   *tab_hover.image_r, opacity);

  canvas->DrawBitmapInt(left, 0, 0);
  canvas->TileImageInt(center, tab_active.l_width, 0,
      width() - tab_active.l_width - tab_active.r_width, height());
  canvas->DrawBitmapInt(right, width() - tab_active.r_width, 0);
}

void TabRenderer::PaintLoadingAnimation(ChromeCanvas* canvas) {
  SkBitmap* frames = (animation_state_ == ANIMATION_WAITING) ?
                      waiting_animation_frames : loading_animation_frames;
  int image_size = frames->height();
  int image_offset = animation_frame_ * image_size;
  int dst_y = (height() - image_size) / 2;

  // Just like with the Tab's title and favicon, the position for the page
  // loading animation also needs to be mirrored if the View's UI layout is
  // right-to-left.
  int dst_x;
  if (UILayoutIsRightToLeft()) {
    dst_x = width() - kLeftPadding - image_size;
  } else {
    dst_x = kLeftPadding;
  }
  canvas->DrawBitmapInt(*frames, image_offset, 0, image_size,
                        image_size, dst_x, dst_y, image_size, image_size,
                        false);
}

int TabRenderer::IconCapacity() const {
  if (height() < GetMinimumUnselectedSize().height())
    return 0;
  return (width() - kLeftPadding - kRightPadding) / kFaviconSize;
}

bool TabRenderer::ShouldShowIcon() const {
  if (!data_.show_icon) {
    return false;
  } else if (IsSelected()) {
    // The selected tab clips favicon before close button.
    return IconCapacity() >= 2;
  }
  // Non-selected tabs clip close button before favicon.
  return IconCapacity() >= 1;
}

bool TabRenderer::ShouldShowCloseBox() const {
  // The selected tab never clips close button.
  return IsSelected() || IconCapacity() >= 3;
}

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, private:

void TabRenderer::StartCrashAnimation() {
  if (!crash_animation_)
    crash_animation_ = new FavIconCrashAnimation(this);
  crash_animation_->Reset();
  crash_animation_->Start();
}

void TabRenderer::StopCrashAnimation() {
  if (!crash_animation_)
    return;
  crash_animation_->Stop();
}

bool TabRenderer::IsPerformingCrashAnimation() const {
  return crash_animation_ && crash_animation_->IsAnimating();
}

void TabRenderer::SetFavIconHidingOffset(int offset) {
  fav_icon_hiding_offset_ = offset;
  SchedulePaint();
}

void TabRenderer::DisplayCrashedFavIcon() {
  should_display_crashed_favicon_ = true;
}

void TabRenderer::ResetCrashedFavIcon() {
  should_display_crashed_favicon_ = false;
}

// static
void TabRenderer::LoadTabImages(bool use_vista_images) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  tab_active.image_l = rb.GetBitmapNamed(IDR_TAB_ACTIVE_LEFT);
  tab_active.image_c = rb.GetBitmapNamed(IDR_TAB_ACTIVE_CENTER);
  tab_active.image_r = rb.GetBitmapNamed(IDR_TAB_ACTIVE_RIGHT);
  tab_active.l_width = tab_active.image_l->width();
  tab_active.r_width = tab_active.image_r->width();

  tab_hover.image_l = rb.GetBitmapNamed(IDR_TAB_HOVER_LEFT);
  tab_hover.image_c = rb.GetBitmapNamed(IDR_TAB_HOVER_CENTER);
  tab_hover.image_r = rb.GetBitmapNamed(IDR_TAB_HOVER_RIGHT);

  if (use_vista_images) {
    tab_inactive.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT_V);
    tab_inactive.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER_V);
    tab_inactive.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT_V);

    // Our Vista frame doesn't change background color to show OTR,
    // so we continue to use the existing background tabs.
    tab_inactive_otr.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT_V);
    tab_inactive_otr.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER_V);
    tab_inactive_otr.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT_V);
  } else {
    tab_inactive.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT);
    tab_inactive.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER);
    tab_inactive.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT);

    tab_inactive_otr.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT_OTR);
    tab_inactive_otr.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER_OTR);
    tab_inactive_otr.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT_OTR);
  }

  tab_inactive.l_width = tab_inactive.image_l->width();
  tab_inactive.r_width = tab_inactive.image_r->width();
  // tab_[hover,inactive_otr] width are not used and are initialized to 0
  // during static initialization.
}
