// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/download_item_view.h"

#include <vector>

#include "app/gfx/canvas.h"
#include "app/gfx/text_elider.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/native_button.h"
#include "views/controls/menu/menu.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

using base::TimeDelta;

// TODO(paulg): These may need to be adjusted when download progress
//              animation is added, and also possibly to take into account
//              different screen resolutions.
static const int kTextWidth = 140;            // Pixels
static const int kDangerousTextWidth = 200;   // Pixels
static const int kHorizontalTextPadding = 2;  // Pixels
static const int kVerticalPadding = 3;        // Pixels
static const int kVerticalTextSpacer = 2;     // Pixels
static const int kVerticalTextPadding = 2;    // Pixels

// The maximum number of characters we show in a file name when displaying the
// dangerous download message.
static const int kFileNameMaxLength = 20;

// We add some padding before the left image so that the progress animation icon
// hides the corners of the left image.
static const int kLeftPadding = 0;  // Pixels.

// The space between the Save and Discard buttons when prompting for a dangerous
// download.
static const int kButtonPadding = 5;  // Pixels.

// The space on the left and right side of the dangerous download label.
static const int kLabelPadding = 4;  // Pixels.

static const SkColor kFileNameColor = SkColorSetRGB(87, 108, 149);
static const SkColor kFileNameDisabledColor = SkColorSetRGB(171, 192, 212);
static const SkColor kStatusColor = SkColorSetRGB(123, 141, 174);

// How long the 'download complete' animation should last for.
static const int kCompleteAnimationDurationMs = 2500;

// How long we keep the item disabled after the user clicked it to open the
// downloaded item.
static const int kDisabledOnOpenDuration = 3000;

// DownloadShelfContextMenuWin -------------------------------------------------

class DownloadShelfContextMenuWin : public DownloadShelfContextMenu,
                                    public views::Menu::Delegate {
 public:
  DownloadShelfContextMenuWin(BaseDownloadItemModel* model,
                              gfx::NativeView window,
                              const gfx::Point& point)
      : DownloadShelfContextMenu(model) {
    DCHECK(model);

    // The menu's anchor point is determined based on the UI layout.
    views::Menu::AnchorPoint anchor_point;
    if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
      anchor_point = views::Menu::TOPRIGHT;
    else
      anchor_point = views::Menu::TOPLEFT;

    scoped_ptr<views::Menu> context_menu(
        views::Menu::Create(this, anchor_point, window));
    if (download_->state() == DownloadItem::COMPLETE) {
      context_menu->AppendMenuItem(OPEN_WHEN_COMPLETE, L"",
                                   views::Menu::NORMAL);
    } else {
      context_menu->AppendMenuItem(OPEN_WHEN_COMPLETE, L"",
                                   views::Menu::CHECKBOX);
    }
    context_menu->AppendMenuItem(ALWAYS_OPEN_TYPE, L"", views::Menu::CHECKBOX);
    context_menu->AppendSeparator();
    context_menu->AppendMenuItem(SHOW_IN_FOLDER, L"", views::Menu::NORMAL);
    context_menu->AppendSeparator();
    context_menu->AppendMenuItem(CANCEL, L"", views::Menu::NORMAL);
    context_menu->RunMenuAt(point.x(), point.y());
  }

  // Menu::Delegate implementation ---------------------------------------------

  virtual bool IsItemChecked(int id) const {
    return ItemIsChecked(id);
  }

  virtual bool IsItemDefault(int id) const {
    return ItemIsDefault(id);
  }

  virtual std::wstring GetLabel(int id) const {
    return GetItemLabel(id);
  }

  virtual bool SupportsCommand(int id) const {
    return id > 0 && id < MENU_LAST;
  }

  virtual bool IsCommandEnabled(int id) const {
    return IsItemCommandEnabled(id);
  }

  virtual void ExecuteCommand(int id) {
    return ExecuteItemCommand(id);
  }
};

// DownloadItemView ------------------------------------------------------------

