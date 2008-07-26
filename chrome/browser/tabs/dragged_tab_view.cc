// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/tabs/dragged_tab_view.h"

#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tabs/hwnd_photobooth.h"
#include "chrome/browser/tabs/tab_renderer.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/views/hwnd_view_container.h"
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

  container_ = new ChromeViews::HWNDViewContainer;
  container_->set_window_style(WS_POPUP);
  container_->set_window_ex_style(
    WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
  container_->set_can_update_layered_window(false);
  container_->Init(NULL, gfx::Rect(0, 0, 0, 0), this, false);
}

DraggedTabView::~DraggedTabView() {
  if (close_animation_.IsAnimating())
    close_animation_.Stop();
  GetParent()->RemoveChildView(this);
  container_->Close();
}

void DraggedTabView::MoveTo(const gfx::Point& screen_point) {
  if (!container_->IsVisible())
    container_->ShowWindow(SW_SHOWNOACTIVATE);

  int x;
  if (UILayoutIsRightToLeft() && !attached_) {
    // On RTL locales, a dragged tab (when it is not attached to a tab strip)
    // is rendered using a right-to-left orientation so we should calculate the
    // window position differently.
    CSize ps;
    GetPreferredSize(&ps);
    x = screen_point.x() - ScaleValue(ps.cx) + mouse_tab_offset_.x() +
        ScaleValue(
            renderer_->MirroredXCoordinateInsideView(mouse_tab_offset_.x()));
  } else {
    x = screen_point.x() + mouse_tab_offset_.x() -
        ScaleValue(mouse_tab_offset_.x());
  }
  int y = screen_point.y() + mouse_tab_offset_.y() -
      ScaleValue(mouse_tab_offset_.y());

  container_->SetWindowPos(NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
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
  container_->PaintNow(CRect());
  container_->set_can_update_layered_window(false);
}

void DraggedTabView::AnimateToBounds(const gfx::Rect& bounds,
                                     Callback0::Type* callback) {
  animation_callback_.reset(callback);

  RECT wr;
  GetWindowRect(GetViewContainer()->GetHWND(), &wr);
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
// DraggedTabView, ChromeViews::View overrides:

void DraggedTabView::Paint(ChromeCanvas* canvas) {
  if (attached_) {
    PaintAttachedTab(canvas);
  } else {
    PaintDetachedView(canvas);
  }
}

void DraggedTabView::Layout() {
  CSize ps;
  GetPreferredSize(&ps);
  if (attached_) {
    renderer_->SetBounds(CRect(0, 0, ps.cx, ps.cy));
  } else {
    int left = 0;
    if (UILayoutIsRightToLeft())
      left = ps.cx - attached_tab_size_.width();
    renderer_->SetBounds(CRect(left, 0, left + attached_tab_size_.width(),
                               attached_tab_size_.height()));
  }
}

void DraggedTabView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  if (attached_) {
    *out = attached_tab_size_.ToSIZE();
  } else {
    int width = std::max(attached_tab_size_.width(), contents_size_.width()) +
        kTwiceDragFrameBorderSize;
    int height = attached_tab_size_.height() + kDragFrameBorderSize +
        contents_size_.height();
    *out = CSize(width, height);
  }
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabView, private:

void DraggedTabView::PaintAttachedTab(ChromeCanvas* canvas) {
  renderer_->ProcessPaint(canvas);
}

void DraggedTabView::PaintDetachedView(ChromeCanvas* canvas) {
  CSize ps;
  GetPreferredSize(&ps);
  ChromeCanvas scale_canvas(ps.cx, ps.cy, false);
  SkBitmap& bitmap_device = const_cast<SkBitmap&>(
      scale_canvas.getTopPlatformDevice().accessBitmap(true));
  bitmap_device.eraseARGB(0, 0, 0, 0);

  scale_canvas.FillRectInt(kDraggedTabBorderColor, 0,
      attached_tab_size_.height() - kDragFrameBorderSize,
      ps.cx, ps.cy - attached_tab_size_.height());
  int image_x = kDragFrameBorderSize;
  int image_y = attached_tab_size_.height();
  int image_w = ps.cx - kTwiceDragFrameBorderSize;
  int image_h =
      ps.cy - kTwiceDragFrameBorderSize - attached_tab_size_.height();
  scale_canvas.FillRectInt(SK_ColorBLACK, image_x, image_y, image_w, image_h);
  photobooth_->PaintScreenshotIntoCanvas(
      &scale_canvas,
      gfx::Rect(image_x, image_y, image_w, image_h));
  renderer_->ProcessPaint(&scale_canvas);

  SkIRect subset;
  subset.set(0, 0, ps.cx, ps.cy);
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
  rc.fRight = SkIntToScalar(ps.cx);
  rc.fBottom = SkIntToScalar(ps.cy);
  canvas->drawRect(rc, paint);
}

void DraggedTabView::ResizeContainer() {
  CSize ps;
  GetPreferredSize(&ps);
  SetWindowPos(container_->GetHWND(), HWND_TOPMOST, 0, 0, ScaleValue(ps.cx),
               ScaleValue(ps.cy), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

int DraggedTabView::ScaleValue(int value) {
  return attached_ ? value : static_cast<int>(value * kScalingFactor);
}
