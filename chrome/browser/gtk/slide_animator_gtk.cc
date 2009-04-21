// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/slide_animator_gtk.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "chrome/common/animation.h"
#include "chrome/common/slide_animation.h"

namespace {

void OnSizeAllocate(GtkWidget* fixed,
                    GtkAllocation* allocation,
                    GtkWidget* child) {
  gint height;
  gtk_widget_get_size_request(child, NULL, &height);
  // The size of the GtkFixed has changed. We want |child_| to match widths,
  // but the height should always be |widget_height_|.
  gtk_widget_set_size_request(child, allocation->width, height);
}

}  // namespace

SlideAnimatorGtk::SlideAnimatorGtk(GtkWidget* child,
                                   Direction direction,
                                   Delegate* delegate)
    : child_(child),
      direction_(direction),
      delegate_(delegate) {
  widget_.Own(gtk_fixed_new());
  // We need to give the GtkFixed its own window so that painting will clip
  // correctly.
  gtk_fixed_set_has_window(GTK_FIXED(widget_.get()), TRUE);
  gtk_fixed_put(GTK_FIXED(widget_.get()), child, 0, 0);
  gtk_widget_set_size_request(widget_.get(), -1, 0);
  // We have to manually set the size request for |child_| every time the
  // GtkFixed changes sizes.
  g_signal_connect(widget_.get(), "size-allocate",
                   G_CALLBACK(OnSizeAllocate), child_);

  animation_.reset(new SlideAnimation(this));
  animation_->SetTweenType(SlideAnimation::NONE);
}

SlideAnimatorGtk::~SlideAnimatorGtk() {
  widget_.Destroy();
}

void SlideAnimatorGtk::Open() {
  gtk_widget_show_all(widget_.get());
  animation_->Show();
}

void SlideAnimatorGtk::OpenWithoutAnimation() {
  animation_->Reset(1.0);
  Open();
}

void SlideAnimatorGtk::Close() {
  animation_->Hide();
}

bool SlideAnimatorGtk::IsShowing() {
  return animation_->IsShowing();
}

void SlideAnimatorGtk::AnimationProgressed(const Animation* animation) {
  int showing_height = child_->allocation.height *
                       animation_->GetCurrentValue();
  if (direction_ == DOWN) {
    gtk_fixed_move(GTK_FIXED(widget_.get()), child_, 0,
                   showing_height - child_->allocation.height);
  }
  gtk_widget_set_size_request(widget_.get(), -1, showing_height);
}

void SlideAnimatorGtk::AnimationEnded(const Animation* animation) {
  if (!animation_->IsShowing() && delegate_)
    delegate_->Closed();
}