DownloadItemView::DownloadItemView(DownloadItem* download,
    DownloadShelfView* parent,
    BaseDownloadItemModel* model)
  : warning_icon_(NULL),
    download_(download),
    parent_(parent),
    status_text_(l10n_util::GetString(IDS_DOWNLOAD_STATUS_STARTING)),
    show_status_text_(true),
    body_state_(NORMAL),
    drop_down_state_(NORMAL),
    progress_angle_(download_util::kStartAngleDegrees),
    drop_down_pressed_(false),
    dragging_(false),
    starting_drag_(false),
    model_(model),
    save_button_(NULL),
    discard_button_(NULL),
    dangerous_download_label_(NULL),
    dangerous_download_label_sized_(false),
    disabled_while_opening_(false),
    creation_time_(base::Time::Now()),
    ALLOW_THIS_IN_INITIALIZER_LIST(reenable_method_factory_(this)) {
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

  BodyImageSet dangerous_mode_body_image_set = {
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_NO_DD),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_NO_DD),
    rb.GetBitmapNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_NO_DD)
  };
  dangerous_mode_body_image_set_ = dangerous_mode_body_image_set;

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

  gfx::Size size = GetPreferredSize();
  if (UILayoutIsRightToLeft()) {
    // Drop down button is glued to the left of the download shelf.
    drop_down_x_left_ = 0;
    drop_down_x_right_ = normal_drop_down_image_set_.top->width();
  } else {
    // Drop down button is glued to the right of the download shelf.
    drop_down_x_left_ =
        size.width() - normal_drop_down_image_set_.top->width();
    drop_down_x_right_ = size.width();
  }

  body_hover_animation_.reset(new SlideAnimation(this));
  drop_hover_animation_.reset(new SlideAnimation(this));

  if (download->safety_state() == DownloadItem::DANGEROUS) {
    body_state_ = DANGEROUS;
    drop_down_state_ = DANGEROUS;

    warning_icon_ = rb.GetBitmapNamed(IDR_WARNING);
    save_button_ = new views::NativeButton(
        this, l10n_util::GetString(IDS_SAVE_DOWNLOAD));
    save_button_->set_ignore_minimum_size(true);
    discard_button_ = new views::NativeButton(
        this, l10n_util::GetString(IDS_DISCARD_DOWNLOAD));
    discard_button_->set_ignore_minimum_size(true);
    AddChildView(save_button_);
    AddChildView(discard_button_);

    // Ensure the file name is not too long.

    // Extract the file extension (if any).
    FilePath filepath(download->original_name());
#if defined(OS_LINUX)
    std::wstring extension = base::SysNativeMBToWide(filepath.Extension());
#else
    std::wstring extension = filepath.Extension();
#endif

    // Remove leading '.'
    if (extension.length() > 0)
      extension = extension.substr(1);
#if defined(OS_LINUX)
    std::wstring rootname =
        base::SysNativeMBToWide(filepath.BaseName().RemoveExtension().value());
#else
    std::wstring rootname = filepath.BaseName().RemoveExtension().value();
#endif

    // Elide giant extensions (this shouldn't currently be hit, but might
    // in future, should we ever notice unsafe giant extensions).
    if (extension.length() > kFileNameMaxLength / 2)
      ElideString(extension, kFileNameMaxLength / 2, &extension);

    ElideString(rootname, kFileNameMaxLength - extension.length(), &rootname);
    dangerous_download_label_ = new views::Label(
        l10n_util::GetStringF(IDS_PROMPT_DANGEROUS_DOWNLOAD,
                              rootname + L"." + extension));
    dangerous_download_label_->SetMultiLine(true);
    dangerous_download_label_->SetHorizontalAlignment(
        views::Label::ALIGN_LEFT);
    dangerous_download_label_->SetColor(kFileNameColor);
    AddChildView(dangerous_download_label_);
    SizeLabelToMinWidth();
  }

  // Set up our animation.
  StartDownloadProgress();
}

DownloadItemView::~DownloadItemView() {
  icon_consumer_.CancelAllRequests();
  StopDownloadProgress();
  download_->RemoveObserver(this);
}

// Progress animation handlers.

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

// DownloadObserver interface.

