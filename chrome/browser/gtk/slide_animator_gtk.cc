// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/slide_animator_gtk.h"

#include "base/logging.h"
#include "chrome/common/animation.h"
#include "chrome/common/slide_animation.h"

namespace {

void OnFixedSizeAllocate(GtkWidget* fixed,
                         GtkAllocation* allocation,
                         GtkWidget* child) {
  // The size of the GtkFixed has changed. We want |child_| to match widths,
  // but the height should not change.
  GtkAllocation new_allocation = child->allocation;
  new_allocation.width = allocation->width;
  gtk_widget_size_allocate(child, &new_allocation);
}

}  // namespace

SlideAnimatorGtk::SlideAnimatorGtk(GtkWidget* child,
                                   Direction direction,
                                   int duration,
                                   bool linear,
                                   Delegate* delegate)
    : child_(child),
      direction_(direction),
      delegate_(delegate),
      fixed_needs_resize_(false) {
  widget_.Own(gtk_fixed_new());
  // We need to give the GtkFixed its own window so that painting will clip
  // correctly.
  gtk_fixed_set_has_window(GTK_FIXED(widget_.get()), TRUE);
  gtk_fixed_put(GTK_FIXED(widget_.get()), child, 0, 0);
  gtk_widget_set_size_request(widget_.get(), -1, 0);
  // We have to manually set the size request for |child_| every time the
  // GtkFixed changes sizes.
  g_signal_connect(widget_.get(), "size-allocate",
                   G_CALLBACK(OnFixedSizeAllocate), child_);

  // The size of the GtkFixed widget is set during animation. When we open
  // without showing the animation, we have to call AnimationProgressed
  // ourselves to properly set the size of the GtkFixed. We can't do this until
  // after the child has been allocated, hence we connect to "size-allocate" on
  // the child.
  g_signal_connect(child, "size-allocate",
                   G_CALLBACK(OnChildSizeAllocate), this);

  animation_.reset(new SlideAnimation(this));
  // Default tween type is EASE_OUT.
  if (linear)
    animation_->SetTweenType(SlideAnimation::NONE);
  if (duration != 0)
    animation_->SetSlideDuration(duration);
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
  fixed_needs_resize_ = true;
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

// static
void SlideAnimatorGtk::OnChildSizeAllocate(GtkWidget* child,
                                           GtkAllocation* allocation,
                                           SlideAnimatorGtk* slider) {
  if (!slider->fixed_needs_resize_)
    return;

  slider->fixed_needs_resize_ = false;
  slider->AnimationProgressed(slider->animation_.get());
}
