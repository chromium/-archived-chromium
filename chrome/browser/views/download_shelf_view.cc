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

#include "chrome/browser/views/download_shelf_view.h"

#include <algorithm>

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/download_item_model.h"
#include "chrome/browser/download_manager.h"
#include "chrome/browser/download_tab_view.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/views/download_item_view.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "base/logging.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/background.h"
#include "chrome/views/button.h"
#include "chrome/views/image_view.h"

#include "generated_resources.h"

// Max number of download views we'll contain. Any time a view is added and
// we already have this many download views, one is removed.
static const int kMaxDownloadViews = 15;

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

// Default background color for the shelf.
static const SkColor kBackgroundColor = SkColorSetRGB(230, 237, 244);

// Border color.
static const SkColor kBorderColor = SkColorSetRGB(214, 214, 214);

// New download item animation speed in milliseconds.
static const int kNewItemAnimationDurationMs = 800;

// Shelf show/hide speed.
static const int kShelfAnimationDurationMs = 120;

namespace {

// Sets size->cx to view's preferred width + size->cx.
// Sets size->cy to the max of the view's preferred height and size->cy;
void AdjustSize(ChromeViews::View* view, CSize* size) {
  CSize view_preferred;
  view->GetPreferredSize(&view_preferred);
  size->cx += view_preferred.cx;
  size->cy = std::max(view_preferred.cy, size->cy);
}

int CenterPosition(int size, int target_size) {
  return std::max((target_size - size) / 2, kTopBottomPadding);
}

} // namespace

DownloadShelfView::DownloadShelfView(TabContents* tab_contents)
    : tab_contents_(tab_contents) {
  Init();
}

