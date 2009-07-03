// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/tab_renderer_gtk.h"

#include "app/gfx/canvas_paint.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"

namespace {

const int kLeftPadding = 16;
const int kTopPadding = 6;
const int kRightPadding = 15;
const int kBottomPadding = 5;
const int kFavIconTitleSpacing = 4;
const int kTitleCloseButtonSpacing = 5;
const int kStandardTitleWidth = 175;
const int kFavIconSize = 16;
const int kDropShadowOffset = 2;
const int kSelectedTitleColor = SK_ColorBLACK;
const int kUnselectedTitleColor = SkColorSetRGB(64, 64, 64);

// How long the hover state takes.
const int kHoverDurationMs = 90;

// How opaque to make the hover state (out of 1).
const double kHoverOpacity = 0.33;

const SkScalar kTabCapWidth = 15;
const SkScalar kTabTopCurveWidth = 4;
const SkScalar kTabBottomCurveWidth = 3;

// The vertical and horizontal offset used to position the close button
// in the tab. TODO(jhawkins): Ask pkasting what the Fuzz is about.
const int kCloseButtonVertFuzz = 0;
const int kCloseButtonHorzFuzz = 5;

SkBitmap* crashed_fav_icon = NULL;

TabRendererGtk::LoadingAnimation::Data loading_animation_data;

// Loads the loading animation images and data.
void InitializeLoadingAnimationData(
    ResourceBundle* rb, TabRendererGtk::LoadingAnimation::Data* data) {
  // The loading animation image is a strip of states. Each state must be
  // square, so the height must divide the width evenly.
  data->loading_animation_frames = rb->GetBitmapNamed(IDR_THROBBER);
  DCHECK(data->loading_animation_frames);
  DCHECK_EQ(data->loading_animation_frames->width() %
            data->loading_animation_frames->height(), 0);
  data->loading_animation_frame_count =
      data->loading_animation_frames->width() /
      data->loading_animation_frames->height();

  data->waiting_animation_frames =
      rb->GetBitmapNamed(IDR_THROBBER_WAITING);
  DCHECK(data->waiting_animation_frames);
  DCHECK_EQ(data->waiting_animation_frames->width() %
            data->waiting_animation_frames->height(), 0);
  data->waiting_animation_frame_count =
      data->waiting_animation_frames->width() /
      data->waiting_animation_frames->height();

  data->waiting_to_loading_frame_count_ratio =
      data->waiting_animation_frame_count /
      data->loading_animation_frame_count;
  // TODO(beng): eventually remove this when we have a proper themeing system.
  //             themes not supporting IDR_THROBBER_WAITING are causing this
  //             value to be 0 which causes DIV0 crashes. The value of 5
  //             matches the current bitmaps in our source.
  if (data->waiting_to_loading_frame_count_ratio == 0)
    data->waiting_to_loading_frame_count_ratio = 5;
}

}  // namespace

bool TabRendererGtk::initialized_ = false;
TabRendererGtk::TabImage TabRendererGtk::tab_active_ = {0};
TabRendererGtk::TabImage TabRendererGtk::tab_inactive_ = {0};
TabRendererGtk::TabImage TabRendererGtk::tab_alpha = {0};
TabRendererGtk::TabImage TabRendererGtk::tab_hover_ = {0};
gfx::Font* TabRendererGtk::title_font_ = NULL;
int TabRendererGtk::title_font_height_ = 0;
int TabRendererGtk::close_button_width_ = 0;
int TabRendererGtk::close_button_height_ = 0;

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk::LoadingAnimation, public:
//
TabRendererGtk::LoadingAnimation::LoadingAnimation(const Data* data)
    : data_(data), animation_state_(ANIMATION_NONE), animation_frame_(0) {
}

