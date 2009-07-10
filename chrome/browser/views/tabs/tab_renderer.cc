// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_renderer.h"

#include <limits>

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "views/widget/widget.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

#ifdef WIN32
#include "app/win_util.h"
#endif

static const int kLeftPadding = 16;
static const int kTopPadding = 6;
static const int kRightPadding = 15;
static const int kBottomPadding = 5;
static const int kDropShadowHeight = 2;
static const int kToolbarOverlap = 1;
static const int kFavIconTitleSpacing = 4;
static const int kTitleCloseButtonSpacing = 5;
static const int kStandardTitleWidth = 175;
static const int kCloseButtonVertFuzz = 0;
static const int kCloseButtonHorzFuzz = 5;
static const int kFaviconSize = 16;
static const int kSelectedTitleColor = SK_ColorBLACK;

// How long the hover state takes.
static const int kHoverDurationMs = 90;

// How long the pulse throb takes.
static const int kPulseDurationMs = 200;

// How opaque to make the hover state (out of 1).
static const double kHoverOpacity = 0.33;

// TODO(beng): (Cleanup) This stuff should move onto the class.
static gfx::Font* title_font = NULL;
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

TabRenderer::TabImage TabRenderer::tab_alpha = {0};
TabRenderer::TabImage TabRenderer::tab_active = {0};
TabRenderer::TabImage TabRenderer::tab_inactive = {0};