void DownloadShelfView::Init() {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  arrow_image_ = new ChromeViews::ImageView();
  arrow_image_->SetImage(rb.GetBitmapNamed(IDR_DOWNLOADS_FAVICON));
  AddChildView(arrow_image_);

  show_all_view_ =
      new ChromeViews::Link(l10n_util::GetString(IDS_SHOW_ALL_DOWNLOADS));
  show_all_view_->SetController(this);
  AddChildView(show_all_view_);

  close_button_ = new ChromeViews::Button();
  close_button_->SetImage(ChromeViews::Button::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(ChromeViews::Button::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(ChromeViews::Button::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetListener(this, 0);
  AddChildView(close_button_);
  SetBackground(
      ChromeViews::Background::CreateSolidBackground(kBackgroundColor));

  new_item_animation_.reset(new SlideAnimation(this));
  new_item_animation_->SetSlideDuration(kNewItemAnimationDurationMs);

  shelf_animation_.reset(new SlideAnimation(this));
  shelf_animation_->SetSlideDuration(kShelfAnimationDurationMs);
  shelf_animation_->Show();
}

void DownloadShelfView::AddDownloadView(View* view) {
  DCHECK(view);
  download_views_.push_back(view);
  AddChildView(view);
  if (download_views_.size() > kMaxDownloadViews)
    RemoveDownloadView(*download_views_.begin());

  new_item_animation_->Reset();
  new_item_animation_->Show();
}

void DownloadShelfView::ChangeTabContents(TabContents* old_contents,
                                          TabContents* new_contents) {
  DCHECK(old_contents == tab_contents_);
  tab_contents_ = new_contents;
}

void DownloadShelfView::AddDownload(DownloadItem* download) {
  shelf_animation_->Show();

  DownloadItemView* view = new DownloadItemView(
      download, this, new DownloadItemModel(download));
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
    tab_contents_->SetDownloadShelfVisible(false);
  Layout();
  SchedulePaint();
}

void DownloadShelfView::Paint(ChromeCanvas* canvas) {
  PaintBackground(canvas);
  PaintBorder(canvas);
}

void DownloadShelfView::PaintBorder(ChromeCanvas* canvas) {
  canvas->FillRectInt(kBorderColor, 0, 0, GetWidth(), 1);
}

void DownloadShelfView::GetPreferredSize(CSize *out) {
  out->cx = kRightPadding + kLeftPadding + kCloseAndLinkPadding;
  out->cy = 0;
  AdjustSize(close_button_, out);
  AdjustSize(show_all_view_, out);
  // Add one download view to the preferred size.
  if (download_views_.size() > 0) {
    AdjustSize(*download_views_.begin(), out);
    out->cx += kDownloadPadding;
  }
  out->cy += kTopBottomPadding + kTopBottomPadding;
  if (shelf_animation_->IsAnimating()) {
    out->cy = static_cast<int>(static_cast<double>(out->cy) *
              shelf_animation_->GetCurrentValue());
  }
}

void DownloadShelfView::DidChangeBounds(const CRect& previous,
                                        const CRect& current) {
  Layout();
}

void DownloadShelfView::AnimationProgressed(const Animation *animation) {
  if (animation == new_item_animation_.get()) {
    Layout();
    SchedulePaint();
  } else if (animation == shelf_animation_.get()) {
    // Force a re-layout of the parent, which will call back into
    // GetPreferredSize, where we will do our animation. In the  case where the
    // animation is hiding, we do a full resize - the fast resizing would
    // otherwise leave blank white areas where the shelf was and where the
    // user's eye is. Thankfully bottom-resizing is a lot faster than
    // top-resizing.
    tab_contents_->ToolbarSizeChanged(shelf_animation_->IsShowing());
  }
}

void DownloadShelfView::AnimationEnded(const Animation *animation) {
  if (animation == shelf_animation_.get()) {
    if (shelf_animation_->IsShowing() == false)
      tab_contents_->SetDownloadShelfVisible(false);
    tab_contents_->ToolbarSizeChanged(false);
  }
}

void DownloadShelfView::Layout() {
  int width = GetWidth();
  int height = GetHeight();
  CSize image_size;
  arrow_image_->GetPreferredSize(&image_size);
  CSize close_button_size;
  close_button_->GetPreferredSize(&close_button_size);
  CSize show_all_size;
  show_all_view_->GetPreferredSize(&show_all_size);
  int max_download_x =
      std::max<int>(0, width - kRightPadding - close_button_size.cx -
                       kCloseAndLinkPadding - show_all_size.cx -
                       image_size.cx - kDownloadPadding);
  int next_x = max_download_x + kDownloadPadding;
  // Align vertically with show_all_view_.
  arrow_image_->SetBounds(next_x, CenterPosition(show_all_size.cy, height),
                          image_size.cx, image_size.cy);
  next_x += image_size.cx + kDownloadsTitlePadding;
  show_all_view_->SetBounds(next_x,
                            CenterPosition(show_all_size.cy, height),
                            show_all_size.cx,
                            show_all_size.cy);
  next_x += show_all_size.cx + kCloseAndLinkPadding;
  close_button_->SetBounds(next_x,
                           CenterPosition(close_button_size.cy, height),
                           close_button_size.cx,
                           close_button_size.cy);

  next_x = kLeftPadding;
  std::vector<View*>::reverse_iterator ri;
  for (ri = download_views_.rbegin(); ri != download_views_.rend(); ++ri) {
    CSize view_size;
    (*ri)->GetPreferredSize(&view_size);

    int x = next_x;

    // Figure out width of item.
    int width = view_size.cx;
    if (new_item_animation_->IsAnimating() && ri == download_views_.rbegin()) {
       width = static_cast<int>(static_cast<double>(view_size.cx) *
                     new_item_animation_->GetCurrentValue());
    }

    next_x += (width + kDownloadPadding);

    // Make sure our item can be contained within the shelf.
    if (next_x < max_download_x) {
      (*ri)->SetVisible(true);
      (*ri)->SetBounds(x, CenterPosition(view_size.cy, height), width,
                       view_size.cy);
    } else {
      (*ri)->SetVisible(false);
    }
  }
}

// Open the download page.
void DownloadShelfView::LinkActivated(ChromeViews::Link* source,
                                      int event_flags) {
  int index;
  NavigationController* controller = tab_contents_->controller();
  Browser* browser = Browser::GetBrowserForController(controller, &index);
  DCHECK(browser);
  browser->ShowNativeUI(DownloadTabUI::GetURL());
}

void DownloadShelfView::ButtonPressed(ChromeViews::BaseButton* button) {
  shelf_animation_->Hide();
}