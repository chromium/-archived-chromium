// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/download_item_view.h"

#include <vector>

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/root_view.h"
#include "chrome/views/view_container.h"

#include "generated_resources.h"

// TODO(paulg): These may need to be adjusted when download progress
//              animation is added, and also possibly to take into account
//              different screen resolutions.
static const int kTextWidth = 140;           // Pixels
static const int kHorizontalTextPadding = 2; // Pixels
static const int kVerticalPadding = 3;       // Pixels
static const int kVerticalTextSpacer = 2;    // Pixels
static const int kVerticalTextPadding = 2;   // Pixels

// We add some padding before the left image so that the progress animation icon
// hides the corners of the left image.
static const int kLeftPadding = 0;  // Pixels.

static const SkColor kFileNameColor = SkColorSetRGB(87, 108, 149);
static const SkColor kStatusColor = SkColorSetRGB(123, 141, 174);

// How long the 'download complete' animation should last for.
static const int kCompleteAnimationDurationMs = 2500;

// DownloadItemView ------------------------------------------------------------

DownloadItemView::DownloadItemView(DownloadItem* download,
    DownloadShelfView* parent,
    DownloadItemView::BaseDownloadItemModel* model)
  : download_(download),
    parent_(parent),
    model_(model),
    progress_angle_(download_util::kStartAngleDegrees),
    body_state_(NORMAL),
    drop_down_state_(NORMAL),
    drop_down_pressed_(false),
    file_name_(download_->file_name()),
    status_text_(l10n_util::GetString(IDS_DOWNLOAD_STATUS_STARTING)),
    show_status_text_(true),
    dragging_(false),
    starting_drag_(false) {
  // TODO(idana) Bug# 1163334
  //
  // We currently do not mirror each download item on the download shelf (even
  // though the download shelf itself is mirrored and the items appear from
  // right to left on RTL UIs).
  //
  // We explicitly disable mirroring for the item because the code that draws
  // the download progress animation relies on the View's UI layout setting
  // when positioning the animation so we should make sure that code doesn't
  // treat our View as a mirrored View.
  EnableUIMirroringForRTLLanguages(false);
  DCHECK(download_);
  download_->AddObserver(this);

  ResourceBundle &rb = ResourceBundle::GetSharedInstance();

  BodyImageSet normal_body_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM)
  };
  normal_body_image_set_ = normal_body_image_set;

  DropDownImageSet normal_drop_down_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM)
  };
  normal_drop_down_image_set_ = normal_drop_down_image_set;

  BodyImageSet hot_body_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_H)
  };
  hot_body_image_set_ = hot_body_image_set;

  DropDownImageSet hot_drop_down_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_H),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_H)
  };
  hot_drop_down_image_set_ = hot_drop_down_image_set;

  BodyImageSet pushed_body_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_P)
  };
  pushed_body_image_set_ = pushed_body_image_set;

  DropDownImageSet pushed_drop_down_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_P),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_P)
  };
  pushed_drop_down_image_set_ = pushed_drop_down_image_set;

  LoadIcon();

  font_ = ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
  box_height_ = std::max<int>(2 * kVerticalPadding + font_.height() +
                                  kVerticalTextPadding + font_.height(),
                              2 * kVerticalPadding +
                                  normal_body_image_set_.top_left->height() +
                                  normal_body_image_set_.bottom_left->height());

  if (download_util::kSmallProgressIconSize > box_height_)
    box_y_ = (download_util::kSmallProgressIconSize - box_height_) / 2;
  else
    box_y_ = kVerticalPadding;

  CSize size;
  GetPreferredSize(&size);
  drop_down_x_ = size.cx - normal_drop_down_image_set_.top->width();

  body_hover_animation_.reset(new SlideAnimation(this));
  drop_hover_animation_.reset(new SlideAnimation(this));

  // Set up our animation
  StartDownloadProgress();
}

DownloadItemView::~DownloadItemView() {
  icon_consumer_.CancelAllRequests();
  StopDownloadProgress();
  download_->RemoveObserver(this);
}

// Progress animation handlers

void DownloadItemView::UpdateDownloadProgress() {
  progress_angle_ = (progress_angle_ +
                     download_util::kUnknownIncrementDegrees) %
                    download_util::kMaxDegrees;
  SchedulePaint();
}

void DownloadItemView::StartDownloadProgress() {
  if (progress_timer_.IsRunning())
    return;
  progress_timer_.Start(
      TimeDelta::FromMilliseconds(download_util::kProgressRateMs), this,
      &DownloadItemView::UpdateDownloadProgress);
}

