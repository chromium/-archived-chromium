// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/download_shelf_view.h"

#include <algorithm>

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/views/download_item_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"

// Max number of download views we'll contain. Any time a view is added and
// we already have this many download views, one is removed.
static const size_t kMaxDownloadViews = 15;

// Padding from left edge and first download view.
static const int kLeftPadding = 2;

// Padding from right edge and close button/show downloads link.
static const int kRightPadding = 10;

// Padding between the show all link and close button.
static const int kCloseAndLinkPadding = 14;

// Padding between the download views.
static const int kDownloadPadding = 10;

// Padding between the top/bottom and the content.
static const int kTopBottomPadding = 2;

// Padding between the icon and 'show all downloads' link
static const int kDownloadsTitlePadding = 4;

// Border color.
static const SkColor kBorderColor = SkColorSetRGB(214, 214, 214);

// New download item animation speed in milliseconds.
static const int kNewItemAnimationDurationMs = 800;

// Shelf show/hide speed.
static const int kShelfAnimationDurationMs = 120;

namespace {

// Sets size->width() to view's preferred width + size->width().s
// Sets size->height() to the max of the view's preferred height and
// size->height();
void AdjustSize(views::View* view, gfx::Size* size) {
  gfx::Size view_preferred = view->GetPreferredSize();
  size->Enlarge(view_preferred.width(), 0);
  size->set_height(std::max(view_preferred.height(), size->height()));
}

int CenterPosition(int size, int target_size) {
  return std::max((target_size - size) / 2, kTopBottomPadding);
}

}  // namespace

DownloadShelfView::DownloadShelfView(Browser* browser, BrowserView* parent)
    : DownloadShelf(browser), parent_(parent) {
  parent->AddChildView(this);
  Init();
}

DownloadShelfView::~DownloadShelfView() {
  parent_->RemoveChildView(this);
}

void DownloadShelfView::Init() {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  arrow_image_ = new views::ImageView();
  arrow_image_->SetImage(rb.GetBitmapNamed(IDR_DOWNLOADS_FAVICON));
  AddChildView(arrow_image_);

  show_all_view_ =
      new views::Link(l10n_util::GetString(IDS_SHOW_ALL_DOWNLOADS));
  show_all_view_->SetController(this);
  AddChildView(show_all_view_);

  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  AddChildView(close_button_);

  new_item_animation_.reset(new SlideAnimation(this));
  new_item_animation_->SetSlideDuration(kNewItemAnimationDurationMs);

  shelf_animation_.reset(new SlideAnimation(this));
  shelf_animation_->SetSlideDuration(kShelfAnimationDurationMs);
  Show();
}

void DownloadShelfView::AddDownloadView(View* view) {
  Show();

  DCHECK(view);
  download_views_.push_back(view);
  AddChildView(view);
  if (download_views_.size() > kMaxDownloadViews)
    RemoveDownloadView(*download_views_.begin());

  new_item_animation_->Reset();
  new_item_animation_->Show();
}

void DownloadShelfView::AddDownload(BaseDownloadItemModel* download_model) {
  DownloadItemView* view = new DownloadItemView(
      download_model->download(), this, download_model);
  AddDownloadView(view);
}

void DownloadShelfView::RemoveDownloadView(View* view) {
  DCHECK(view);
  std::vector<View*>::iterator i =
    find(download_views_.begin(), download_views_.end(), view);
  DCHECK(i != download_views_.end());
  download_views_.erase(i);
  RemoveChildView(view);
  delete view;
  if (download_views_.empty())
    Close();
  Layout();
  SchedulePaint();
}

void DownloadShelfView::Paint(gfx::Canvas* canvas) {
  PaintBackground(canvas);
  PaintBorder(canvas);
}

void DownloadShelfView::PaintBorder(gfx::Canvas* canvas) {
  canvas->FillRectInt(kBorderColor, 0, 0, width(), 1);
}

gfx::Size DownloadShelfView::GetPreferredSize() {
  gfx::Size prefsize(kRightPadding + kLeftPadding + kCloseAndLinkPadding, 0);
  AdjustSize(close_button_, &prefsize);
  AdjustSize(show_all_view_, &prefsize);
  // Add one download view to the preferred size.
  if (download_views_.size() > 0) {
    AdjustSize(*download_views_.begin(), &prefsize);
    prefsize.Enlarge(kDownloadPadding, 0);
  }
  prefsize.Enlarge(0, kTopBottomPadding + kTopBottomPadding);
  if (shelf_animation_->IsAnimating()) {
    prefsize.set_height(static_cast<int>(
        static_cast<double>(prefsize.height()) *
                            shelf_animation_->GetCurrentValue()));
  }
  return prefsize;
}

