// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/dragged_tab_gtk.h"

#include <gdk/gdk.h>

#include "app/gfx/canvas.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/gtk/tabs/tab_renderer_gtk.h"
#include "third_party/skia/include/core/SkShader.h"

namespace {

// The size of the dragged window frame.
const int kDragFrameBorderSize = 2;
const int kTwiceDragFrameBorderSize = 2 * kDragFrameBorderSize;

// Used to scale the dragged window sizes.
const float kScalingFactor = 0.5;

const int kAnimateToBoundsDurationMs = 150;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DraggedTabGtk, public:

DraggedTabGtk::DraggedTabGtk(TabContents* datasource,
                             const gfx::Point& mouse_tab_offset,
                             const gfx::Size& contents_size)
    : renderer_(new TabRendererGtk),
      attached_(false),
      mouse_tab_offset_(mouse_tab_offset),
      attached_tab_size_(TabRendererGtk::GetMinimumSelectedSize()),
      contents_size_(contents_size),
      close_animation_(this) {
  renderer_->UpdateData(datasource, false);

  container_ = gtk_window_new(GTK_WINDOW_POPUP);
  SetContainerColorMap();
  gtk_widget_set_app_paintable(container_, TRUE);
  g_signal_connect(G_OBJECT(container_), "expose-event",
                   G_CALLBACK(OnExpose), this);
  gtk_container_add(GTK_CONTAINER(container_), renderer_->widget());
  gtk_widget_show_all(container_);
}

DraggedTabGtk::~DraggedTabGtk() {
  gtk_widget_destroy(container_);
}

void DraggedTabGtk::MoveTo(const gfx::Point& screen_point) {
  int x = screen_point.x() + mouse_tab_offset_.x() -
      ScaleValue(mouse_tab_offset_.x());
  int y = screen_point.y() + mouse_tab_offset_.y() -
      ScaleValue(mouse_tab_offset_.y());

  gtk_window_move(GTK_WINDOW(container_), x, y);
}

void DraggedTabGtk::Attach(int selected_width) {
  attached_ = true;
  attached_tab_size_.set_width(selected_width);
  ResizeContainer();
  Update();
}

void DraggedTabGtk::Update() {
  gtk_widget_queue_draw(container_);
}

void DraggedTabGtk::AnimateToBounds(const gfx::Rect& bounds,
                                    AnimateToBoundsCallback* callback) {
  animation_callback_.reset(callback);

  gint x, y, width, height;
  gdk_window_get_origin(container_->window, &x, &y);
  gdk_window_get_geometry(container_->window, NULL, NULL,
                          &width, &height, NULL);

  animation_start_bounds_ = gfx::Rect(x, y, width, height);
  animation_end_bounds_ = bounds;

  close_animation_.SetSlideDuration(kAnimateToBoundsDurationMs);
  close_animation_.SetTweenType(SlideAnimation::EASE_OUT);
  if (!close_animation_.IsShowing()) {
    close_animation_.Reset();
    close_animation_.Show();
  }
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabGtk, AnimationDelegate implementation:

void DraggedTabGtk::AnimationProgressed(const Animation* animation) {
  int delta_x = (animation_end_bounds_.x() - animation_start_bounds_.x());
  int x = animation_start_bounds_.x() +
      static_cast<int>(delta_x * animation->GetCurrentValue());
  int y = animation_end_bounds_.y();
  gdk_window_move(container_->window, x, y);
}

void DraggedTabGtk::AnimationEnded(const Animation* animation) {
  animation_callback_->Run();
}

void DraggedTabGtk::AnimationCanceled(const Animation* animation) {
  AnimationEnded(animation);
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabGtk, private:

gfx::Size DraggedTabGtk::GetPreferredSize() {
  if (attached_)
    return attached_tab_size_;

  int width = std::max(attached_tab_size_.width(), contents_size_.width()) +
      kTwiceDragFrameBorderSize;
  int height = attached_tab_size_.height() + kDragFrameBorderSize +
      contents_size_.height();
  return gfx::Size(width, height);
}

void DraggedTabGtk::ResizeContainer() {
  gfx::Size size = GetPreferredSize();
  gtk_window_resize(GTK_WINDOW(container_),
                    ScaleValue(size.width()), ScaleValue(size.height()));
  gfx::Rect bounds = renderer_->bounds();
  bounds.set_width(ScaleValue(size.width()));
  bounds.set_height(ScaleValue(size.height()));
  renderer_->SetBounds(bounds);
  Update();
}

int DraggedTabGtk::ScaleValue(int value) {
  return attached_ ? value : static_cast<int>(value * kScalingFactor);
}

void DraggedTabGtk::SetContainerColorMap() {
  GdkScreen* screen = gtk_widget_get_screen(container_);
  GdkColormap* colormap = gdk_screen_get_rgba_colormap(screen);

  // If rgba is not available, use rgb instead.
  if (!colormap)
    colormap = gdk_screen_get_rgb_colormap(screen);

  gtk_widget_set_colormap(container_, colormap);
}

// static
gboolean DraggedTabGtk::OnExpose(GtkWidget* widget, GdkEventExpose* event,
                                 DraggedTabGtk* dragged_tab) {
  cairo_t* cairo_context = gdk_cairo_create(widget->window);
  if (!cairo_context)
      return FALSE;

  // Make the background of the dragged tab window fully transparent.  All of
  // the content of the window (child widgets) will be completely opaque.
  gint width, height;
  gtk_window_get_size(GTK_WINDOW(widget), &width, &height);
  cairo_scale(cairo_context, static_cast<double>(width),
              static_cast<double>(height));
  cairo_set_source_rgba(cairo_context, 1.0f, 1.0f, 1.0f, 0.0f);
  cairo_set_operator(cairo_context, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cairo_context);
  cairo_destroy(cairo_context);

  return FALSE;
}