// Update the progress graphic on the icon and our text status label
// to reflect our current bytes downloaded, time remaining.
void DownloadItemView::OnDownloadUpdated(DownloadItem* download) {
  DCHECK(download == download_);

  if (body_state_ == DANGEROUS &&
      download->safety_state() == DownloadItem::DANGEROUS_BUT_VALIDATED) {
    // We have been approved.
    ClearDangerousMode();
  }

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

void DownloadItemView::OnDownloadOpened(DownloadItem* download) {
  disabled_while_opening_ = true;
  SetEnabled(false);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      reenable_method_factory_.NewRunnableMethod(&DownloadItemView::Reenable),
      kDisabledOnOpenDuration);
}

// View overrides

// In dangerous mode we have to layout our buttons.
void DownloadItemView::Layout() {
  if (IsDangerousMode()) {
    int x = kLeftPadding + dangerous_mode_body_image_set_.top_left->width() +
      warning_icon_->width() + kLabelPadding;
    int y = (height() - dangerous_download_label_->height()) / 2;
    dangerous_download_label_->SetBounds(x, y,
                                         dangerous_download_label_->width(),
                                         dangerous_download_label_->height());
    gfx::Size button_size = GetButtonSize();
    x += dangerous_download_label_->width() + kLabelPadding;
    y = (height() - button_size.height()) / 2;
    save_button_->SetBounds(x, y, button_size.width(), button_size.height());
    x += button_size.width() + kButtonPadding;
    discard_button_->SetBounds(x, y, button_size.width(), button_size.height());
  }
}

void DownloadItemView::ButtonPressed(views::Button* sender) {
  if (sender == discard_button_) {
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.discard_download",
                             base::Time::Now() - creation_time_);
    if (download_->state() == DownloadItem::IN_PROGRESS)
      download_->Cancel(true);
    download_->Remove(true);
    // WARNING: we are deleted at this point.  Don't access 'this'.
  } else if (sender == save_button_) {
    // The user has confirmed a dangerous download.  We'd record how quickly the
    // user did this to detect whether we're being clickjacked.
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.save_download",
                             base::Time::Now() - creation_time_);
    // This will change the state and notify us.
    download_->manager()->DangerousDownloadValidated(download_);
  }
}

