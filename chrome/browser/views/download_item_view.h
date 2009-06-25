// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A ChromeView that implements one download on the Download shelf.
// Each DownloadItemView contains an application icon, a text label
// indicating the download's file name, a text label indicating the
// download's status (such as the number of bytes downloaded so far)
// and a button for canceling an in progress download, or opening
// the completed download.
//
// The DownloadItemView lives in the Browser, and has a corresponding
// DownloadController that receives / writes data which lives in the
// Renderer.

#ifndef CHROME_BROWSER_VIEWS_DOWNLOAD_ITEM_VIEW_H__
#define CHROME_BROWSER_VIEWS_DOWNLOAD_ITEM_VIEW_H__

#include <string>

#include "app/gfx/font.h"
#include "app/slide_animation.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/icon_manager.h"
#include "views/event.h"
#include "views/controls/button/button.h"
#include "views/view.h"

namespace views {
class Label;
class NativeButton;
}
class BaseDownloadItemModel;
class DownloadShelfView;
class SkBitmap;

class DownloadItemView : public views::ButtonListener,
                         public views::View,
                         public DownloadItem::Observer,
                         public AnimationDelegate {
 public:
  DownloadItemView(DownloadItem* download,
                   DownloadShelfView* parent,
                   BaseDownloadItemModel* model);
  virtual ~DownloadItemView();

  // DownloadObserver method
  virtual void OnDownloadUpdated(DownloadItem* download);
  virtual void OnDownloadOpened(DownloadItem* download);

  // View overrides
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual void OnMouseMoved(const views::MouseEvent& event);
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual bool OnMouseDragged(const views::MouseEvent& event);

  // ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender);

  // AnimationDelegate implementation.
  virtual void AnimationProgressed(const Animation* animation);

  // Timer callback for handling animations
  void UpdateDownloadProgress();
  void StartDownloadProgress();
  void StopDownloadProgress();

  // IconManager::Client interface.
  void OnExtractIconComplete(IconManager::Handle handle, SkBitmap* icon_bitmap);

 private:
  enum State {
    NORMAL = 0,
    HOT,
    PUSHED,
    DANGEROUS
  };

  // The image set associated with the part containing the icon and text.
  struct BodyImageSet {
    SkBitmap* top_left;
    SkBitmap* left;
    SkBitmap* bottom_left;
    SkBitmap* top;
    SkBitmap* center;
    SkBitmap* bottom;
    SkBitmap* top_right;
    SkBitmap* right;
    SkBitmap* bottom_right;
  };

  // The image set associated with the drop-down button on the right.
  struct DropDownImageSet {
    SkBitmap* top;
    SkBitmap* center;
    SkBitmap* bottom;
  };

  void OpenDownload();

  void LoadIcon();

  // Convenience method to paint the 3 vertical bitmaps (bottom, middle, top)
  // that form the background.
  void PaintBitmaps(gfx::Canvas* canvas,
                    const SkBitmap* top_bitmap,
                    const SkBitmap* center_bitmap,
                    const SkBitmap* bottom_bitmap,
                    int x,
                    int y,
                    int height,
                    int width);

  // Sets the state and triggers a repaint.
  void SetState(State body_state, State drop_down_state);

  // Whether we are in the dangerous mode.
  bool IsDangerousMode() { return body_state_ == DANGEROUS; }

  // Reverts from dangerous mode to normal download mode.
  void ClearDangerousMode();

  // Sets |size| with the size of the Save and Discard buttons (they have the
  // same size).
  gfx::Size GetButtonSize();

  // Sizes the dangerous download label to a minimum width available using 2
  // lines.  The size is computed only the first time this method is invoked
  // and simply returned on subsequent calls.
  void SizeLabelToMinWidth();

  // Reenables the item after it has been disabled when a user clicked it to
  // open the downloaded file.
  void Reenable();

  // Given |x|, returns whether |x| is within the x coordinate range of
  // the drop-down button or not.
  bool InDropDownButtonXCoordinateRange(int x);

  // The different images used for the background.
  BodyImageSet normal_body_image_set_;
  BodyImageSet hot_body_image_set_;
  BodyImageSet pushed_body_image_set_;
  BodyImageSet dangerous_mode_body_image_set_;
  DropDownImageSet normal_drop_down_image_set_;
  DropDownImageSet hot_drop_down_image_set_;
  DropDownImageSet pushed_drop_down_image_set_;

  // The warning icon showns for dangerous downloads.
  SkBitmap* warning_icon_;

  // The model we query for display information
  DownloadItem* download_;

  // Our parent view that owns us.
  DownloadShelfView* parent_;

  // Elements of our particular download
  std::wstring status_text_;
  bool show_status_text_;

  // The font used to print the file name and status.
  gfx::Font font_;

  // The current state (normal, hot or pushed) of the body and drop-down.
  State body_state_;
  State drop_down_state_;

  // In degrees, for downloads with no known total size.
  int progress_angle_;

  // The left and right x coordinates of the drop-down button.
  int drop_down_x_left_;
  int drop_down_x_right_;

  // Used when we are showing the menu to show the drop-down as pressed.
  bool drop_down_pressed_;

  // The height of the box formed by the background images and its labels.
  int box_height_;

  // The y coordinate of the box formed by the background images and its labels.
  int box_y_;

  // Whether we are dragging the download button.
  bool dragging_;

  // Whether we are tracking a possible drag.
  bool starting_drag_;

  // Position that a possible drag started at.
  gfx::Point drag_start_point_;

  // For canceling an in progress icon request.
  CancelableRequestConsumerT<int, 0> icon_consumer_;

  // A model class to control the status text we display and the cancel
  // behavior.
  // This class owns the pointer.
  scoped_ptr<BaseDownloadItemModel> model_;

  // Hover animations for our body and drop buttons.
  scoped_ptr<SlideAnimation> body_hover_animation_;
  scoped_ptr<SlideAnimation> drop_hover_animation_;

  // Animation for download complete.
  scoped_ptr<SlideAnimation> complete_animation_;

  // Progress animation
  base::RepeatingTimer<DownloadItemView> progress_timer_;

  // Dangerous mode buttons.
  views::NativeButton* save_button_;
  views::NativeButton* discard_button_;

  // Dangerous mode label.
  views::Label* dangerous_download_label_;

  // Whether the dangerous mode label has been sized yet.
  bool dangerous_download_label_sized_;

  // The size of the buttons.  Cached so animation works when hidden.
  gfx::Size cached_button_size_;

  // Whether we are currently disabled as part of opening the downloaded file.
  bool disabled_while_opening_;

  // The time at which this view was created.
  base::Time creation_time_;

  // Method factory used to delay reenabling of the item when opening the
  // downloaded file.
  ScopedRunnableMethodFactory<DownloadItemView> reenable_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemView);
};

#endif  // CHROME_BROWSER_VIEWS_DOWNLOAD_ITEM_VIEW_H__