void DownloadItemView::StopDownloadProgress() {
  progress_timer_.Stop();
}

// DownloadObserver interface

// Update the progress graphic on the icon and our text status label
// to reflect our current bytes downloaded, time remaining.
void DownloadItemView::OnDownloadUpdated(DownloadItem* download) {
  DCHECK(download == download_);

  std::wstring status_text = model_->GetStatusText();
  switch (download_->state()) {
    case DownloadItem::IN_PROGRESS:
      download_->is_paused() ? StopDownloadProgress() : StartDownloadProgress();
      break;
    case DownloadItem::COMPLETE:
      StopDownloadProgress();
      complete_animation_.reset(new SlideAnimation(this));
      complete_animation_->SetSlideDuration(kCompleteAnimationDurationMs);
      complete_animation_->SetTweenType(SlideAnimation::NONE);
      complete_animation_->Show();
      if (status_text.empty())
        show_status_text_ = false;
      SchedulePaint();
      LoadIcon();
      break;
    case DownloadItem::CANCELLED:
      StopDownloadProgress();
      LoadIcon();
      break;
    case DownloadItem::REMOVING:
      parent_->RemoveDownloadView(this);  // This will delete us!
      return;
    default:
      NOTREACHED();
  }

  status_text_ = status_text;

  // We use the parent's (DownloadShelfView's) SchedulePaint, since there
  // are spaces between each DownloadItemView that the parent is responsible
  // for painting.
  GetParent()->SchedulePaint();
}

// View overrides

// Load an icon for the file type we're downloading, and animate any in progress
// download state.
void DownloadItemView::Paint(ChromeCanvas* canvas) {
  int height = GetHeight();
  int center_width = GetWidth() - kLeftPadding -
                     normal_body_image_set_.left->width() -
                     normal_body_image_set_.right->width() -
                     normal_drop_down_image_set_.center->width();

  // May be caused by animation.
  if (center_width <= 0)
    return;

  BodyImageSet* body_image_set;
  switch (body_state_) {
    case NORMAL:
    case HOT:
      body_image_set = &normal_body_image_set_;
      break;
    case PUSHED:
      body_image_set = &pushed_body_image_set_;
      break;
    default:
      NOTREACHED();
  }
  DropDownImageSet* drop_down_image_set;
  switch (drop_down_state_) {
    case NORMAL:
    case HOT:
      drop_down_image_set = &normal_drop_down_image_set_;
      break;
    case PUSHED:
      drop_down_image_set = &pushed_drop_down_image_set_;
      break;
    default:
      NOTREACHED();
  }

  // Paint the background images.
  int x = kLeftPadding;
  PaintBitmaps(canvas,
               body_image_set->top_left, body_image_set->left,
               body_image_set->bottom_left,
               x, box_y_, box_height_, body_image_set->top_left->width());
  x += body_image_set->top_left->width();
  PaintBitmaps(canvas,
               body_image_set->top, body_image_set->center,
               body_image_set->bottom,
               x, box_y_, box_height_, center_width);
  x += center_width;
  PaintBitmaps(canvas,
               body_image_set->top_right, body_image_set->right,
               body_image_set->bottom_right,
               x, box_y_, box_height_, body_image_set->top_right->width());

  // Overlay our body hot state.
  if (body_hover_animation_->GetCurrentValue() > 0) {
    canvas->saveLayerAlpha(NULL,
        static_cast<int>(body_hover_animation_->GetCurrentValue() * 255),
        SkCanvas::kARGB_NoClipLayer_SaveFlag);
    canvas->drawARGB(0, 255, 255, 255, SkPorterDuff::kClear_Mode);

    int x = kLeftPadding;
    PaintBitmaps(canvas,
                 hot_body_image_set_.top_left, hot_body_image_set_.left,
                 hot_body_image_set_.bottom_left,
                 x, box_y_, box_height_, hot_body_image_set_.top_left->width());
    x += body_image_set->top_left->width();
    PaintBitmaps(canvas,
                 hot_body_image_set_.top, hot_body_image_set_.center,
                 hot_body_image_set_.bottom,
                 x, box_y_, box_height_, center_width);
    x += center_width;
    PaintBitmaps(canvas,
                 hot_body_image_set_.top_right, hot_body_image_set_.right,
                 hot_body_image_set_.bottom_right,
                 x, box_y_, box_height_, hot_body_image_set_.top_right->width());
    canvas->restore();
  }

  x += body_image_set->top_right->width();
  PaintBitmaps(canvas,
               drop_down_image_set->top, drop_down_image_set->center,
               drop_down_image_set->bottom,
               x, box_y_, box_height_, drop_down_image_set->top->width());

  // Overlay our drop-down hot state.
  if (drop_hover_animation_->GetCurrentValue() > 0) {
    canvas->saveLayerAlpha(NULL,
        static_cast<int>(drop_hover_animation_->GetCurrentValue() * 255),
        SkCanvas::kARGB_NoClipLayer_SaveFlag);
    canvas->drawARGB(0, 255, 255, 255, SkPorterDuff::kClear_Mode);

    PaintBitmaps(canvas,
                 drop_down_image_set->top, drop_down_image_set->center,
                 drop_down_image_set->bottom,
                 x, box_y_, box_height_, drop_down_image_set->top->width());

    canvas->restore();
  }

  // Print the text, left aligned.
  // Last value of x was the end of the right image, just before the button.
  if (show_status_text_) {
    int y = box_y_ + kVerticalPadding;
    canvas->DrawStringInt(file_name_, font_, kFileNameColor,
                          download_util::kSmallProgressIconSize, y,
                          kTextWidth, font_.height());
    y += font_.height() + kVerticalTextPadding;

    canvas->DrawStringInt(status_text_, font_, kStatusColor,
                          download_util::kSmallProgressIconSize, y,
                          kTextWidth, font_.height());
  } else {
    int y = box_y_ + (box_height_ - font_.height()) / 2;
    canvas->DrawStringInt(file_name_, font_, kFileNameColor,
                          download_util::kSmallProgressIconSize, y,
                          kTextWidth, font_.height());
  }

  // Paint the icon.
  IconManager* im = g_browser_process->icon_manager();
  SkBitmap* icon = im->LookupIcon(download_->full_path(), IconLoader::SMALL);

  if (icon) {
    if (download_->state() == DownloadItem::IN_PROGRESS) {
      download_util::PaintDownloadProgress(canvas, this, 0, 0,
                                           progress_angle_,
                                           download_->PercentComplete(),
                                           download_util::SMALL);
    } else if (download_->state() == DownloadItem::COMPLETE &&
        complete_animation_->IsAnimating()) {
      download_util::PaintDownloadComplete(canvas, this, 0, 0,
          complete_animation_->GetCurrentValue(),
          download_util::SMALL);
    }

    // Draw the icon image
    canvas->DrawBitmapInt(*icon,
                          download_util::kSmallProgressIconOffset,
                          download_util::kSmallProgressIconOffset);
  }
}

