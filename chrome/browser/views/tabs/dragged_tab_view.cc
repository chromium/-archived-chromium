// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/dragged_tab_view.h"

#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/tabs/hwnd_photobooth.h"
#include "chrome/browser/views/tabs/tab_renderer.h"
#include "chrome/views/widget_win.h"
#include "skia/include/SkShader.h"

const int kTransparentAlpha = 200;
const int kOpaqueAlpha = 255;
const int kDragFrameBorderSize = 2;
const int kTwiceDragFrameBorderSize = 2 * kDragFrameBorderSize;
const float kScalingFactor = 0.5;
const int kAnimateToBoundsDurationMs = 150;
static const SkColor kDraggedTabBorderColor = SkColorSetRGB(103, 129, 162);

////////////////////////////////////////////////////////////////////////////////
// DraggedTabView, public:

DraggedTabView::DraggedTabView(TabContents* datasource,
                               const gfx::Point& mouse_tab_offset,
                               const gfx::Size& contents_size)
    : container_(NULL),
      renderer_(new TabRenderer),
      attached_(false),
      mouse_tab_offset_(mouse_tab_offset),
      attached_tab_size_(TabRenderer::GetMinimumSelectedSize()),
      photobooth_(NULL),
      contents_size_(contents_size),
      close_animation_(this) {
  SetParentOwned(false);

  renderer_->UpdateData(datasource);

  container_.reset(new views::WidgetWin);
  container_->set_delete_on_destroy(false);
  container_->set_window_style(WS_POPUP);
  container_->set_window_ex_style(
    WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
  container_->set_can_update_layered_window(false);
  container_->Init(NULL, gfx::Rect(0, 0, 0, 0), false);
  container_->SetContentsView(this);
}

DraggedTabView::~DraggedTabView() {
  if (close_animation_.IsAnimating())
    close_animation_.Stop();
  GetParent()->RemoveChildView(this);
  container_->CloseNow();
}

void DraggedTabView::MoveTo(const gfx::Point& screen_point) {
  int show_flags = container_->IsVisible() ? SWP_NOZORDER : SWP_SHOWWINDOW;

  int x;
  if (UILayoutIsRightToLeft() && !attached_) {
    // On RTL locales, a dragged tab (when it is not attached to a tab strip)
    // is rendered using a right-to-left orientation so we should calculate the
    // window position differently.
    gfx::Size ps = GetPreferredSize();
    x = screen_point.x() - ScaleValue(ps.width()) + mouse_tab_offset_.x() +
        ScaleValue(
            renderer_->MirroredXCoordinateInsideView(mouse_tab_offset_.x()));
  } else {
    x = screen_point.x() + mouse_tab_offset_.x() -
        ScaleValue(mouse_tab_offset_.x());
  }
  int y = screen_point.y() + mouse_tab_offset_.y() -
      ScaleValue(mouse_tab_offset_.y());

  container_->SetWindowPos(HWND_TOP, x, y, 0, 0,
                           SWP_NOSIZE | SWP_NOACTIVATE | show_flags);
}

void DraggedTabView::Attach(int selected_width) {
  attached_ = true;
  photobooth_ = NULL;
  attached_tab_size_.set_width(selected_width);
  container_->SetLayeredAlpha(kOpaqueAlpha);
  ResizeContainer();
  Update();
}

void DraggedTabView::Detach(HWNDPhotobooth* photobooth) {
  attached_ = false;
  photobooth_ = photobooth;
  container_->SetLayeredAlpha(kTransparentAlpha);
  ResizeContainer();
  Update();
}

void DraggedTabView::Update() {
  container_->set_can_update_layered_window(true);
  SchedulePaint();
  container_->PaintNow(gfx::Rect());
  container_->set_can_update_layered_window(false);
}

void DraggedTabView::AnimateToBounds(const gfx::Rect& bounds,
                                     Callback0::Type* callback) {
  animation_callback_.reset(callback);

  RECT wr;
  GetWindowRect(GetWidget()->GetHWND(), &wr);
  animation_start_bounds_ = wr;
  animation_end_bounds_ = bounds;

  close_animation_.SetSlideDuration(kAnimateToBoundsDurationMs);
  close_animation_.SetTweenType(SlideAnimation::EASE_OUT);
  if (!close_animation_.IsShowing()) {
    close_animation_.Reset();
    close_animation_.Show();
  }
}

///////////////////////////////////////////////////////////////////////////////
// DraggedTabView, AnimationDelegate implementation:

void DraggedTabView::AnimationProgressed(const Animation* animation) {
  int delta_x = (animation_end_bounds_.x() - animation_start_bounds_.x());
  int x = animation_start_bounds_.x() +
      static_cast<int>(delta_x * animation->GetCurrentValue());
  int y = animation_end_bounds_.y();
  container_->SetWindowPos(NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

void DraggedTabView::AnimationEnded(const Animation* animation) {
  animation_callback_->Run();
}

void DraggedTabView::AnimationCanceled(const Animation* animation) {
  AnimationEnded(animation);
}

///////////////////////////////////////////////////////////////////////////////
// DraggedTabView, views::View overrides:

void DraggedTabView::Paint(ChromeCanvas* canvas) {
  if (attached_) {
    PaintAttachedTab(canvas);
  } else {
    PaintDetachedView(canvas);
  }
}

void DraggedTabView::Layout() {
  if (attached_) {
    gfx::Size prefsize = GetPreferredSize();
    renderer_->SetBounds(0, 0, prefsize.width(), prefsize.height());
  } else {
    int left = 0;
    if (UILayoutIsRightToLeft())
      left = GetPreferredSize().width() - attached_tab_size_.width();
    renderer_->SetBounds(left, 0, left + attached_tab_size_.width(),
                         attached_tab_size_.height());
  }
}

gfx::Size DraggedTabView::GetPreferredSize() {
  if (attached_)
    return attached_tab_size_;

  int width = std::max(attached_tab_size_.width(), contents_size_.width()) +
      kTwiceDragFrameBorderSize;
  int height = attached_tab_size_.height() + kDragFrameBorderSize +
      contents_size_.height();
  return gfx::Size(width, height);
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabView, private:

void DraggedTabView::PaintAttachedTab(ChromeCanvas* canvas) {
  renderer_->ProcessPaint(canvas);
}

void DraggedTabView::PaintDetachedView(ChromeCanvas* canvas) {
  gfx::Size ps = GetPreferredSize();
  ChromeCanvas scale_canvas(ps.width(), ps.height(), false);
  SkBitmap& bitmap_device = const_cast<SkBitmap&>(
      scale_canvas.getTopPlatformDevice().accessBitmap(true));
  bitmap_device.eraseARGB(0, 0, 0, 0);

  scale_canvas.FillRectInt(kDraggedTabBorderColor, 0,
      attached_tab_size_.height() - kDragFrameBorderSize,
      ps.width(), ps.height() - attached_tab_size_.height());
  int image_x = kDragFrameBorderSize;
  int image_y = attached_tab_size_.height();
  int image_w = ps.width() - kTwiceDragFrameBorderSize;
  int image_h =
      ps.height() - kTwiceDragFrameBorderSize - attached_tab_size_.height();
  scale_canvas.FillRectInt(SK_ColorBLACK, image_x, image_y, image_w, image_h);
  photobooth_->PaintScreenshotIntoCanvas(
      &scale_canvas,
      gfx::Rect(image_x, image_y, image_w, image_h));
  renderer_->ProcessPaint(&scale_canvas);

  SkIRect subset;
  subset.set(0, 0, ps.width(), ps.height());
  SkBitmap mipmap = scale_canvas.ExtractBitmap();
  mipmap.buildMipMap(true);

  SkShader* bitmap_shader =
      SkShader::CreateBitmapShader(mipmap, SkShader::kClamp_TileMode,
                                   SkShader::kClamp_TileMode);

  SkMatrix shader_scale;
  shader_scale.setScale(kScalingFactor, kScalingFactor);
  bitmap_shader->setLocalMatrix(shader_scale);

  SkPaint paint;
  paint.setShader(bitmap_shader);
  paint.setAntiAlias(true);
  bitmap_shader->unref();

  SkRect rc;
  rc.fLeft = 0;
  rc.fTop = 0;
  rc.fRight = SkIntToScalar(ps.width());
  rc.fBottom = SkIntToScalar(ps.height());
  canvas->drawRect(rc, paint);
}

void DraggedTabView::ResizeContainer() {
  gfx::Size ps = GetPreferredSize();
  SetWindowPos(container_->GetHWND(), HWND_TOPMOST, 0, 0,
               ScaleValue(ps.width()), ScaleValue(ps.height()),
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

int DraggedTabView::ScaleValue(int value) {
  return attached_ ? value : static_cast<int>(value * kScalingFactor);
}