void TabRendererGtk::LoadingAnimation::ValidateLoadingAnimation(
    AnimationState animation_state) {
  if (animation_state_ != animation_state) {
    // The waiting animation is the reverse of the loading animation, but at a
    // different rate - the following reverses and scales the animation_frame_
    // so that the frame is at an equivalent position when going from one
    // animation to the other.
    if (animation_state_ == ANIMATION_WAITING &&
        animation_state == ANIMATION_LOADING) {
      animation_frame_ = data_->loading_animation_frame_count -
          (animation_frame_ / data_->waiting_to_loading_frame_count_ratio);
    }
    animation_state_ = animation_state;
  }

  if (animation_state_ != ANIMATION_NONE) {
    animation_frame_ = ++animation_frame_ %
                       ((animation_state_ == ANIMATION_WAITING) ?
                         data_->waiting_animation_frame_count :
                         data_->loading_animation_frame_count);
  } else {
    animation_frame_ = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// FaviconCrashAnimation
//
//  A custom animation subclass to manage the favicon crash animation.
class TabRendererGtk::FavIconCrashAnimation : public Animation,
                                              public AnimationDelegate {
 public:
  explicit FavIconCrashAnimation(TabRendererGtk* target)
      : ALLOW_THIS_IN_INITIALIZER_LIST(Animation(1000, 25, this)),
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
  TabRendererGtk* target_;

  DISALLOW_COPY_AND_ASSIGN(FavIconCrashAnimation);
};

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk, public:

TabRendererGtk::TabRendererGtk()
    : showing_icon_(false),
      showing_close_button_(false),
      fav_icon_hiding_offset_(0),
      should_display_crashed_favicon_(false),
      loading_animation_(&loading_animation_data) {
  InitResources();

  tab_.Own(gtk_fixed_new());
  gtk_widget_set_app_paintable(tab_.get(), TRUE);
  g_signal_connect(G_OBJECT(tab_.get()), "expose-event",
                   G_CALLBACK(OnExpose), this);
  close_button_.reset(MakeCloseButton());
  gtk_widget_show(tab_.get());

  hover_animation_.reset(new SlideAnimation(this));
  hover_animation_->SetSlideDuration(kHoverDurationMs);
}

TabRendererGtk::~TabRendererGtk() {
  tab_.Destroy();
}

void TabRendererGtk::UpdateData(TabContents* contents, bool loading_only) {
  DCHECK(contents);
  if (!loading_only) {
    data_.title = UTF16ToWideHack(contents->GetTitle());
    data_.off_the_record = contents->profile()->IsOffTheRecord();
    data_.crashed = contents->is_crashed();
    data_.favicon = contents->GetFavIcon();
  }

  // Loading state also involves whether we show the favicon, since that's where
  // we display the throbber.
  data_.loading = contents->is_loading();
  data_.show_icon = contents->ShouldDisplayFavIcon();

  theme_provider_ = contents->profile()->GetThemeProvider();
}

void TabRendererGtk::UpdateFromModel() {
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

bool TabRendererGtk::IsSelected() const {
  return true;
}

bool TabRendererGtk::IsVisible() const {
  return GTK_WIDGET_FLAGS(tab_.get()) & GTK_VISIBLE;
}

void TabRendererGtk::SetVisible(bool visible) const {
  if (visible) {
    gtk_widget_show(tab_.get());
  } else {
    gtk_widget_hide(tab_.get());
  }
}

void TabRendererGtk::ValidateLoadingAnimation(AnimationState animation_state) {
  loading_animation_.ValidateLoadingAnimation(animation_state);
}

// static
gfx::Size TabRendererGtk::GetMinimumUnselectedSize() {
  InitResources();

  gfx::Size minimum_size;
  minimum_size.set_width(kLeftPadding + kRightPadding);
  // Since we use bitmap images, the real minimum height of the image is
  // defined most accurately by the height of the end cap images.
  minimum_size.set_height(tab_active_.image_l->height());
  return minimum_size;
}

// static
gfx::Size TabRendererGtk::GetMinimumSelectedSize() {
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  minimum_size.set_width(kLeftPadding + kFavIconSize + kRightPadding);
  return minimum_size;
}

// static
gfx::Size TabRendererGtk::GetStandardSize() {
  gfx::Size standard_size = GetMinimumUnselectedSize();
  standard_size.Enlarge(kFavIconTitleSpacing + kStandardTitleWidth, 0);
  return standard_size;
}

// static
int TabRendererGtk::GetContentHeight() {
  // The height of the content of the Tab is the largest of the favicon,
  // the title text and the close button graphic.
  int content_height = std::max(kFavIconSize, title_font_height_);
  return std::max(content_height, close_button_height_);
}

// static
void TabRendererGtk::LoadTabImages() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  tab_alpha.image_l = rb.GetBitmapNamed(IDR_TAB_ALPHA_LEFT);
  tab_alpha.image_r = rb.GetBitmapNamed(IDR_TAB_ALPHA_RIGHT);

  tab_active_.image_l = rb.GetBitmapNamed(IDR_TAB_ACTIVE_LEFT);
  tab_active_.image_c = rb.GetBitmapNamed(IDR_TAB_ACTIVE_CENTER);
  tab_active_.image_r = rb.GetBitmapNamed(IDR_TAB_ACTIVE_RIGHT);
  tab_active_.l_width = tab_active_.image_l->width();
  tab_active_.r_width = tab_active_.image_r->width();

  tab_inactive_.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT);
  tab_inactive_.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER);
  tab_inactive_.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT);
  tab_inactive_.l_width = tab_inactive_.image_l->width();
  tab_inactive_.r_width = tab_inactive_.image_r->width();

  close_button_width_ = rb.GetBitmapNamed(IDR_TAB_CLOSE)->width();
  close_button_height_ = rb.GetBitmapNamed(IDR_TAB_CLOSE)->height();
}