// Load an icon for the file type we're downloading, and animate any in progress
// download state.
void DownloadItemView::Paint(gfx::Canvas* canvas) {
  BodyImageSet* body_image_set;
  switch (body_state_) {
    case NORMAL:
    case HOT:
      body_image_set = &normal_body_image_set_;
      break;
    case PUSHED:
      body_image_set = &pushed_body_image_set_;
      break;
    case DANGEROUS:
      body_image_set = &dangerous_mode_body_image_set_;
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
    case DANGEROUS:
      drop_down_image_set = NULL;  // No drop-down in dangerous mode.
      break;
    default:
      NOTREACHED();
  }

  int center_width = width() - kLeftPadding -
                     body_image_set->left->width() -
                     body_image_set->right->width() -
                     (drop_down_image_set ?
                        normal_drop_down_image_set_.center->width() :
                        0);

  // May be caused by animation.
  if (center_width <= 0)
    return;

  // Paint the background images.
  int x = kLeftPadding;
  bool rtl_ui = UILayoutIsRightToLeft();
  if (rtl_ui) {
    // Since we do not have the mirrored images for
    // (hot_)body_image_set->top_left, (hot_)body_image_set->left,
    // (hot_)body_image_set->bottom_left, and drop_down_image_set,
    // for RTL UI, we flip the canvas to draw those images mirrored.
    // Consequently, we do not need to mirror the x-axis of those images.
    canvas->save();
    canvas->TranslateInt(width(), 0);
    canvas->ScaleInt(-1, 1);
  }
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
    canvas->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);

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
                 x, box_y_, box_height_,
                 hot_body_image_set_.top_right->width());
    canvas->restore();
    if (rtl_ui) {
      canvas->restore();
      canvas->save();
      // Flip it for drawing drop-down images for RTL locales.
      canvas->TranslateInt(width(), 0);
      canvas->ScaleInt(-1, 1);
    }
  }

  x += body_image_set->top_right->width();

  // Paint the drop-down.
  if (drop_down_image_set) {
    PaintBitmaps(canvas,
                 drop_down_image_set->top, drop_down_image_set->center,
                 drop_down_image_set->bottom,
                 x, box_y_, box_height_, drop_down_image_set->top->width());

    // Overlay our drop-down hot state.
    if (drop_hover_animation_->GetCurrentValue() > 0) {
      canvas->saveLayerAlpha(NULL,
          static_cast<int>(drop_hover_animation_->GetCurrentValue() * 255),
          SkCanvas::kARGB_NoClipLayer_SaveFlag);
      canvas->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);

      PaintBitmaps(canvas,
                   drop_down_image_set->top, drop_down_image_set->center,
                   drop_down_image_set->bottom,
                   x, box_y_, box_height_, drop_down_image_set->top->width());

      canvas->restore();
    }
  }

  if (rtl_ui) {
    // Restore the canvas to avoid file name etc. text are drawn flipped.
    // Consequently, the x-axis of following canvas->DrawXXX() method should be
    // mirrored so the text and images are down in the right positions.
    canvas->restore();
  }

  // Print the text, left aligned and always print the file extension.
  // Last value of x was the end of the right image, just before the button.
  // Note that in dangerous mode we use a label (as the text is multi-line).
  if (!IsDangerousMode()) {
    std::wstring filename;
    if (!disabled_while_opening_) {
      filename = gfx::ElideFilename(download_->GetFileName(),
                                    font_, kTextWidth);
    } else {
      std::wstring tmp_name =
          l10n_util::GetStringF(IDS_DOWNLOAD_STATUS_OPENING,
                                download_->GetFileName().ToWStringHack());
#if defined(OS_WIN)
      FilePath filepath(tmp_name);
#else
      FilePath filepath(base::SysWideToNativeMB(tmp_name));
#endif
      filename = gfx::ElideFilename(filepath, font_, kTextWidth);
    }

    int mirrored_x = MirroredXWithWidthInsideView(
        download_util::kSmallProgressIconSize, kTextWidth);
    if (show_status_text_) {
      int y = box_y_ + kVerticalPadding;

      // Draw the file's name.
      canvas->DrawStringInt(filename, font_,
                            IsEnabled() ? kFileNameColor :
                                          kFileNameDisabledColor,
                            mirrored_x, y, kTextWidth, font_.height());

      y += font_.height() + kVerticalTextPadding;

      canvas->DrawStringInt(status_text_, font_, kStatusColor, mirrored_x, y,
                            kTextWidth, font_.height());
    } else {
      int y = box_y_ + (box_height_ - font_.height()) / 2;

      // Draw the file's name.
      canvas->DrawStringInt(filename, font_,
                            IsEnabled() ? kFileNameColor :
                                          kFileNameDisabledColor,
                            mirrored_x, y, kTextWidth, font_.height());
    }
  }

  // Paint the icon.
  IconManager* im = g_browser_process->icon_manager();
  SkBitmap* icon = IsDangerousMode() ? warning_icon_ :
      im->LookupIcon(download_->full_path(), IconLoader::SMALL);

  // We count on the fact that the icon manager will cache the icons and if one
  // is available, it will be cached here. We *don't* want to request the icon
  // to be loaded here, since this will also get called if the icon can't be
  // loaded, in which case LookupIcon will always be NULL. The loading will be
  // triggered only when we think the status might change.
  if (icon) {
    if (!IsDangerousMode()) {
      if (download_->state() == DownloadItem::IN_PROGRESS) {
        download_util::PaintDownloadProgress(canvas, this, 0, 0,
                                             progress_angle_,
                                             download_->PercentComplete(),
                                             download_util::SMALL);
      } else if (download_->state() == DownloadItem::COMPLETE &&
                 complete_animation_.get() &&
                 complete_animation_->IsAnimating()) {
        download_util::PaintDownloadComplete(canvas, this, 0, 0,
            complete_animation_->GetCurrentValue(),
            download_util::SMALL);
      }
    }

    // Draw the icon image.
    int mirrored_x = MirroredXWithWidthInsideView(
        download_util::kSmallProgressIconOffset, icon->width());
    if (IsEnabled()) {
      canvas->DrawBitmapInt(*icon, mirrored_x,
                            download_util::kSmallProgressIconOffset);
    } else {
      // Use an alpha to make the image look disabled.
      SkPaint paint;
      paint.setAlpha(120);
      canvas->DrawBitmapInt(*icon, mirrored_x,
                            download_util::kSmallProgressIconOffset, paint);
    }
  }
}

