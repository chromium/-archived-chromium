// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/tabs/tab_2.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "app/throb_animation.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"  // required for FormatTitleForDisplay.
#include "chrome/browser/browser_theme_provider.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "views/animator.h"
#include "views/controls/button/image_button.h"
#include "views/widget/widget.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

static const SkScalar kTabCapWidth = 15;
static const SkScalar kTabTopCurveWidth = 4;
static const SkScalar kTabBottomCurveWidth = 3;

// Space between the edges of the tab's bounds and its content.
static const int kLeftPadding = 16;
static const int kTopPadding = 6;
static const int kRightPadding = 15;
static const int kBottomPadding = 5;

// The height of the "drop shadow" drawn across the top of the tab. We allow
// the containing window to consider this region part of the window caption
// rather than the tab, since we are otherwise starved for drag area.
static const int kDropShadowHeight = 2;

// By how much the bottom edge of the tab overlaps the top of the toolbar.
static const int kToolbarOverlap = 1;

// The space between the tab icon and the title.
static const int kIconTitleSpacing = 4;

// The space between the tab title and the close button.
static const int kTitleCloseButtonSpacing = 5;

// The ideal width of a tab, provided sufficient width is available.
static const int kStandardTitleWidth = 175;

// TODO(beng): figure out what these are used for.
static const int kCloseButtonVertFuzz = 0;
static const int kCloseButtonHorzFuzz = 5;

// The size (both width and height) of the tab icon.
static const int kIconSize = 16;

// The color of the text painted in tabs.
static const int kSelectedTitleColor = SK_ColorBLACK;

// How long the hover state takes.
static const int kHoverDurationMs = 90;

// How long the pulse throb takes.
static const int kPulseDurationMs = 200;

// How opaque to make the hover state (out of 1).
static const double kHoverOpacity = 0.33;

// Resources for rendering tabs.
static gfx::Font* title_font = NULL;
static int title_font_height = 0;
static SkBitmap* close_button_n = NULL;
static SkBitmap* close_button_h = NULL;
static SkBitmap* close_button_p = NULL;
static int close_button_height = 0;
static int close_button_width = 0;

static SkBitmap* waiting_animation_frames = NULL;
static SkBitmap* loading_animation_frames = NULL;
static SkBitmap* crashed_icon = NULL;
static int loading_animation_frame_count = 0;
static int waiting_animation_frame_count = 0;
static int waiting_to_loading_frame_count_ratio = 0;

Tab2::TabImage Tab2::tab_alpha_ = {0};
Tab2::TabImage Tab2::tab_active_ = {0};
Tab2::TabImage Tab2::tab_inactive_ = {0};

namespace {

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
    ImageButton::OnMouseEntered(event);
    GetParent()->OnMouseEntered(event);
  }

  virtual void OnMouseExited(const views::MouseEvent& event) {
    ImageButton::OnMouseExited(event);
    GetParent()->OnMouseExited(event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabCloseButton);
};

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

    Tab2::LoadTabImages();

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

    crashed_icon = rb.GetBitmapNamed(IDR_SAD_FAVICON);

    initialized = true;
  }
}