void TabRendererGtk::SetBounds(const gfx::Rect& bounds) {
  gtk_widget_set_size_request(tab_.get(), bounds.width(), bounds.height());
  bounds_ = bounds;
  Layout();
}

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk, protected:

std::wstring TabRendererGtk::GetTitle() const {
  return data_.title;
}

///////////////////////////////////////////////////////////////////////////////
// TabRendererGtk, AnimationDelegate implementation:

void TabRendererGtk::AnimationProgressed(const Animation* animation) {
  gtk_widget_queue_draw(tab_.get());
}

void TabRendererGtk::AnimationCanceled(const Animation* animation) {
  AnimationEnded(animation);
}

void TabRendererGtk::AnimationEnded(const Animation* animation) {
  gtk_widget_queue_draw(tab_.get());
}

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk, private:

void TabRendererGtk::StartCrashAnimation() {
  if (!crash_animation_.get())
    crash_animation_.reset(new FavIconCrashAnimation(this));
  crash_animation_->Reset();
  crash_animation_->Start();
}

void TabRendererGtk::StopCrashAnimation() {
  if (!crash_animation_.get())
    return;
  crash_animation_->Stop();
}

bool TabRendererGtk::IsPerformingCrashAnimation() const {
  return crash_animation_.get() && crash_animation_->IsAnimating();
}

void TabRendererGtk::SetFavIconHidingOffset(int offset) {
  fav_icon_hiding_offset_ = offset;
  SchedulePaint();
}

void TabRendererGtk::DisplayCrashedFavIcon() {
  should_display_crashed_favicon_ = true;
}

void TabRendererGtk::ResetCrashedFavIcon() {
  should_display_crashed_favicon_ = false;
}

void TabRendererGtk::Paint(gfx::Canvas* canvas) {
  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (width() < GetMinimumUnselectedSize().width())
    return;

  // See if the model changes whether the icons should be painted.
  const bool show_icon = ShouldShowIcon();
  const bool show_close_button = ShouldShowCloseBox();
  if (show_icon != showing_icon_ ||
      show_close_button != showing_close_button_)
    Layout();

  PaintTabBackground(canvas);

  if (show_icon) {
    if (loading_animation_.animation_state() != ANIMATION_NONE) {
      PaintLoadingAnimation(canvas);
    } else {
      canvas->save();
      canvas->ClipRectInt(0, 0, width(), height() - kFavIconTitleSpacing);
      if (should_display_crashed_favicon_) {
        canvas->DrawBitmapInt(*crashed_fav_icon, 0, 0,
                              crashed_fav_icon->width(),
                              crashed_fav_icon->height(),
                              favicon_bounds_.x(),
                              favicon_bounds_.y() + fav_icon_hiding_offset_,
                              kFavIconSize, kFavIconSize,
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
                                kFavIconSize, kFavIconSize,
                                true);
        }
      }
      canvas->restore();
    }
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
  canvas->DrawStringInt(title, *title_font_, title_color, title_bounds_.x(),
                        title_bounds_.y(), title_bounds_.width(),
                        title_bounds_.height());
}

SkBitmap TabRendererGtk::PaintBitmap() {
  gfx::Canvas canvas(width(), height(), false);
  Paint(&canvas);
  return canvas.ExtractBitmap();
}