void DownloadItemView::PaintBitmaps(ChromeCanvas* canvas,
                                    const SkBitmap* top_bitmap,
                                    const SkBitmap* center_bitmap,
                                    const SkBitmap* bottom_bitmap,
                                    int x, int y, int height, int width) {
  int middle_height = height - top_bitmap->height() - bottom_bitmap->height();
  // Draw the top.
  canvas->DrawBitmapInt(*top_bitmap,
                        0, 0, top_bitmap->width(), top_bitmap->height(),
                        x, y, width, top_bitmap->height(), false);
  y += top_bitmap->height();
  // Draw the center.
  canvas->DrawBitmapInt(*center_bitmap,
                        0, 0, center_bitmap->width(), center_bitmap->height(),
                        x, y, width, middle_height, false);
  y += middle_height;
  // Draw the bottom.
  canvas->DrawBitmapInt(*bottom_bitmap,
                        0, 0, bottom_bitmap->width(), bottom_bitmap->height(),
                        x, y, width, bottom_bitmap->height(), false);
}

void DownloadItemView::SetState(State body_state, State drop_down_state) {
  if (body_state_ == body_state && drop_down_state_ == drop_down_state)
    return;

  body_state_ = body_state;
  drop_down_state_ = drop_down_state;
  SchedulePaint();
}

void DownloadItemView::GetPreferredSize(CSize* out) {
  int width = kLeftPadding + normal_body_image_set_.top_left->width();
  width += download_util::kSmallProgressIconSize;
  width += kTextWidth;
  width += normal_body_image_set_.top_right->width();
  width += normal_drop_down_image_set_.top->width();

  out->cx = width;
  out->cy = std::max<int>(
      2 * kVerticalPadding +  2 * font_.height() + kVerticalTextPadding,
      download_util::kSmallProgressIconSize);
}

void DownloadItemView::OnMouseExited(const ChromeViews::MouseEvent& event) {
  SetState(NORMAL, drop_down_pressed_ ? PUSHED : NORMAL);
  body_hover_animation_->Hide();
  drop_hover_animation_->Hide();
}

