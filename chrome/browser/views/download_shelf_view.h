// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H_
#define CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H_

#include "chrome/browser/download/download_shelf.h"
#include "chrome/common/slide_animation.h"
#include "chrome/views/controls/button/button.h"
#include "chrome/views/controls/link.h"

namespace views {
class ImageButton;
class ImageView;
}

class BaseDownloadItemModel;
class TabContents;

class DownloadAnimation;

// DownloadShelfView is a view that contains individual views for each download,
// as well as a close button and a link to show all downloads.
//
// To add a view representing a download to DownloadShelfView, invoke
// AddDownloadView. AddDownloadView takes ownership of the passed in View.
// DownloadShelfView does not hold an infinite number of download views, rather
// it'll automatically remove views once a certain point is reached.
class DownloadShelfView : public DownloadShelf,
                          public views::View,
                          public views::ButtonListener,
                          public views::LinkController,
                          public AnimationDelegate {
 public:
  explicit DownloadShelfView(TabContents* tab_contents);

  // Implementation of View.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(ChromeCanvas* canvas);

  // Implementation of AnimationDelegate.
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // Implementation of LinkController.
  // Invoked when the user clicks the 'show all downloads' link button.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Implementation of ButtonListener.
  // Invoked when the user clicks the close button. Asks the browser to
  // hide the download shelf.
  virtual void ButtonPressed(views::Button* button);

  // Implementation of DownloadShelf.
  virtual void AddDownload(BaseDownloadItemModel* download_model);
  virtual bool IsShowing() const;

  // Removes a specified download view. The supplied view is deleted after
  // it's removed.
  void RemoveDownloadView(views::View* view);

 private:
  void Init();

  // Adds a View representing a download to this DownloadShelfView.
  // DownloadShelfView takes ownership of the View, and will delete it as
  // necessary.
  void AddDownloadView(views::View* view);

  // Paints the border.
  void PaintBorder(ChromeCanvas* canvas);

  // The animation for adding new items to the shelf.
  scoped_ptr<SlideAnimation> new_item_animation_;

  // The show/hide animation for the shelf itself.
  scoped_ptr<SlideAnimation> shelf_animation_;

  // The download views. These are also child Views, and deleted when
  // the DownloadShelfView is deleted.
  std::vector<View*> download_views_;

  // An image displayed on the right of the "Show all downloads..." link.
  views::ImageView* arrow_image_;

  // Link for showing all downloads. This is contained as a child, and deleted
  // by View.
  views::Link* show_all_view_;

  // Button for closing the downloads. This is contained as a child, and
  // deleted by View.
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(DownloadShelfView);
};

#endif  // CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H_