void TabRendererGtk::SchedulePaint() {
  gtk_widget_queue_draw(tab_.get());
}

gfx::Rect TabRendererGtk::GetLocalBounds() {
  return gfx::Rect(0, 0, bounds_.width(), bounds_.height());
}

void TabRendererGtk::Layout() {
  gfx::Rect local_bounds = GetLocalBounds();
  if (local_bounds.IsEmpty())
    return;
  local_bounds.Inset(kLeftPadding, kTopPadding, kRightPadding, kBottomPadding);

  // Figure out who is tallest.
  int content_height = GetContentHeight();

  // Size the Favicon.
  showing_icon_ = ShouldShowIcon();
  if (showing_icon_) {
    int favicon_top = kTopPadding + (content_height - kFavIconSize) / 2;
    favicon_bounds_.SetRect(local_bounds.x(), favicon_top,
                            kFavIconSize, kFavIconSize);
  } else {
    favicon_bounds_.SetRect(local_bounds.x(), local_bounds.y(), 0, 0);
  }

  // Size the Close button.
  showing_close_button_ = ShouldShowCloseBox();
  if (showing_close_button_) {
    int close_button_top =
        kTopPadding + kCloseButtonVertFuzz +
        (content_height - close_button_height_) / 2;
    close_button_bounds_.SetRect(local_bounds.width() + kCloseButtonHorzFuzz,
                                 close_button_top, close_button_width_,
                                 close_button_height_);
  } else {
    close_button_bounds_.SetRect(0, 0, 0, 0);
  }

  // Size the Title text to fill the remaining space.
  int title_left = favicon_bounds_.right() + kFavIconTitleSpacing;
  int title_top = kTopPadding + (content_height - title_font_height_) / 2;

  // If the user has big fonts, the title will appear rendered too far down on
  // the y-axis if we use the regular top padding, so we need to adjust it so
  // that the text appears centered.
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  int text_height = title_top + title_font_height_ + kBottomPadding;
  if (text_height > minimum_size.height())
    title_top -= (text_height - minimum_size.height()) / 2;

  int title_width;
  if (close_button_bounds_.width() && close_button_bounds_.height()) {
    title_width = std::max(close_button_bounds_.x() -
                           kTitleCloseButtonSpacing - title_left, 0);
  } else {
    title_width = std::max(local_bounds.width() - title_left, 0);
  }
  title_bounds_.SetRect(title_left, title_top, title_width, title_font_height_);

  favicon_bounds_.set_x(
      gtk_util::MirroredLeftPointForRect(tab_.get(), favicon_bounds_));
  close_button_bounds_.set_x(
      gtk_util::MirroredLeftPointForRect(tab_.get(), close_button_bounds_));
  title_bounds_.set_x(
      gtk_util::MirroredLeftPointForRect(tab_.get(), title_bounds_));

  MoveCloseButtonWidget();
}

void TabRendererGtk::MoveCloseButtonWidget() {
  if (!close_button_bounds_.IsEmpty()) {
    gtk_fixed_move(GTK_FIXED(tab_.get()), close_button_->widget(),
                   close_button_bounds_.x(), close_button_bounds_.y());
    gtk_widget_show(close_button_->widget());
  } else {
    gtk_widget_hide(close_button_->widget());
  }
}

void TabRendererGtk::PaintTab(GdkEventExpose* event) {
  gfx::CanvasPaint canvas(event, false);
  if (canvas.is_empty())
    return;

  // The tab is rendered into a windowless widget whose offset is at the
  // coordinate event->area.  Translate by these offsets so we can render at
  // (0,0) to match Windows' rendering metrics.
  canvas.TranslateInt(event->area.x, event->area.y);
  Paint(&canvas);
}

void TabRendererGtk::PaintTabBackground(gfx::Canvas* canvas) {
  if (IsSelected()) {
    // Sometimes detaching a tab quickly can result in the model reporting it
    // as not being selected, so is_drag_clone_ ensures that we always paint
    // the active representation for the dragged tab.
    PaintActiveTabBackground(canvas);
  } else {
    // Draw our hover state.
    Animation* animation = hover_animation_.get();

    PaintInactiveTabBackground(canvas);
    if (animation->GetCurrentValue() > 0) {
      SkRect bounds;
      bounds.set(0, 0,
          SkIntToScalar(width()), SkIntToScalar(height()));
      canvas->saveLayerAlpha(&bounds,
          static_cast<int>(animation->GetCurrentValue() * kHoverOpacity * 0xff),
              SkCanvas::kARGB_ClipLayer_SaveFlag);
      canvas->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
      PaintActiveTabBackground(canvas);
      canvas->restore();
    }
  }
}

