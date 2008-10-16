// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/views/info_bar_item_view.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/container.h"
#include "chrome/views/external_focus_tracker.h"
#include "chrome/views/image_view.h"
#include "chrome/views/root_view.h"

#include "generated_resources.h"

namespace {

class HorizontalSpacer : public views::View {
 public:
  explicit HorizontalSpacer(int width) : width_(width) {}

  gfx::Size GetPreferredSize() {
    return gfx::Size(width_, 0);
  }

 private:
  int width_;
};

const int kInfoBarVerticalSpacing = 3;
const int kInfoBarLeftMargin = 3;
const double kInfoBarHeight = 37.0;

} // namespace

InfoBarItemView::InfoBarItemView()
    : insert_index_(0),
      close_button_(NULL),
      icon_(NULL) {
  Init();
}

InfoBarItemView::~InfoBarItemView() {
}

// static
int InfoBarItemView::CenterPosition(int size, int target_size) {
  return (target_size - size) / 2;
}

gfx::Size InfoBarItemView::GetPreferredSize() {
  return gfx::Size(
      GetParent()->width(),
      static_cast<int>(kInfoBarHeight * animation_->GetCurrentValue()));
}

// The following is an overall note on the underlying implementation. You don't
// need this in order to use this view. Ignore unless you're editing
// implementation:
// Layout() lays out all of its child views, but it uses insert_index_ to
// decide whether to lay out on the left or right. Whenever a view is added or
// removed the insert_index_ is updated accordingly to make sure it is directly
// between left aligned views and right aligned views. Whenever a view is added,
// a spacer view provides padding to the right of the view if the view is
// left aligned, or to the left of the view if the view is right aligned.
// Removing assumes this spacer view exists.
//
// For example, below M stands for built in margins, I stands for the icon
// which is optional and includes padding of its own. L stands for a left
// aligned view, and R for a right aligned view. P is padding, which can be
// zero. The insert index is currently 4, separating the right of left views.
// The numbers represent what index the child views P, R, and L occupy.
//
// M I L P L P P R P R M
//     0 1 2 3 ^4 5 6 7
// Say we call AddChildViewTrailing(right_view, 10). We end up with:
// M I L P L P P R P R P R M
//     0 1 2 3 ^4 5 6 7 8 9
// First the right view was added, then its padding was added, the insert index
// did not need to change because it still separates the right and left views.
// Note that the padding showed up at the lower index, or to the left of the
// right aligned view.
// Then we call AddChildViewLeading(left_view, 0). We end up with:
// M I L P L P L P P R P R P R M
//     0 1 2 3 4 5 ^6 7 8 9 10 11
// First the left view was added, then the insert_index_ was incremented, then
// the padding is added, even though it is zero (It has no effect on layout)
// and insert_index_ is incremented again to keep it between the right and
// left views. Note in this case, the padding appears to the right of the view
// left aligned view. Removing works the same, but in reverse.
void InfoBarItemView::Layout() {
  int next_x = width() - kButtonHEdgeMargin;
  int height_diff = static_cast<int>(kInfoBarHeight) - height();
  const int child_count = GetChildViewCount();
  // Anything greater than or equal to insert_index_ is laid out on the right,
  // with the greatest index (the first one added to the right) being laid out
  // rightmost.
  for (int i = child_count - 1; i >= insert_index_ ; i--) {
    View* v = GetChildViewAt(i);
    if (v->IsVisible()) {
      gfx::Size view_size = v->GetPreferredSize();
      next_x = next_x - view_size.width();
      v->SetBounds(next_x,
                   CenterPosition(view_size.height(),
                       static_cast<int>(kInfoBarHeight)) - height_diff,
                   view_size.width(),
                   view_size.height());
    }
  }
  int left_most_x = next_x;

  next_x = kInfoBarLeftMargin;

  // Anything less than insert_index_ is laid out on the left, with the
  // smallest index (the first one added to the left) being laid out leftmost.
  for (int i = 0; i < insert_index_ ; i++) {
    View* v = GetChildViewAt(i);
    if (v->IsVisible()) {
      gfx::Size view_size = v->GetPreferredSize();
      int remaining_space = std::max(0, left_most_x - next_x);
      if (view_size.width() > remaining_space) {
        view_size.set_width(remaining_space);
      }
      v->SetBounds(next_x,
                   CenterPosition(view_size.height(),
                       static_cast<int>(kInfoBarHeight)) - height_diff,
                   view_size.width(),
                   view_size.height());
      next_x = next_x + view_size.width();
    }
  }
}

void InfoBarItemView::DidChangeBounds(const gfx::Rect& previous,
                                      const gfx::Rect& current) {
  if (GetParent() != NULL)
    Layout();
}