namespace {

void InitResources() {
  static bool initialized = false;
  if (!initialized) {
    // TODO(glen): Allow theming of these.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    title_font = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont));
    title_font_height = title_font->height();

    close_button_n = rb.GetBitmapNamed(IDR_TAB_CLOSE);
    close_button_h = rb.GetBitmapNamed(IDR_TAB_CLOSE_H);
    close_button_p = rb.GetBitmapNamed(IDR_TAB_CLOSE_P);
    close_button_width = close_button_n->width();
    close_button_height = close_button_n->height();

    TabRenderer::LoadTabImages();

    // The loading animation image is a strip of states. Each state must be
    // square, so the height must divide the width evenly.
    loading_animation_frames = rb.GetBitmapNamed(IDR_THROBBER);
    DCHECK(loading_animation_frames);
    DCHECK(loading_animation_frames->width() %
           loading_animation_frames->height() == 0);
    loading_animation_frame_count =
        loading_animation_frames->width() / loading_animation_frames->height();

    // We get a DIV0 further down when the throbber is replaced by an image
    // which is taller than wide. In this case we cannot deduce an animation
    // sequence from it since we assume that each animation frame has the width
    // of the image's height.
    if (loading_animation_frame_count == 0) {
#ifdef WIN32
      // TODO(idanan): Remove this when we have a way to handle theme errors.
      // See: http://code.google.com/p/chromium/issues/detail?id=12531
      // For now, this is Windows-specific because some users have downloaded
      // a DLL from outside of Google to override the theme.
      std::wstring text = l10n_util::GetString(IDS_RESOURCE_ERROR);
      std::wstring caption = l10n_util::GetString(IDS_RESOURCE_ERROR_CAPTION);
      UINT flags = MB_OK | MB_ICONWARNING | MB_TOPMOST;
      win_util::MessageBox(NULL, text, caption, flags);
#endif
      CHECK(loading_animation_frame_count) << "Invalid throbber size. Width = "
          << loading_animation_frames->width() << ", height = "
          << loading_animation_frames->height();
    }

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
  TabRenderer* target_;

  DISALLOW_EVIL_CONSTRUCTORS(FavIconCrashAnimation);
};

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, public:

TabRenderer::TabRenderer()
    : animation_state_(ANIMATION_NONE),
      animation_frame_(0),
      showing_icon_(false),
      showing_close_button_(false),
      fav_icon_hiding_offset_(0),
      crash_animation_(NULL),
      should_display_crashed_favicon_(false),
      theme_provider_(NULL) {
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

void TabRenderer::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (parent->GetThemeProvider())
    SetThemeProvider(parent->GetThemeProvider());
}

ThemeProvider* TabRenderer::GetThemeProvider() {
  ThemeProvider* tp = View::GetThemeProvider();
  if (tp)
    return tp;

  if (theme_provider_)
    return theme_provider_;

  // return contents->profile()->GetThemeProvider();
  NOTREACHED() << "Unable to find a theme provider";
  return NULL;
}

void TabRenderer::UpdateData(TabContents* contents, bool loading_only) {
  DCHECK(contents);
  if (!loading_only) {
    data_.title = contents->GetTitle();
    data_.off_the_record = contents->profile()->IsOffTheRecord();
    data_.crashed = contents->is_crashed();
    data_.favicon = contents->GetFavIcon();
  }

  // TODO(glen): Temporary hax.
  theme_provider_ = contents->profile()->GetThemeProvider();

  // Loading state also involves whether we show the favicon, since that's where
  // we display the throbber.
  data_.loading = contents->is_loading();
  data_.show_icon = contents->ShouldDisplayFavIcon();
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
  return UTF16ToWideHack(data_.title);
}

////////////////////////////////////////////////////////////////////////////////
// TabRenderer, views::View overrides:

void TabRenderer::Paint(gfx::Canvas* canvas) {
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

  // Paint the Title.
  string16 title = data_.title;
  if (title.empty()) {
    if (data_.loading) {
      title = l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);
    } else {
      title = l10n_util::GetStringUTF16(IDS_TAB_UNTITLED_TITLE);
    }
  } else {
    Browser::FormatTitleForDisplay(&title);
  }

  SkColor title_color = GetThemeProvider()->
      GetColor(IsSelected() ?
          BrowserThemeProvider::COLOR_TAB_TEXT :
          BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT);

  canvas->DrawStringInt(UTF16ToWideHack(title), *title_font, title_color,
                        title_bounds_.x(), title_bounds_.y(),
                        title_bounds_.width(), title_bounds_.height());
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
  title_bounds_.SetRect(title_left, title_top, title_width, title_font_height);

  // Certain UI elements within the Tab (the favicon, etc.) are not represented
  // as child Views (which is the preferred method).  Instead, these UI elements
  // are drawn directly on the canvas from within Tab::Paint(). The Tab's child
  // Views (for example, the Tab's close button which is a views::Button
  // instance) are automatically mirrored by the mirroring infrastructure in
  // views. The elements Tab draws directly on the canvas need to be manually
  // mirrored if the View's layout is right-to-left.
  favicon_bounds_.set_x(MirroredLeftPointForRect(favicon_bounds_));
  title_bounds_.set_x(MirroredLeftPointForRect(title_bounds_));
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
  LoadTabImages();
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

void TabRenderer::PaintTabBackground(gfx::Canvas* canvas) {
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

    PaintInactiveTabBackground(canvas);
    if (animation->GetCurrentValue() > 0) {
      SkRect bounds;
      bounds.set(0, 0, SkIntToScalar(width()), SkIntToScalar(height()));
      canvas->saveLayerAlpha(&bounds,
          static_cast<int>(animation->GetCurrentValue() * kHoverOpacity * 0xff),
              SkCanvas::kARGB_ClipLayer_SaveFlag);
      canvas->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
      PaintActiveTabBackground(canvas);
      canvas->restore();
    }
  }
}

void TabRenderer::PaintInactiveTabBackground(gfx::Canvas* canvas) {
  bool is_otr = data_.off_the_record;

  // The tab image needs to be lined up with the background image
  // so that it feels partially transparent.  These offsets represent the tab
  // position within the frame background image.
  int offset = GetX(views::View::APPLY_MIRRORING_TRANSFORMATION) +
      background_offset_.x();

  int tab_id;
  if (GetWidget() &&
      GetWidget()->GetWindow()->GetNonClientView()->UseNativeFrame()) {
    tab_id = IDR_THEME_TAB_BACKGROUND_V;
  } else {
    tab_id = is_otr ? IDR_THEME_TAB_BACKGROUND_INCOGNITO :
                      IDR_THEME_TAB_BACKGROUND;
  }

  SkBitmap* tab_bg = GetThemeProvider()->GetBitmapNamed(tab_id);

  // Draw left edge.  Don't draw over the toolbar, as we're not the foreground
  // tab.
  SkBitmap tab_l = skia::ImageOperations::CreateTiledBitmap(
      *tab_bg, offset, background_offset_.y(),
      tab_active.l_width, height());
  SkBitmap theme_l = skia::ImageOperations::CreateMaskedBitmap(
      tab_l, *tab_alpha.image_l);
  canvas->DrawBitmapInt(theme_l,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      false);

  // Draw right edge.  Again, don't draw over the toolbar.
  SkBitmap tab_r = skia::ImageOperations::CreateTiledBitmap(
      *tab_bg,
      offset + width() - tab_active.r_width, background_offset_.y(),
      tab_active.r_width, height());
  SkBitmap theme_r = skia::ImageOperations::CreateMaskedBitmap(
      tab_r, *tab_alpha.image_r);
  canvas->DrawBitmapInt(theme_r,
      0, 0, theme_r.width(), theme_r.height() - kToolbarOverlap,
      width() - theme_r.width(), 0, theme_r.width(),
      theme_r.height() - kToolbarOverlap, false);

  // Draw center.  Instead of masking out the top portion we simply skip over it
  // by incrementing by kDropShadowHeight, since it's a simple rectangle.  And
  // again, don't draw over the toolbar.
  canvas->TileImageInt(*tab_bg,
     offset + tab_active.l_width, background_offset_.y() + kDropShadowHeight,
     tab_active.l_width, kDropShadowHeight,
     width() - tab_active.l_width - tab_active.r_width,
     height() - kDropShadowHeight - kToolbarOverlap);

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawBitmapInt(*tab_inactive.image_l, 0, 0);
  canvas->TileImageInt(*tab_inactive.image_c,
                       tab_inactive.l_width, 0,
                       width() - tab_inactive.l_width - tab_inactive.r_width,
                       height());
  canvas->DrawBitmapInt(*tab_inactive.image_r,
                        width() - tab_inactive.r_width, 0);
}

void TabRenderer::PaintActiveTabBackground(gfx::Canvas* canvas) {
  int offset = GetX(views::View::APPLY_MIRRORING_TRANSFORMATION) +
      background_offset_.x();
  ThemeProvider* tp = GetThemeProvider();
  if (!tp)
    NOTREACHED() << "Unable to get theme provider";

  SkBitmap* tab_bg = GetThemeProvider()->GetBitmapNamed(IDR_THEME_TOOLBAR);

  // Draw left edge.
  SkBitmap tab_l = skia::ImageOperations::CreateTiledBitmap(
      *tab_bg, offset, 0, tab_active.l_width, height());
  SkBitmap theme_l = skia::ImageOperations::CreateMaskedBitmap(
      tab_l, *tab_alpha.image_l);
  canvas->DrawBitmapInt(theme_l, 0, 0);

  // Draw right edge.
  SkBitmap tab_r = skia::ImageOperations::CreateTiledBitmap(
      *tab_bg,
      offset + width() - tab_active.r_width, 0,
      tab_active.r_width, height());
  SkBitmap theme_r = skia::ImageOperations::CreateMaskedBitmap(
      tab_r, *tab_alpha.image_r);
  canvas->DrawBitmapInt(theme_r, width() - tab_active.r_width, 0);

  // Draw center.  Instead of masking out the top portion we simply skip over it
  // by incrementing by kDropShadowHeight, since it's a simple rectangle.
  canvas->TileImageInt(*tab_bg,
     offset + tab_active.l_width, kDropShadowHeight,
     tab_active.l_width, kDropShadowHeight,
     width() - tab_active.l_width - tab_active.r_width,
     height() - kDropShadowHeight);

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawBitmapInt(*tab_active.image_l, 0, 0);
  canvas->TileImageInt(*tab_active.image_c, tab_active.l_width, 0,
      width() - tab_active.l_width - tab_active.r_width, height());
  canvas->DrawBitmapInt(*tab_active.image_r, width() - tab_active.r_width, 0);
}

void TabRenderer::PaintHoverTabBackground(gfx::Canvas* canvas,
                                          double opacity) {
  SkBitmap left = skia::ImageOperations::CreateBlendedBitmap(
                  *tab_inactive.image_l, *tab_active.image_l, opacity);
  SkBitmap center = skia::ImageOperations::CreateBlendedBitmap(
                    *tab_inactive.image_c, *tab_active.image_c, opacity);
  SkBitmap right = skia::ImageOperations::CreateBlendedBitmap(
                   *tab_inactive.image_r, *tab_active.image_r, opacity);

  canvas->DrawBitmapInt(left, 0, 0);
  canvas->TileImageInt(center, tab_active.l_width, 0,
      width() - tab_active.l_width - tab_active.r_width, height());
  canvas->DrawBitmapInt(right, width() - tab_active.r_width, 0);
}

void TabRenderer::PaintLoadingAnimation(gfx::Canvas* canvas) {
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
void TabRenderer::LoadTabImages() {
  // We're not letting people override tab images just yet.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  tab_alpha.image_l = rb.GetBitmapNamed(IDR_TAB_ALPHA_LEFT);
  tab_alpha.image_r = rb.GetBitmapNamed(IDR_TAB_ALPHA_RIGHT);

  tab_active.image_l = rb.GetBitmapNamed(IDR_TAB_ACTIVE_LEFT);
  tab_active.image_c = rb.GetBitmapNamed(IDR_TAB_ACTIVE_CENTER);
  tab_active.image_r = rb.GetBitmapNamed(IDR_TAB_ACTIVE_RIGHT);
  tab_active.l_width = tab_active.image_l->width();
  tab_active.r_width = tab_active.image_r->width();

  tab_inactive.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT);
  tab_inactive.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER);
  tab_inactive.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT);

  tab_inactive.l_width = tab_inactive.image_l->width();
  tab_inactive.r_width = tab_inactive.image_r->width();

  loading_animation_frames = rb.GetBitmapNamed(IDR_THROBBER);
  waiting_animation_frames = rb.GetBitmapNamed(IDR_THROBBER_WAITING);
}