void TabRendererGtk::PaintInactiveTabBackground(gfx::Canvas* canvas) {
  bool is_otr = data_.off_the_record;

  // The tab image needs to be lined up with the background image
  // so that it feels partially transparent.
  int offset = 1;
  int offset_y = 20;

  int tab_id = is_otr ?
      IDR_THEME_TAB_BACKGROUND_INCOGNITO : IDR_THEME_TAB_BACKGROUND;

  SkBitmap* tab_bg = theme_provider_->GetBitmapNamed(tab_id);

  // Draw left edge.
  SkBitmap tab_l = skia::ImageOperations::CreateTiledBitmap(
      *tab_bg, offset, offset_y,
      tab_active_.l_width, height());
  SkBitmap theme_l = skia::ImageOperations::CreateMaskedBitmap(
      tab_l, *tab_alpha.image_l);
  canvas->DrawBitmapInt(theme_l,
      0, 0, theme_l.width(), theme_l.height() - 1,
      0, 0, theme_l.width(), theme_l.height() - 1,
      false);

  // Draw right edge.
  SkBitmap tab_r = skia::ImageOperations::CreateTiledBitmap(
      *tab_bg,
      offset + width() - tab_active_.r_width, offset_y,
      tab_active_.r_width, height());
  SkBitmap theme_r = skia::ImageOperations::CreateMaskedBitmap(
      tab_r, *tab_alpha.image_r);
  canvas->DrawBitmapInt(theme_r,
      0, 0, theme_r.width(), theme_r.height() - 1,
      width() - theme_r.width(), 0, theme_r.width(), theme_r.height() - 1,
      false);

  // Draw center.
  canvas->TileImageInt(*tab_bg,
      offset + tab_active_.l_width, kDropShadowOffset + offset_y,
      tab_active_.l_width, 2,
      width() - tab_active_.l_width - tab_active_.r_width, height() - 3);

  canvas->DrawBitmapInt(*tab_inactive_.image_l, 0, 0);
  canvas->TileImageInt(*tab_inactive_.image_c, tab_inactive_.l_width, 0,
      width() - tab_inactive_.l_width - tab_inactive_.r_width, height());
  canvas->DrawBitmapInt(*tab_inactive_.image_r,
      width() - tab_inactive_.r_width, 0);
}

void TabRendererGtk::PaintActiveTabBackground(gfx::Canvas* canvas) {
  int offset = 1;

  SkBitmap* tab_bg = theme_provider_->GetBitmapNamed(IDR_THEME_TOOLBAR);

  // Draw left edge.
  SkBitmap tab_l = skia::ImageOperations::CreateTiledBitmap(
      *tab_bg, offset, 0, tab_active_.l_width, height());
  SkBitmap theme_l = skia::ImageOperations::CreateMaskedBitmap(
      tab_l, *tab_alpha.image_l);
  canvas->DrawBitmapInt(theme_l, 0, 0);

  // Draw right edge.
  SkBitmap tab_r = skia::ImageOperations::CreateTiledBitmap(
      *tab_bg,
      offset + width() - tab_active_.r_width, 0,
      tab_active_.r_width, height());
  SkBitmap theme_r = skia::ImageOperations::CreateMaskedBitmap(
      tab_r, *tab_alpha.image_r);
  canvas->DrawBitmapInt(theme_r, width() - tab_active_.r_width, 0);

  // Draw center.
  canvas->TileImageInt(*tab_bg,
      offset + tab_active_.l_width, 2,
      tab_active_.l_width, 2,
      width() - tab_active_.l_width - tab_active_.r_width, height() - 2);

  canvas->DrawBitmapInt(*tab_active_.image_l, 0, 0);
  canvas->TileImageInt(*tab_active_.image_c, tab_active_.l_width, 0,
      width() - tab_active_.l_width - tab_active_.r_width, height());
  canvas->DrawBitmapInt(*tab_active_.image_r, width() - tab_active_.r_width, 0);
}