void InfoBarItemView::BeginClose() {
  animation_->Hide();
}

void InfoBarItemView::Close() {
  views::View* parent = GetParent();
  parent->RemoveChildView(this);
  if (focus_tracker_.get() != NULL)
    focus_tracker_->FocusLastFocusedExternalView();
  delete this;
}

void InfoBarItemView::CloseButtonPressed() {
  // Close this view by default.
  BeginClose();
}

void InfoBarItemView::AddChildViewTrailing(views::View* view,
                                          int leading_padding) {
  views::View::AddChildView(insert_index_, view);
  View* padding = new HorizontalSpacer(leading_padding);
  views::View::AddChildView(insert_index_, padding);
}

void InfoBarItemView::AddChildViewTrailing(views::View* view) {
  AddChildViewTrailing(view, kUnrelatedControlHorizontalSpacing);
}

void InfoBarItemView::AddChildViewLeading(views::View* view,
                                         int trailing_padding) {
  views::View::AddChildView(insert_index_, view);
  insert_index_++;
  View* padding = new HorizontalSpacer(trailing_padding);
  views::View::AddChildView(insert_index_, padding);
  insert_index_++;
}

void InfoBarItemView::AddChildViewLeading(views::View* view) {
  AddChildViewLeading(view, kRelatedControlSmallHorizontalSpacing);
}

void InfoBarItemView::SetIcon(const SkBitmap& icon) {
  if (icon_ == NULL) {
    // Add the icon and its padding to the far left of the info bar, and adjust
    // the insert index accordingly.
    icon_ = new views::ImageView();
    View* padding = new HorizontalSpacer(kRelatedControlHorizontalSpacing);
    views::View::AddChildView(0, padding);
    views::View::AddChildView(0, icon_);
    insert_index_ += 2;
  }
  icon_->SetImage(icon);
  Layout();
}

void InfoBarItemView::ViewHierarchyChanged(bool is_add,
                                           View *parent,
                                           View *child) {
  if (child == this) {
    if (is_add) {
      Layout();

      View* root_view = GetRootView();
      HWND root_hwnd = NULL;
      if (root_view)
        root_hwnd = root_view->GetContainer()->GetHWND();

      if (root_hwnd) {
        focus_tracker_.reset(new views::ExternalFocusTracker(
            this, views::FocusManager::GetFocusManager(root_hwnd)));
      }
    } else {
      // When we're removed from the hierarchy our focus manager is no longer
      // valid.
      if (focus_tracker_.get() != NULL)
        focus_tracker_->SetFocusManager(NULL);
    }
  }
}

void InfoBarItemView::AddChildView(views::View* view) {
  AddChildViewTrailing(view, kUnrelatedControlHorizontalSpacing);
}

void InfoBarItemView::AddChildView(int index, views::View* view) {
  if (index < insert_index_)
    AddChildViewLeading(view);
  else
    AddChildViewTrailing(view);
}

void InfoBarItemView::RemoveChildView(views::View* view) {
  int index = GetChildIndex(view);
  if (index >= 0) {
    if (index < insert_index_) {
      // We're removing a leading view. So the view at index + 1 (immediately
      // trailing) is the corresponding spacer view.
      View* spacer_view = GetChildViewAt(index + 1);
      views::View::RemoveChildView(view);
      views::View::RemoveChildView(spacer_view);
      delete spacer_view;
      // Need to change the insert_index_ so it is still pointing at the
      // "middle" index between left and right aligned views.
      insert_index_ -= 2;
    } else {
      // We're removing a trailing view. So the view at index - 1 (immediately
      // leading) is the corresponding spacer view.
      View* spacer_view = GetChildViewAt(index - 1);
      views::View::RemoveChildView(view);
      views::View::RemoveChildView(spacer_view);
      delete spacer_view;
    }
  }
}

void InfoBarItemView::ButtonPressed(views::BaseButton* button) {
  if (button == close_button_)
    CloseButtonPressed();
}

void InfoBarItemView::AnimationProgressed(const Animation* animation) {
  static_cast<InfoBarView*>(GetParent())->ChildAnimationProgressed();
}

void InfoBarItemView::AnimationEnded(const Animation* animation) {
  static_cast<InfoBarView*>(GetParent())->ChildAnimationEnded();

  if (!animation_->IsShowing())
    Close();
}

void InfoBarItemView::Init() {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  close_button_ = new views::Button();
  close_button_->SetImage(views::Button::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::Button::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::Button::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetListener(this, 0);
  close_button_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_CLOSE));
  AddChildViewTrailing(close_button_);

  animation_.reset(new SlideAnimation(this));
  animation_->SetTweenType(SlideAnimation::NONE);
  animation_->Show();
}