void DownloadShelfView::AnimationProgressed(const Animation *animation) {
  if (animation == new_item_animation_.get()) {
    Layout();
    SchedulePaint();
  } else if (animation == shelf_animation_.get()) {
    // Force a re-layout of the parent, which will call back into
    // GetPreferredSize, where we will do our animation. In the case where the
    // animation is hiding, we do a full resize - the fast resizing would
    // otherwise leave blank white areas where the shelf was and where the
    // user's eye is. Thankfully bottom-resizing is a lot faster than
    // top-resizing.
    parent_->SelectedTabToolbarSizeChanged(shelf_animation_->IsShowing());
  }
}

void DownloadShelfView::AnimationEnded(const Animation *animation) {
  if (animation == shelf_animation_.get())
    parent_->SetDownloadShelfVisible(shelf_animation_->IsShowing());
}

void DownloadShelfView::Layout() {
  // Now that we know we have a parent, we can safely set our theme colors.
  show_all_view_->SetColor(
      GetThemeProvider()->GetColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT));
  set_background(views::Background::CreateSolidBackground(
      GetThemeProvider()->GetColor(BrowserThemeProvider::COLOR_TOOLBAR)));

  // Let our base class layout our child views
  views::View::Layout();

  // If there is not enought room to show the first download item, show the
  // "Show all downloads" link to the left to make it more visible that there is
  // something to see.
  bool show_link_only = !CanFitFirstDownloadItem();

  gfx::Size image_size = arrow_image_->GetPreferredSize();
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  gfx::Size show_all_size = show_all_view_->GetPreferredSize();
  int max_download_x =
      std::max<int>(0, width() - kRightPadding - close_button_size.width() -
                       kCloseAndLinkPadding - show_all_size.width() -
                       kDownloadsTitlePadding - image_size.width() -
                       kDownloadPadding);
  int next_x = show_link_only ? kLeftPadding :
                                max_download_x + kDownloadPadding;
  // Align vertically with show_all_view_.
  arrow_image_->SetBounds(next_x,
                          CenterPosition(show_all_size.height(), height()),
                          image_size.width(), image_size.height());
  next_x += image_size.width() + kDownloadsTitlePadding;
  show_all_view_->SetBounds(next_x,
                            CenterPosition(show_all_size.height(), height()),
                            show_all_size.width(),
                            show_all_size.height());
  next_x += show_all_size.width() + kCloseAndLinkPadding;
  close_button_->SetBounds(next_x,
                           CenterPosition(close_button_size.height(), height()),
                           close_button_size.width(),
                           close_button_size.height());
  if (show_link_only) {
    // Let's hide all the items.
    std::vector<View*>::reverse_iterator ri;
    for (ri = download_views_.rbegin(); ri != download_views_.rend(); ++ri)
      (*ri)->SetVisible(false);
    return;
  }

  next_x = kLeftPadding;
  std::vector<View*>::reverse_iterator ri;
  for (ri = download_views_.rbegin(); ri != download_views_.rend(); ++ri) {
    gfx::Size view_size = (*ri)->GetPreferredSize();

    int x = next_x;

    // Figure out width of item.
    int item_width = view_size.width();
    if (new_item_animation_->IsAnimating() && ri == download_views_.rbegin()) {
       item_width = static_cast<int>(static_cast<double>(view_size.width()) *
                     new_item_animation_->GetCurrentValue());
    }

    next_x += item_width;

    // Make sure our item can be contained within the shelf.
    if (next_x < max_download_x) {
      (*ri)->SetVisible(true);
      (*ri)->SetBounds(x, CenterPosition(view_size.height(), height()),
                       item_width, view_size.height());
    } else {
      (*ri)->SetVisible(false);
    }
  }
}

bool DownloadShelfView::CanFitFirstDownloadItem() {
  if (download_views_.empty())
    return true;

  gfx::Size image_size = arrow_image_->GetPreferredSize();
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  gfx::Size show_all_size = show_all_view_->GetPreferredSize();

  // Let's compute the width available for download items, which is the width
  // of the shelf minus the "Show all downloads" link, arrow and close button
  // and the padding.
  int available_width = width() - kRightPadding - close_button_size.width() -
      kCloseAndLinkPadding - show_all_size.width() - kDownloadsTitlePadding -
      image_size.width() - kDownloadPadding - kLeftPadding;
  if (available_width <= 0)
    return false;

  gfx::Size item_size = (*download_views_.rbegin())->GetPreferredSize();
  return item_size.width() < available_width;
}

void DownloadShelfView::LinkActivated(views::Link* source, int event_flags) {
  ShowAllDownloads();
}

void DownloadShelfView::ButtonPressed(views::Button* button) {
  Close();
}

bool DownloadShelfView::IsShowing() const {
  return shelf_animation_->IsShowing();
}

bool DownloadShelfView::IsClosing() const {
  // TODO(estade): This is never called. For now just return false.
  return false;
}

void DownloadShelfView::Show() {
  shelf_animation_->Show();
}

void DownloadShelfView::Close() {
  parent_->SetDownloadShelfVisible(false);
  shelf_animation_->Hide();
}