void TabRendererGtk::PaintLoadingAnimation(gfx::Canvas* canvas) {
  const SkBitmap* frames =
      (loading_animation_.animation_state() == ANIMATION_WAITING) ?
      loading_animation_.waiting_animation_frames() :
      loading_animation_.loading_animation_frames();
  const int image_size = frames->height();
  const int image_offset = loading_animation_.animation_frame() * image_size;
  const int dst_y = (height() - image_size) / 2;

  // Just like with the Tab's title and favicon, the position for the page
  // loading animation also needs to be mirrored if the UI layout is RTL.
  int dst_x;
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    dst_x = width() - kLeftPadding - image_size;
  } else {
    dst_x = kLeftPadding;
  }

  canvas->DrawBitmapInt(*frames, image_offset, 0, image_size,
                        image_size, dst_x, dst_y, image_size, image_size,
                        false);
}

int TabRendererGtk::IconCapacity() const {
  if (height() < GetMinimumUnselectedSize().height())
    return 0;
  return (width() - kLeftPadding - kRightPadding) / kFavIconSize;
}

bool TabRendererGtk::ShouldShowIcon() const {
  if (!data_.show_icon) {
    return false;
  } else if (IsSelected()) {
    // The selected tab clips favicon before close button.
    return IconCapacity() >= 2;
  }
  // Non-selected tabs clip close button before favicon.
  return IconCapacity() >= 1;
}

bool TabRendererGtk::ShouldShowCloseBox() const {
  // The selected tab never clips close button.
  return IsSelected() || IconCapacity() >= 3;
}

CustomDrawButton* TabRendererGtk::MakeCloseButton() {
  CustomDrawButton* button = new CustomDrawButton(IDR_TAB_CLOSE,
      IDR_TAB_CLOSE_P, IDR_TAB_CLOSE_H, IDR_TAB_CLOSE, NULL);

  g_signal_connect(G_OBJECT(button->widget()), "clicked",
                   G_CALLBACK(OnCloseButtonClicked), this);
  g_signal_connect(G_OBJECT(button->widget()), "button-release-event",
                   G_CALLBACK(OnCloseButtonMouseRelease), this);
  GTK_WIDGET_UNSET_FLAGS(button->widget(), GTK_CAN_FOCUS);
  gtk_fixed_put(GTK_FIXED(tab_.get()), button->widget(), 0, 0);

  return button;
}

void TabRendererGtk::CloseButtonClicked() {
  // Nothing to do.
}

// static
void TabRendererGtk::OnCloseButtonClicked(GtkWidget* widget,
                                          TabRendererGtk* tab) {
  tab->CloseButtonClicked();
}

// static
gboolean TabRendererGtk::OnCloseButtonMouseRelease(GtkWidget* widget,
                                                   GdkEventButton* event,
                                                   TabRendererGtk* tab) {
  if (event->button == 2) {
    tab->CloseButtonClicked();
    return TRUE;
  }

  return FALSE;
}

// static
gboolean TabRendererGtk::OnExpose(GtkWidget* widget, GdkEventExpose* event,
                                  TabRendererGtk* tab) {
  tab->PaintTab(event);
  gtk_container_propagate_expose(GTK_CONTAINER(tab->tab_.get()),
                                 tab->close_button_->widget(), event);
  return TRUE;
}

void TabRendererGtk::OnMouseEntered() {
  hover_animation_->SetTweenType(SlideAnimation::EASE_OUT);
  hover_animation_->Show();
}

void TabRendererGtk::OnMouseExited() {
  hover_animation_->SetTweenType(SlideAnimation::EASE_IN);
  hover_animation_->Hide();
}

// static
void TabRendererGtk::InitResources() {
  if (initialized_)
    return;

  LoadTabImages();

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  // Force the font size to 10pt.
  gfx::Font base_font = rb.GetFont(ResourceBundle::BaseFont);
  title_font_ = new gfx::Font(gfx::Font::CreateFont(base_font.FontName(), 10));
  title_font_height_ = title_font_->height();

  InitializeLoadingAnimationData(&rb, &loading_animation_data);

  crashed_fav_icon = rb.GetBitmapNamed(IDR_SAD_FAVICON);

  initialized_ = true;
}