// Display the context menu for this item.
bool DownloadItemView::OnMousePressed(const ChromeViews::MouseEvent& event) {
  // Stop any completion animation.
  if (complete_animation_.get() && complete_animation_->IsAnimating())
    complete_animation_->End();

  if (event.IsOnlyLeftMouseButton()) {
    WTL::CPoint point(event.GetX(), event.GetY());
    if (event.GetX() < drop_down_x_) {
      SetState(PUSHED, NORMAL);
      return true;
    }

    drop_down_pressed_ = true;
    SetState(NORMAL, PUSHED);

    // Similar hack as in MenuButton.
    // We're about to show the menu from a mouse press. By showing from the
    // mouse press event we block RootView in mouse dispatching. This also
    // appears to cause RootView to get a mouse pressed BEFORE the mouse
    // release is seen, which means RootView sends us another mouse press no
    // matter where the user pressed. To force RootView to recalculate the
    // mouse target during the mouse press we explicitly set the mouse handler
    // to NULL.
    GetRootView()->SetMouseHandler(NULL);

    // The menu's position is different depending on the UI layout.
    // DownloadShelfContextMenu will take care of setting the right anchor for
    // the menu depending on the locale.
    //
    // TODO(idana): when bug# 1163334 is fixed the following check should be
    // replaced with UILayoutIsRightToLeft().
    point.y = GetHeight();
    if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
      point.x = GetWidth();
    } else {
      point.x = drop_down_x_;
    }

    ChromeViews::View::ConvertPointToScreen(this, &point);
    download_util::DownloadShelfContextMenu menu(download_,
                                                 GetViewContainer()->GetHWND(),
                                                 model_.get(),
                                                 point);
    drop_down_pressed_ = false;
    // Showing the menu blocks. Here we revert the state.
    SetState(NORMAL, NORMAL);
  }
  return true;
}

void DownloadItemView::OnMouseMoved(const ChromeViews::MouseEvent& event) {
  bool on_body = event.GetX() < drop_down_x_;
  SetState(on_body ? HOT : NORMAL, on_body ? NORMAL : HOT);
  if (on_body) {
    body_hover_animation_->Show();
    drop_hover_animation_->Hide();
  } else {
    body_hover_animation_->Hide();
    drop_hover_animation_->Show();
  }
}

void DownloadItemView::OnMouseReleased(const ChromeViews::MouseEvent& event,
                                       bool canceled) {
  if (dragging_) {
    // Starting a drag results in a MouseReleased, we need to ignore it.
    dragging_ = false;
    starting_drag_ = false;
    return;
  }
  if (event.IsOnlyLeftMouseButton() && event.GetX() < drop_down_x_)
    OpenDownload();

  SetState(NORMAL, NORMAL);
}

// Handle drag (file copy) operations.
bool DownloadItemView::OnMouseDragged(const ChromeViews::MouseEvent& event) {
  if (!starting_drag_) {
    starting_drag_ = true;
    drag_start_point_ = event.location();
  }
  if (dragging_) {
    if (download_->state() == DownloadItem::COMPLETE) {
      IconManager* im = g_browser_process->icon_manager();
      SkBitmap* icon = im->LookupIcon(download_->full_path(),
                                      IconLoader::SMALL);
      if (icon)
        download_util::DragDownload(download_, icon);
    }
  } else if (win_util::IsDrag(drag_start_point_.ToPOINT(),
                              event.location().ToPOINT())) {
    dragging_ = true;
  }
  return true;
}

void DownloadItemView::AnimationProgressed(const Animation* animation) {
  // We don't care if what animation (body button/drop button/complete),
  // is calling back, as they all have to go through the same paint call.
  SchedulePaint();
}

void DownloadItemView::OpenDownload() {
  if (download_->state() == DownloadItem::IN_PROGRESS) {
    download_->set_open_when_complete(!download_->open_when_complete());
  } else if (download_->state() == DownloadItem::COMPLETE) {
    download_util::OpenDownload(download_);
  }
}

void DownloadItemView::OnExtractIconComplete(IconManager::Handle handle,
                                             SkBitmap* icon_bitmap) {
  GetParent()->SchedulePaint();
}

void DownloadItemView::LoadIcon() {
  IconManager* im = g_browser_process->icon_manager();
  im->LoadIcon(download_->full_path(), IconLoader::SMALL,
               &icon_consumer_,
               NewCallback(this, &DownloadItemView::OnExtractIconComplete));
}