void DownloadItemView::PaintBitmaps(gfx::Canvas* canvas,
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

void DownloadItemView::ClearDangerousMode() {
  DCHECK(download_->safety_state() == DownloadItem::DANGEROUS_BUT_VALIDATED &&
         body_state_ == DANGEROUS && drop_down_state_ == DANGEROUS);

  body_state_ = NORMAL;
  drop_down_state_ = NORMAL;

  // Remove the views used by the dangerous mode.
  RemoveChildView(save_button_);
  delete save_button_;
  save_button_ = NULL;
  RemoveChildView(discard_button_);
  delete discard_button_;
  discard_button_ = NULL;
  RemoveChildView(dangerous_download_label_);
  delete dangerous_download_label_;
  dangerous_download_label_ = NULL;

  // We need to load the icon now that the download_ has the real path.
  LoadIcon();

  // Force the shelf to layout again as our size has changed.
  parent_->Layout();
  parent_->SchedulePaint();
}

gfx::Size DownloadItemView::GetPreferredSize() {
  int width, height;

  // First, we set the height to the height of two rows or text plus margins.
  height = 2 * kVerticalPadding + 2 * font_.height() + kVerticalTextPadding;
  // Then we increase the size if the progress icon doesn't fit.
  height = std::max<int>(height, download_util::kSmallProgressIconSize);

  if (IsDangerousMode()) {
    width = kLeftPadding + dangerous_mode_body_image_set_.top_left->width();
    width += warning_icon_->width() + kLabelPadding;
    width += dangerous_download_label_->width() + kLabelPadding;
    gfx::Size button_size = GetButtonSize();
    // Make sure the button fits.
    height = std::max<int>(height, 2 * kVerticalPadding + button_size.height());
    // Then we make sure the warning icon fits.
    height = std::max<int>(height, 2 * kVerticalPadding +
                                   warning_icon_->height());
    width += button_size.width() * 2 + kButtonPadding;
    width += dangerous_mode_body_image_set_.top_right->width();
  } else {
    width = kLeftPadding + normal_body_image_set_.top_left->width();
    width += download_util::kSmallProgressIconSize;
    width += kTextWidth;
    width += normal_body_image_set_.top_right->width();
    width += normal_drop_down_image_set_.top->width();
  }
  return gfx::Size(width, height);
}

void DownloadItemView::OnMouseExited(const views::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsDangerousMode())
    return;

  SetState(NORMAL, drop_down_pressed_ ? PUSHED : NORMAL);
  body_hover_animation_->Hide();
  drop_hover_animation_->Hide();
}

// Display the context menu for this item.
bool DownloadItemView::OnMousePressed(const views::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsDangerousMode())
    return true;

  // Stop any completion animation.
  if (complete_animation_.get() && complete_animation_->IsAnimating())
    complete_animation_->End();

  if (event.IsOnlyLeftMouseButton()) {
    gfx::Point point(event.location());
    if (!InDropDownButtonXCoordinateRange(event.x())) {
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
    point.set_y(height());
    if (UILayoutIsRightToLeft())
      point.set_x(drop_down_x_right_);
    else
      point.set_x(drop_down_x_left_);

    views::View::ConvertPointToScreen(this, &point);
    DownloadShelfContextMenuWin menu(model_.get(),
                                     GetWidget()->GetNativeView(),
                                     point);
    drop_down_pressed_ = false;
    // Showing the menu blocks. Here we revert the state.
    SetState(NORMAL, NORMAL);
  }
  return true;
}

void DownloadItemView::OnMouseMoved(const views::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsDangerousMode())
    return;

  bool on_body = !InDropDownButtonXCoordinateRange(event.x());
  SetState(on_body ? HOT : NORMAL, on_body ? NORMAL : HOT);
  if (on_body) {
    body_hover_animation_->Show();
    drop_hover_animation_->Hide();
  } else {
    body_hover_animation_->Hide();
    drop_hover_animation_->Show();
  }
}

