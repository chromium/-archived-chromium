// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/slide_animator_gtk.h"

#include "app/animation.h"
#include "app/slide_animation.h"
#include "base/logging.h"

namespace {

void OnFixedSizeAllocate(GtkWidget* fixed,
                         GtkAllocation* allocation,
                         GtkWidget* child) {
  // The size of the GtkFixed has changed. We want |child_| to match widths,
  // but the height should not change.
  gtk_widget_set_size_request(child, allocation->width, -1);
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

  child_needs_move_ = (direction == DOWN);

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
  gtk_widget_show(widget_.get());
  animation_->Show();
}

void SlideAnimatorGtk::OpenWithoutAnimation() {
  animation_->Reset(1.0);
  Open();

  // This checks to see if |child_| has been allocated yet. If it has been
  // allocated already, we can go ahead and reposition everything by calling
  // AnimationProgressed(). If it has not been allocated, we have to delay
  // this call until it has been allocated (see OnChildSizeAllocate).
  if (child_->allocation.x != -1) {
    AnimationProgressed(animation_.get());
  } else {
    fixed_needs_resize_ = true;
  }
}

void SlideAnimatorGtk::Close() {
  animation_->Hide();
}

void SlideAnimatorGtk::CloseWithoutAnimation() {
  animation_->Reset(0.0);
  animation_->Hide();
  AnimationProgressed(animation_.get());
  gtk_widget_hide(widget_.get());
}

bool SlideAnimatorGtk::IsShowing() {
  return animation_->IsShowing();
}

bool SlideAnimatorGtk::IsClosing() {
  return animation_->IsClosing();
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
  if (!animation_->IsShowing()) {
    gtk_widget_hide(widget_.get());
    if (delegate_)
      delegate_->Closed();
  }
}

// static
void SlideAnimatorGtk::OnChildSizeAllocate(GtkWidget* child,
                                           GtkAllocation* allocation,
                                           SlideAnimatorGtk* slider) {
  if (slider->child_needs_move_) {
    gtk_fixed_move(GTK_FIXED(slider->widget()), child, 0, -allocation->height);
    slider->child_needs_move_ = false;
  }

  if (slider->fixed_needs_resize_) {
    slider->AnimationProgressed(slider->animation_.get());
    slider->fixed_needs_resize_ = false;
  }
}