int GetContentHeight() {
  // The height of the content of the Tab is the largest of the icon,
  // the title text and the close button graphic.
  int content_height = std::max(kIconSize, title_font_height);
  return std::max(content_height, close_button_height);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Tab2, public:

Tab2::Tab2(Tab2Model* model)
    : model_(model),
      dragging_(false),
      removing_(false) {
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

Tab2::~Tab2() {
}

void Tab2::SetRemovingModel(Tab2Model* model) {
  removing_model_.reset(model);
  model_ = removing_model_.get();
}

bool Tab2::IsAnimating() const {
  return animator_.get() && animator_->IsAnimating();
}

views::Animator* Tab2::GetAnimator() {
  if (!animator_.get())
    animator_.reset(new views::Animator(this, model_->AsAnimatorDelegate()));
  return animator_.get();
}

// static
gfx::Size Tab2::GetMinimumUnselectedSize() {
  InitResources();

  gfx::Size minimum_size;
  minimum_size.set_width(kLeftPadding + kRightPadding);
  // Since we use bitmap images, the real minimum height of the image is
  // defined most accurately by the height of the end cap images.
  minimum_size.set_height(tab_active_.image_l->height());
  return minimum_size;
}

// static
gfx::Size Tab2::GetMinimumSelectedSize() {
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  minimum_size.set_width(kLeftPadding + kIconSize + kRightPadding);
  return minimum_size;
}

// static
gfx::Size Tab2::GetStandardSize() {
  gfx::Size standard_size = GetMinimumUnselectedSize();
  standard_size.set_width(
      standard_size.width() + kIconTitleSpacing + kStandardTitleWidth);
  return standard_size;
}

void Tab2::AddTabShapeToPath(gfx::Path* path) const {
  SkScalar h = SkIntToScalar(height());
  SkScalar w = SkIntToScalar(width());

  path->moveTo(0, h);

  // Left end cap.
  path->lineTo(kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(kTabCapWidth, 0);

  // Connect to the right cap.
  path->lineTo(w - kTabCapWidth, 0);

  // Right end cap.
  path->lineTo(w - kTabCapWidth + kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(w - kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(w, h);

  // Close out the path.
  path->lineTo(0, h);
  path->close();
}

///////////////////////////////////////////////////////////////////////////////
// Tab2, views::ButtonListener implementation:

void Tab2::ButtonPressed(views::Button* sender) {
  if (sender == close_button_)
    model_->CloseTab(this);
}

////////////////////////////////////////////////////////////////////////////////
// Tab2, views::View overrides:

void Tab2::Layout() {
  gfx::Rect content_rect = GetLocalBounds(false);
  if (content_rect.IsEmpty())
    return;
  content_rect.Inset(kLeftPadding, kTopPadding, kRightPadding, kBottomPadding);

  int content_height = GetContentHeight();
  LayoutIcon(content_height, content_rect);
  LayoutCloseButton(content_height, content_rect);
  LayoutTitle(content_height, content_rect);
}

void Tab2::Paint(gfx::Canvas* canvas) {
  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (width() < GetMinimumUnselectedSize().width())
    return;

  PaintTabBackground(canvas);
  if (ShouldShowIcon())
    PaintIcon(canvas);
  PaintTitle(canvas);
}

void Tab2::OnMouseEntered(const views::MouseEvent& e) {
  hover_animation_->SetTweenType(SlideAnimation::EASE_OUT);
  hover_animation_->Show();
}

void Tab2::OnMouseExited(const views::MouseEvent& e) {
  hover_animation_->SetTweenType(SlideAnimation::EASE_IN);
  hover_animation_->Hide();
}

bool Tab2::OnMousePressed(const views::MouseEvent& event) {
  if (event.IsLeftMouseButton()) {
    model_->SelectTab(this);
    model_->CaptureDragInfo(this, event);
  }
  return true;
}

bool Tab2::OnMouseDragged(const views::MouseEvent& event) {
  dragging_ = true;
  return model_->DragTab(this, event);
}

void Tab2::OnMouseReleased(const views::MouseEvent& event, bool canceled) {
  if (dragging_) {
    dragging_ = false;
    model_->DragEnded(this);
  }
}

void Tab2::ThemeChanged() {
  LoadTabImages();
  View::ThemeChanged();
}

void Tab2::ViewHierarchyChanged(bool is_add, views::View* parent,
                                views::View* child) {
  if (parent->GetThemeProvider())
    theme_provider_ = parent->GetThemeProvider();
}

ThemeProvider* Tab2::GetThemeProvider() {
  ThemeProvider* provider = View::GetThemeProvider();
  if (provider)
    return provider;

  if (theme_provider_)
    return theme_provider_;

  // return contents->profile()->GetThemeProvider();
  NOTREACHED() << "Unable to find a theme provider";
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Tab2, views::AnimationDelegate implementation:

void Tab2::AnimationProgressed(const Animation* animation) {
  SchedulePaint();
}

void Tab2::AnimationCanceled(const Animation* animation) {
  AnimationEnded(animation);
}

void Tab2::AnimationEnded(const Animation* animation) {
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// Tab2, private:

void Tab2::LayoutIcon(int content_height, const gfx::Rect& content_rect) {
  showing_icon_ = ShouldShowIcon();
  if (showing_icon_) {
    int icon_y = kTopPadding + (content_height - kIconSize) / 2;
    icon_bounds_.SetRect(content_rect.x(), icon_y, kIconSize, kIconSize);
  } else {
    icon_bounds_.SetRect(content_rect.x(), content_rect.y(), 0, 0);
  }

  // Since we paint the icon manually instead of using a child view, we need
  // to adjust its bounds for RTL.
  icon_bounds_.set_x(MirroredLeftPointForRect(icon_bounds_));
}

void Tab2::LayoutCloseButton(int content_height,
                             const gfx::Rect& content_rect) {
  showing_close_button_ = ShouldShowCloseBox();
  if (showing_close_button_) {
    int close_button_top =
        kTopPadding + kCloseButtonVertFuzz +
        (content_height - close_button_height) / 2;
    // If the ratio of the close button size to tab width exceeds the maximum.
    close_button_->SetBounds(content_rect.width() + kCloseButtonHorzFuzz,
                             close_button_top, close_button_width,
                             close_button_height);
    close_button_->SetVisible(true);
  } else {
    close_button_->SetBounds(0, 0, 0, 0);
    close_button_->SetVisible(false);
  }
}

void Tab2::LayoutTitle(int content_height, const gfx::Rect& content_rect) {
  // Size the Title text to fill the remaining space.
  int title_left = icon_bounds_.right() + kIconTitleSpacing;
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
    title_width = std::max(content_rect.width() - title_left, 0);
  }
  title_bounds_.SetRect(title_left, title_top, title_width, title_font_height);

  // Since we paint the title manually instead of using a child view, we need
  // to adjust its bounds for RTL.
  title_bounds_.set_x(MirroredLeftPointForRect(title_bounds_));
}

void Tab2::PaintIcon(gfx::Canvas* canvas) {
  if (animation_state_ != ANIMATION_NONE) {
    PaintLoadingAnimation(canvas);
  } else {
    canvas->save();
    canvas->ClipRectInt(0, 0, width(), height() - 4);
    SkBitmap icon = model_->GetIcon(this);
    if (!icon.isNull()) {
      // TODO(pkasting): Use code in tab_icon_view.cc:PaintIcon() (or switch
      // to using that class to render the icon).
      canvas->DrawBitmapInt(icon, 0, 0, icon.width(), icon.height(),
                            icon_bounds_.x(),
                            icon_bounds_.y() + icon_hiding_offset_,
                            kIconSize, kIconSize, true);
    }
    canvas->restore();
  }
}

void Tab2::PaintTitle(gfx::Canvas* canvas) {
  // Paint the Title.
  string16 title = model_->GetTitle(this);
  if (title.empty()) {
    if (model_->IsLoading(this))
      title = l10n_util::GetString(IDS_TAB_LOADING_TITLE);
    else
      title = l10n_util::GetString(IDS_TAB_UNTITLED_TITLE);
  } else {
    Browser::FormatTitleForDisplay(&title);
  }

  SkColor title_color = GetThemeProvider()->
      GetColor(model_->IsSelected(this) ?
          BrowserThemeProvider::COLOR_TAB_TEXT :
          BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT);

  canvas->DrawStringInt(title, *title_font, title_color, title_bounds_.x(),
                        title_bounds_.y(), title_bounds_.width(),
                        title_bounds_.height());
}

void Tab2::PaintTabBackground(gfx::Canvas* canvas) {
  if (model_->IsSelected(this)) {
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

void Tab2::PaintInactiveTabBackground(gfx::Canvas* canvas) {
  bool is_otr = model_->IsIncognito(this);

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
      tab_active_.l_width, height());
  SkBitmap theme_l = skia::ImageOperations::CreateMaskedBitmap(
      tab_l, *tab_alpha_.image_l);
  canvas->DrawBitmapInt(theme_l,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      false);

  // Draw right edge.  Again, don't draw over the toolbar.
  SkBitmap tab_r = skia::ImageOperations::CreateTiledBitmap(
      *tab_bg,
      offset + width() - tab_active_.r_width, background_offset_.y(),
      tab_active_.r_width, height());
  SkBitmap theme_r = skia::ImageOperations::CreateMaskedBitmap(
      tab_r, *tab_alpha_.image_r);
  canvas->DrawBitmapInt(theme_r,
      0, 0, theme_r.width(), theme_r.height() - kToolbarOverlap,
      width() - theme_r.width(), 0, theme_r.width(),
      theme_r.height() - kToolbarOverlap, false);

  // Draw center.  Instead of masking out the top portion we simply skip over it
  // by incrementing by kDropShadowHeight, since it's a simple rectangle.  And
  // again, don't draw over the toolbar.
  canvas->TileImageInt(*tab_bg,
     offset + tab_active_.l_width, background_offset_.y() + kDropShadowHeight,
     tab_active_.l_width, kDropShadowHeight,
     width() - tab_active_.l_width - tab_active_.r_width,
     height() - kDropShadowHeight - kToolbarOverlap);

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawBitmapInt(*tab_inactive_.image_l, 0, 0);
  canvas->TileImageInt(*tab_inactive_.image_c,
                       tab_inactive_.l_width, 0,
                       width() - tab_inactive_.l_width - tab_inactive_.r_width,
                       height());
  canvas->DrawBitmapInt(*tab_inactive_.image_r,
                        width() - tab_inactive_.r_width, 0);
}

void Tab2::PaintActiveTabBackground(gfx::Canvas* canvas) {
  int offset = GetX(views::View::APPLY_MIRRORING_TRANSFORMATION) +
      background_offset_.x();
  ThemeProvider* tp = GetThemeProvider();
  if (!tp)
    NOTREACHED() << "Unable to get theme provider";

  SkBitmap* tab_bg = GetThemeProvider()->GetBitmapNamed(IDR_THEME_TOOLBAR);

  // Draw left edge.
  SkBitmap tab_l = skia::ImageOperations::CreateTiledBitmap(
      *tab_bg, offset, 0, tab_active_.l_width, height());
  SkBitmap theme_l = skia::ImageOperations::CreateMaskedBitmap(
      tab_l, *tab_alpha_.image_l);
  canvas->DrawBitmapInt(theme_l, 0, 0);

  // Draw right edge.
  SkBitmap tab_r = skia::ImageOperations::CreateTiledBitmap(
      *tab_bg,
      offset + width() - tab_active_.r_width, 0,
      tab_active_.r_width, height());
  SkBitmap theme_r = skia::ImageOperations::CreateMaskedBitmap(
      tab_r, *tab_alpha_.image_r);
  canvas->DrawBitmapInt(theme_r, width() - tab_active_.r_width, 0);

  // Draw center.  Instead of masking out the top portion we simply skip over it
  // by incrementing by kDropShadowHeight, since it's a simple rectangle.
  canvas->TileImageInt(*tab_bg,
     offset + tab_active_.l_width, kDropShadowHeight,
     tab_active_.l_width, kDropShadowHeight,
     width() - tab_active_.l_width - tab_active_.r_width,
     height() - kDropShadowHeight);

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawBitmapInt(*tab_active_.image_l, 0, 0);
  canvas->TileImageInt(*tab_active_.image_c, tab_active_.l_width, 0,
      width() - tab_active_.l_width - tab_active_.r_width, height());
  canvas->DrawBitmapInt(*tab_active_.image_r, width() - tab_active_.r_width, 0);
}

void Tab2::PaintHoverTabBackground(gfx::Canvas* canvas, double opacity) {
  SkBitmap left = skia::ImageOperations::CreateBlendedBitmap(
                  *tab_inactive_.image_l, *tab_active_.image_l, opacity);
  SkBitmap center = skia::ImageOperations::CreateBlendedBitmap(
                    *tab_inactive_.image_c, *tab_active_.image_c, opacity);
  SkBitmap right = skia::ImageOperations::CreateBlendedBitmap(
                   *tab_inactive_.image_r, *tab_active_.image_r, opacity);

  canvas->DrawBitmapInt(left, 0, 0);
  canvas->TileImageInt(center, tab_active_.l_width, 0,
      width() - tab_active_.l_width - tab_active_.r_width, height());
  canvas->DrawBitmapInt(right, width() - tab_active_.r_width, 0);
}

void Tab2::PaintLoadingAnimation(gfx::Canvas* canvas) {
  SkBitmap* frames = (animation_state_ == ANIMATION_WAITING) ?
                      waiting_animation_frames : loading_animation_frames;
  int image_size = frames->height();
  int image_offset = animation_frame_ * image_size;
  int dst_y = (height() - image_size) / 2;

  // Just like with the Tab's title and icon, the position for the page
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

int Tab2::IconCapacity() const {
  if (height() < GetMinimumUnselectedSize().height())
    return 0;
  return (width() - kLeftPadding - kRightPadding) / kIconSize;
}

bool Tab2::ShouldShowIcon() const {
  if (!model_->ShouldShowIcon(const_cast<Tab2*>(this))) {
    return false;
  } else if (model_->IsSelected(const_cast<Tab2*>(this))) {
    // The selected tab clips icon before close button.
    return IconCapacity() >= 2;
  }
  // Non-selected tabs clip close button before icon.
  return IconCapacity() >= 1;
}

bool Tab2::ShouldShowCloseBox() const {
  // The selected tab never clips close button.
  return model_->IsSelected(const_cast<Tab2*>(this)) || IconCapacity() >= 3;
}

// static
void Tab2::LoadTabImages() {
  // We're not letting people override tab images just yet.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  tab_alpha_.image_l = rb.GetBitmapNamed(IDR_TAB_ALPHA_LEFT);
  tab_alpha_.image_r = rb.GetBitmapNamed(IDR_TAB_ALPHA_RIGHT);

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

  loading_animation_frames = rb.GetBitmapNamed(IDR_THROBBER);
  waiting_animation_frames = rb.GetBitmapNamed(IDR_THROBBER_WAITING);
}