void DownloadItemView::OnMouseReleased(const views::MouseEvent& event,
                                       bool canceled) {
  // Mouse should not activate us in dangerous mode.
  if (IsDangerousMode())
    return;

  if (dragging_) {
    // Starting a drag results in a MouseReleased, we need to ignore it.
    dragging_ = false;
    starting_drag_ = false;
    return;
  }
  if (event.IsOnlyLeftMouseButton() &&
      !InDropDownButtonXCoordinateRange(event.x()))
    OpenDownload();

  SetState(NORMAL, NORMAL);
}

// Handle drag (file copy) operations.
bool DownloadItemView::OnMouseDragged(const views::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsDangerousMode())
    return true;

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
  } else if (ExceededDragThreshold(
                 event.location().x() - drag_start_point_.x(),
                 event.location().y() - drag_start_point_.y())) {
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
  // We're interested in how long it takes users to open downloads.  If they
  // open downloads super quickly, we should be concerned about clickjacking.
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.open_download",
                           base::Time::Now() - creation_time_);
  if (download_->state() == DownloadItem::IN_PROGRESS) {
    download_->set_open_when_complete(!download_->open_when_complete());
  } else if (download_->state() == DownloadItem::COMPLETE) {
    download_util::OpenDownload(download_);
  }
}

void DownloadItemView::OnExtractIconComplete(IconManager::Handle handle,
                                             SkBitmap* icon_bitmap) {
  if (icon_bitmap)
    GetParent()->SchedulePaint();
}

void DownloadItemView::LoadIcon() {
  IconManager* im = g_browser_process->icon_manager();
  im->LoadIcon(download_->full_path(), IconLoader::SMALL, &icon_consumer_,
               NewCallback(this, &DownloadItemView::OnExtractIconComplete));
}

gfx::Size DownloadItemView::GetButtonSize() {
  DCHECK(save_button_ && discard_button_);
  gfx::Size size;

  // We cache the size when successfully retrieved, not for performance reasons
  // but because if this DownloadItemView is being animated while the tab is
  // not showing, the native buttons are not parented and their preferred size
  // is 0, messing-up the layout.
  if (cached_button_size_.width() != 0)
    return cached_button_size_;

  size = save_button_->GetMinimumSize();
  gfx::Size discard_size = discard_button_->GetMinimumSize();

  size.SetSize(std::max(size.width(), discard_size.width()),
               std::max(size.height(), discard_size.height()));

  if (size.width() != 0)
    cached_button_size_ = size;

  return size;
}

// This method computes the minimum width of the label for displaying its text
// on 2 lines.  It just breaks the string in 2 lines on the spaces and keeps the
// configuration with minimum width.
void DownloadItemView::SizeLabelToMinWidth() {
  if (dangerous_download_label_sized_)
    return;

  std::wstring text = dangerous_download_label_->GetText();
  TrimWhitespace(text, TRIM_ALL, &text);
  DCHECK_EQ(std::wstring::npos, text.find(L"\n"));

  // Make the label big so that GetPreferredSize() is not constrained by the
  // current width.
  dangerous_download_label_->SetBounds(0, 0, 1000, 1000);

  gfx::Size size;
  int min_width = -1;
  size_t sp_index = text.find(L" ");
  while (sp_index != std::wstring::npos) {
    text.replace(sp_index, 1, L"\n");
    dangerous_download_label_->SetText(text);
    size = dangerous_download_label_->GetPreferredSize();

    if (min_width == -1)
      min_width = size.width();

    // If the width is growing again, it means we passed the optimal width spot.
    if (size.width() > min_width)
      break;
    else
      min_width = size.width();

    // Restore the string.
    text.replace(sp_index, 1, L" ");

    sp_index = text.find(L" ", sp_index + 1);
  }

  // If we have a line with no space, we won't cut it.
  if (min_width == -1)
    size = dangerous_download_label_->GetPreferredSize();

  dangerous_download_label_->SetBounds(0, 0, size.width(), size.height());
  dangerous_download_label_sized_ = true;
}

void DownloadItemView::Reenable() {
  disabled_while_opening_ = false;
  SetEnabled(true);  // Triggers a repaint.
}

bool DownloadItemView::InDropDownButtonXCoordinateRange(int x) {
  if (x > drop_down_x_left_ && x < drop_down_x_right_)
    return true;
  return false;
}
