// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/download_tab_view.h"

#include <time.h>

#include <algorithm>
#include <functional>

#include "base/file_util.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/time_format.h"
#include "base/timer.h"
#include "grit/theme_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/common/time_format.h"
#include "chrome/views/background.h"
#include "googleurl/src/gurl.h"
#include "generated_resources.h"

using base::Time;
using base::TimeDelta;

// Approximate spacing, in pixels, taken from initial UI mock up screens
static const int kVerticalPadding = 5;
static const int kHorizontalLinkPadding = 15;
static const int kHorizontalButtonPadding = 8;

// For vertical and horizontal element spacing
static const int kSpacer = 20;

// Horizontal space between the left edge of the entries and the
// left edge of the view.
static const int kLeftMargin = 38;

// x-position of the icon (massage this so it visually matches
// kDestinationSearchOffset in native_ui_contents.cc
static const int kDownloadIconOffset = 132;

// Padding between the progress icon and the title, url
static const int kInfoPadding = 16;

// Horizontal distance from the left window edge to the left icon edge
static const int kDateSize = 132;

// Maximum size of the text for the file name or URL
static const int kFilenameSize = 350;

// Maximum size of the progress text during download, which is taken
// out of kFilenameSize
static const int kProgressSize = 170;

// Status label color (grey)
static const SkColor kStatusColor = SkColorSetRGB(128, 128, 128);

// URL label color (green)
static const SkColor kUrlColor = SkColorSetRGB(0, 128, 0);

// Paused download indicator (red)
static const SkColor kPauseColor = SkColorSetRGB(128, 0, 0);

// Warning label color (blue)
static const SkColor kWarningColor = SkColorSetRGB(87, 108, 149);

// Selected item background color
static const SkColor kSelectedItemColor = SkColorSetRGB(215, 232, 255);

// State key used to identify search text.
static const wchar_t kSearchTextKey[] = L"st";

// The maximum number of characters we show in a file name when displaying the
// dangerous download message.
static const int kFileNameMaxLength = 20;

// Sorting functor for DownloadItem --------------------------------------------

// Sort DownloadItems into ascending order by their start time.
class DownloadItemSorter : public std::binary_function<DownloadItem*,
                                                       DownloadItem*,
                                                       bool> {
 public:
  bool operator()(const DownloadItem* lhs, const DownloadItem* rhs) {
    return lhs->start_time() < rhs->start_time();
  }
};


// DownloadItemTabView implementation ------------------------------------------
DownloadItemTabView::DownloadItemTabView()
    : model_(NULL),
      parent_(NULL),
      is_floating_view_renderer_(false) {
  // Create our element views using empty strings for now,
  // set them based on the model's state in Layout().
  since_ = new views::Label(L"");
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont font = rb.GetFont(ResourceBundle::WebFont);
  since_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  since_->SetFont(font);
  AddChildView(since_);

  date_ = new views::Label(L"");
  date_->SetColor(kStatusColor);
  date_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  date_->SetFont(font);
  AddChildView(date_);

  // file_name_ is enabled once the download has finished and we can open
  // it via ShellExecute.
  file_name_ = new views::Link(L"");
  file_name_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  file_name_->SetController(this);
  file_name_->SetFont(font);
  AddChildView(file_name_);

  // dangerous_download_warning_ is enabled when a dangerous download has been
  // initiated.
  dangerous_download_warning_ = new views::Label();
  dangerous_download_warning_ ->SetMultiLine(true);
  dangerous_download_warning_->SetColor(kWarningColor);
  dangerous_download_warning_->SetHorizontalAlignment(
      views::Label::ALIGN_LEFT);
  dangerous_download_warning_->SetFont(font);
  AddChildView(dangerous_download_warning_);

  // The save and discard buttons are shown to prompt the user when a dangerous
  // download was started.
  save_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_SAVE_DOWNLOAD));
  save_button_->set_enforce_dlu_min_size(false);
  save_button_->SetListener(this);
  discard_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_DISCARD_DOWNLOAD));
  discard_button_->SetListener(this);
  discard_button_->set_enforce_dlu_min_size(false);
  AddChildView(save_button_);
  AddChildView(discard_button_);

  // Set our URL name
  download_url_ = new views::Label(L"");
  download_url_->SetColor(kUrlColor);
  download_url_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  download_url_->SetFont(font);
  AddChildView(download_url_);

  // Set our time remaining
  time_remaining_ = new views::Label(L"");
  time_remaining_->SetColor(kStatusColor);
  time_remaining_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  time_remaining_->SetFont(font);
  AddChildView(time_remaining_);

  // Set our download progress
  download_progress_ = new views::Label(L"");
  download_progress_->SetColor(kStatusColor);
  download_progress_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  download_progress_->SetFont(font);
  AddChildView(download_progress_);

  // Set our 'Pause', 'Cancel' and 'Show in folder' links using
  // actual strings, since these are constant
  pause_ = new views::Link(l10n_util::GetString(IDS_DOWNLOAD_LINK_PAUSE));
  pause_->SetController(this);
  pause_->SetFont(font);
  AddChildView(pause_);

  cancel_ = new views::Link(l10n_util::GetString(IDS_DOWNLOAD_LINK_CANCEL));
  cancel_->SetController(this);
  cancel_->SetFont(font);
  AddChildView(cancel_);

  show_ = new views::Link(l10n_util::GetString(IDS_DOWNLOAD_LINK_SHOW));
  show_->SetController(this);
  show_->SetFont(font);
  AddChildView(show_);
}

DownloadItemTabView::~DownloadItemTabView() {
}

void DownloadItemTabView::SetModel(DownloadItem* model,
                                   DownloadTabView* parent) {
  DCHECK(model && parent);
  model_ = model;
  parent_ = parent;
  parent_->LookupIcon(model_);
}

gfx::Size DownloadItemTabView::GetPreferredSize() {
  gfx::Size pause_size = pause_->GetPreferredSize();
  gfx::Size cancel_size = cancel_->GetPreferredSize();
  gfx::Size show_size = show_->GetPreferredSize();
  return gfx::Size(parent_->big_icon_size() +
                   2 * kSpacer +
                   kHorizontalLinkPadding +
                   kFilenameSize +
                   std::max(pause_size.width() + cancel_size.width() +
                            kHorizontalLinkPadding, show_size.width()),
                   parent_->big_icon_size());
}

// Each DownloadItemTabView has reasonably complex layout requirements
// that are based on the state of its model. To make the code much simpler
// to read, Layout() is split into state specific code which will result
// in some redundant code.
void DownloadItemTabView::Layout() {
  DCHECK(model_);
  switch (model_->state()) {
    case DownloadItem::COMPLETE:
      if (model_->safety_state() == DownloadItem::DANGEROUS)
        LayoutPromptDangerousDownload();
      else
        LayoutComplete();
      break;
    case DownloadItem::CANCELLED:
      LayoutCancelled();
      break;
    case DownloadItem::IN_PROGRESS:
      if (model_->safety_state() == DownloadItem::DANGEROUS)
        LayoutPromptDangerousDownload();
      else
        LayoutInProgress();
      break;
    case DownloadItem::REMOVING:
      break;
    default:
      NOTREACHED();
  }
}

// Only display the date if the download is the last that occurred
// on a given day.
void DownloadItemTabView::LayoutDate() {
  if (!parent_->ShouldDrawDateForDownload(model_)) {
    since_->SetVisible(false);
    date_->SetVisible(false);
    return;
  }

  since_->SetText(TimeFormat::RelativeDate(model_->start_time(), NULL));
  gfx::Size since_size = since_->GetPreferredSize();
  since_->SetBounds(kLeftMargin, parent_->big_icon_offset(),
                    kDateSize, since_size.height());
  since_->SetVisible(true);

  date_->SetText(base::TimeFormatShortDate(model_->start_time()));
  gfx::Size date_size = date_->GetPreferredSize();
  date_->SetBounds(kLeftMargin, since_size.height() + kVerticalPadding +
                       parent_->big_icon_offset(),
                   kDateSize, date_size.height());
  date_->SetVisible(true);
}

// DownloadItem::COMPLETE state layout
void DownloadItemTabView::LayoutComplete() {
  // Hide unused UI elements
  pause_->SetVisible(false);
  pause_->SetEnabled(false);
  cancel_->SetVisible(false);
  cancel_->SetEnabled(false);
  time_remaining_->SetVisible(false);
  download_progress_->SetVisible(false);
  dangerous_download_warning_->SetVisible(false);
  save_button_->SetVisible(false);
  save_button_->SetEnabled(false);
  discard_button_->SetVisible(false);
  discard_button_->SetEnabled(false);

  LayoutDate();
  int dx = kDownloadIconOffset - parent_->big_icon_offset() +
           parent_->big_icon_size() + kInfoPadding;

  // File name and URL
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont font = rb.GetFont(ResourceBundle::WebFont);
  file_name_->SetText(
    gfx::ElideFilename(model_->GetFileName().ToWStringHack(), font,
                       kFilenameSize));

  gfx::Size file_name_size = file_name_->GetPreferredSize();

  file_name_->SetBounds(dx, parent_->big_icon_offset(),
                        std::min(kFilenameSize,
                                 static_cast<int>(file_name_size.width())),
                        file_name_size.height());
  file_name_->SetVisible(true);
  file_name_->SetEnabled(true);

  GURL url(model_->url());
  download_url_->SetURL(url);
  gfx::Size url_size = download_url_->GetPreferredSize();
  download_url_->SetBounds(dx,
                           file_name_size.height() + kVerticalPadding +
                           parent_->big_icon_offset(),
                           std::min(kFilenameSize,
                                    static_cast<int>(width() - dx)),
                           url_size.height());
  download_url_->SetVisible(true);
  dx += kFilenameSize + kSpacer;

  // Action button (text is constant and set in constructor)
  gfx::Size show_size = show_->GetPreferredSize();
  show_->SetBounds(dx, ((file_name_size.height() + url_size.height()) / 2) +
                   parent_->big_icon_offset(),
                   show_size.width(), show_size.height());
  show_->SetVisible(true);
  show_->SetEnabled(true);
}

// DownloadItem::CANCELLED state layout
void DownloadItemTabView::LayoutCancelled() {
  // Hide unused UI elements
  show_->SetVisible(false);
  show_->SetEnabled(false);
  pause_->SetVisible(false);
  pause_->SetEnabled(false);
  cancel_->SetVisible(false);
  cancel_->SetEnabled(false);
  dangerous_download_warning_->SetVisible(false);
  save_button_->SetVisible(false);
  save_button_->SetEnabled(false);
  discard_button_->SetVisible(false);
  discard_button_->SetEnabled(false);

  LayoutDate();
  int dx = kDownloadIconOffset - parent_->big_icon_offset() +
           parent_->big_icon_size() + kInfoPadding;

  // File name and URL, truncated to show cancelled status
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont font = rb.GetFont(ResourceBundle::WebFont);
  file_name_->SetText(gfx::ElideFilename(model_->GetFileName().ToWStringHack(),
                                         font, 
                                         kFilenameSize));
  gfx::Size file_name_size = file_name_->GetPreferredSize();
  file_name_->SetBounds(dx, parent_->big_icon_offset(),
                        kFilenameSize - kProgressSize - kSpacer,
                        file_name_size.height());
  file_name_->SetVisible(true);
  file_name_->SetEnabled(false);

  GURL url(model_->url());
  download_url_->SetURL(url);
  gfx::Size url_size = download_url_->GetPreferredSize();
  download_url_->SetBounds(dx,
                           file_name_size.height() + kVerticalPadding +
                           parent_->big_icon_offset(),
                           std::min(kFilenameSize - kProgressSize - kSpacer,
                                    static_cast<int>(width() - dx)),
                           url_size.height());
  download_url_->SetVisible(true);

  dx += kFilenameSize - kProgressSize;

  // Display cancelled status
  time_remaining_->SetColor(kStatusColor);
  time_remaining_->SetText(l10n_util::GetString(IDS_DOWNLOAD_TAB_CANCELLED));
  gfx::Size cancel_size = time_remaining_->GetPreferredSize();
  time_remaining_->SetBounds(dx, parent_->big_icon_offset(),
                             kProgressSize, cancel_size.height());
  time_remaining_->SetVisible(true);

  // Display received size, we may not know the total size if the server didn't
  // provide a content-length.
  int64 total = model_->total_bytes();
  int64 size = model_->received_bytes();
  DataUnits amount_units = GetByteDisplayUnits(size);
  std::wstring received_size = FormatBytes(size, amount_units, true);
  std::wstring amount = received_size;

  // We don't know which string we'll end up using for constructing the final
  // progress string so we need to adjust both strings for the locale
  // direction.
  std::wstring amount_localized;
  if (l10n_util::AdjustStringForLocaleDirection(amount, &amount_localized)) {
    amount.assign(amount_localized);
    received_size.assign(amount_localized);
  }

  if (total > 0) {
    amount_units = GetByteDisplayUnits(total);
    std::wstring total_text = FormatBytes(total, amount_units, true);
    std::wstring total_text_localized;
    if (l10n_util::AdjustStringForLocaleDirection(total_text,
                                                  &total_text_localized))
      total_text.assign(total_text_localized);

    // Note that there is no need to adjust the new amount string for the
    // locale direction as views::Label does that for us.
    amount = l10n_util::GetStringF(IDS_DOWNLOAD_TAB_PROGRESS_SIZE,
                                   received_size,
                                   total_text);
  }

  download_progress_->SetText(amount);
  gfx::Size byte_size = download_progress_->GetPreferredSize();
  download_progress_->SetBounds(dx,
                                file_name_size.height() + kVerticalPadding +
                                parent_->big_icon_offset(),
                                kProgressSize,
                                byte_size.height());
  download_progress_->SetVisible(true);
}

// DownloadItem::IN_PROGRESS state layout
void DownloadItemTabView::LayoutInProgress() {
  // Hide unused UI elements
  show_->SetVisible(false);
  show_->SetEnabled(false);
  dangerous_download_warning_->SetVisible(false);
  save_button_->SetVisible(false);
  save_button_->SetEnabled(false);
  discard_button_->SetVisible(false);
  discard_button_->SetEnabled(false);

  LayoutDate();
  int dx = kDownloadIconOffset - parent_->big_icon_offset() +
           parent_->big_icon_size() + kInfoPadding;

  // File name and URL, truncated to show progress status
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont font = rb.GetFont(ResourceBundle::WebFont);
  file_name_->SetText(gfx::ElideFilename(model_->GetFileName().ToWStringHack(),
                                         font,
                                         kFilenameSize));
  gfx::Size file_name_size = file_name_->GetPreferredSize();
  file_name_->SetBounds(dx, parent_->big_icon_offset(),
                        kFilenameSize - kProgressSize - kSpacer,
                        file_name_size.height());
  file_name_->SetVisible(true);
  file_name_->SetEnabled(false);

  GURL url(model_->url());
  download_url_->SetURL(url);
  gfx::Size url_size = download_url_->GetPreferredSize();
  download_url_->SetBounds(dx, file_name_size.height() + kVerticalPadding +
                           parent_->big_icon_offset(),
                           std::min(kFilenameSize - kProgressSize - kSpacer,
                                    static_cast<int>(width() - dx)),
                           url_size.height());
  download_url_->SetVisible(true);

  dx += kFilenameSize - kProgressSize;

  // Set the time remaining and progress display strings. This can
  // be complicated by not having received the total download size
  // In that case, we can't calculate time remaining so we just
  // display speed and received size.

  // Size
  int64 total = model_->total_bytes();
  int64 size = model_->received_bytes();
  DataUnits amount_units = GetByteDisplayUnits(size);
  std::wstring received_size = FormatBytes(size, amount_units, true);
  std::wstring amount = received_size;

  // Adjust both strings for the locale direction since we don't yet know which
  // string we'll end up using for constructing the final progress string.
  std::wstring amount_localized;
  if (l10n_util::AdjustStringForLocaleDirection(amount, &amount_localized)) {
    amount.assign(amount_localized);
    received_size.assign(amount_localized);
  }

  if (total > 0) {
    amount_units = GetByteDisplayUnits(total);
    std::wstring total_text = FormatBytes(total, amount_units, true);
    std::wstring total_text_localized;
    if (l10n_util::AdjustStringForLocaleDirection(total_text,
                                                  &total_text_localized))
      total_text.assign(total_text_localized);

    amount = l10n_util::GetStringF(IDS_DOWNLOAD_TAB_PROGRESS_SIZE,
                                   received_size,
                                   total_text);

    // We adjust the 'amount' string in case we use it as part of the progress
    // text.
    if (l10n_util::AdjustStringForLocaleDirection(amount, &amount_localized))
      amount.assign(amount_localized);
  }

  // Speed
  int64 speed = model_->CurrentSpeed();
  std::wstring progress = amount;
  if (!model_->is_paused() && speed > 0) {
    amount_units = GetByteDisplayUnits(speed);
    std::wstring speed_text = FormatSpeed(speed, amount_units, true);
    std::wstring speed_text_localized;
    if (l10n_util::AdjustStringForLocaleDirection(speed_text,
                                                  &speed_text_localized))
      speed_text.assign(speed_text_localized);

    progress = l10n_util::GetStringF(IDS_DOWNLOAD_TAB_PROGRESS_SPEED,
                                     speed_text,
                                     amount);

    // For some reason, the appearance of the dash character ('-') in a string
    // causes Windows to ignore the 'LRE'/'RLE'/'PDF' Unicode formatting
    // characters within the string and this causes the string to be displayed
    // incorrectly on RTL UIs. Therefore, we add the Unicode right-to-left
    // override character (U+202E) if the locale is RTL in order to fix this
    // problem.
    if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
      progress.insert(0, L"\x202E");
  }

  // Time remaining
  int y_pos = file_name_size.height() + kVerticalPadding +
              parent_->big_icon_offset();
  gfx::Size time_size;
  time_remaining_->SetColor(kStatusColor);
  if (model_->is_paused()) {
    time_remaining_->SetColor(kPauseColor);
    time_remaining_->SetText(
        l10n_util::GetString(IDS_DOWNLOAD_PROGRESS_PAUSED));
    time_size = time_remaining_->GetPreferredSize();
    time_remaining_->SetBounds(dx, parent_->big_icon_offset(),
                               kProgressSize, time_size.height());
    time_remaining_->SetVisible(true);
  } else if (total > 0)  {
    TimeDelta remaining;
    if (model_->TimeRemaining(&remaining))
      time_remaining_->SetText(TimeFormat::TimeRemaining(remaining));
    time_size = time_remaining_->GetPreferredSize();
    time_remaining_->SetBounds(dx, parent_->big_icon_offset(),
                               kProgressSize, time_size.height());
    time_remaining_->SetVisible(true);
  } else {
    time_remaining_->SetText(L"");
    y_pos = ((file_name_size.height() + url_size.height()) / 2) +
            parent_->big_icon_offset();
  }

  download_progress_->SetText(progress);
  gfx::Size byte_size = download_progress_->GetPreferredSize();
  download_progress_->SetBounds(dx, y_pos,
                                kProgressSize, byte_size.height());
  download_progress_->SetVisible(true);

  dx += kProgressSize + kSpacer;
  y_pos = ((file_name_size.height() + url_size.height()) / 2) +
          parent_->big_icon_offset();

  // Pause (or Resume) / Cancel buttons.
  if (model_->is_paused())
    pause_->SetText(l10n_util::GetString(IDS_DOWNLOAD_LINK_RESUME));
  else
    pause_->SetText(l10n_util::GetString(IDS_DOWNLOAD_LINK_PAUSE));

  pause_->SetVisible(true);
  pause_->SetEnabled(true);
  gfx::Size pause_size = pause_->GetPreferredSize();
  pause_->SetBounds(dx, y_pos, pause_size.width(), pause_size.height());

  dx += pause_size.width() + kHorizontalLinkPadding;

  gfx::Size cancel_size = cancel_->GetPreferredSize();
  cancel_->SetBounds(dx, y_pos, cancel_size.width(), cancel_size.height());
  cancel_->SetVisible(true);
  cancel_->SetEnabled(true);
}

void DownloadItemTabView::LayoutPromptDangerousDownload() {
  // Hide unused UI elements
  show_->SetVisible(false);
  show_->SetEnabled(false);
  file_name_->SetVisible(false);
  file_name_->SetEnabled(false);
  pause_->SetVisible(false);
  pause_->SetEnabled(false);
  cancel_->SetVisible(false);
  cancel_->SetEnabled(false);
  time_remaining_->SetVisible(false);
  download_progress_->SetVisible(false);

  LayoutDate();
  int dx = kDownloadIconOffset - parent_->big_icon_offset() +
           parent_->big_icon_size() + kInfoPadding;

  // Warning message and URL.
  std::wstring file_name;
  ElideString(model_->original_name().ToWStringHack(), kFileNameMaxLength, &file_name);
  dangerous_download_warning_->SetText(
      l10n_util::GetStringF(IDS_PROMPT_DANGEROUS_DOWNLOAD, file_name));
  gfx::Size warning_size = dangerous_download_warning_->GetPreferredSize();
  dangerous_download_warning_->SetBounds(dx, 0,
                                         kFilenameSize, warning_size.height());
  dangerous_download_warning_->SetVisible(true);

  GURL url(model_->url());
  download_url_->SetURL(url);
  gfx::Size url_size = download_url_->GetPreferredSize();
  download_url_->SetBounds(dx, height() - url_size.height(),
                           std::min(kFilenameSize - kSpacer,
                                    static_cast<int>(width() - dx)),
                           url_size.height());
  download_url_->SetVisible(true);

  dx += kFilenameSize + kSpacer;

  // Save/Discard buttons.
  gfx::Size button_size = save_button_->GetPreferredSize();
  save_button_->SetBounds(dx, (height() - button_size.height()) / 2,
                          button_size.width(), button_size.height());
  save_button_->SetVisible(true);
  save_button_->SetEnabled(true);

  dx += button_size.width() + kHorizontalButtonPadding;

  button_size = discard_button_->GetPreferredSize();
  discard_button_->SetBounds(dx, (height() - button_size.height()) / 2,
                             button_size.width(), button_size.height());
  discard_button_->SetVisible(true);
  discard_button_->SetEnabled(true);
}

void DownloadItemTabView::Paint(ChromeCanvas* canvas) {
  PaintBackground(canvas);

  if (model_->state() == DownloadItem::IN_PROGRESS  &&
      model_->safety_state() != DownloadItem::DANGEROUS) {
    // For most languages, 'offset' will be 0. For languages where the dangerous
    // download warning is longer than usual, the download view will be slightly
    // larger and 'offset' will be positive value that lines up the progress
    // halo and the file's icon in order to accomodate the larger view.
    int offset = (parent_->big_icon_size() -
                  download_util::kBigProgressIconSize) / 2;
    download_util::PaintDownloadProgress(
        canvas,
        this,
        offset + kDownloadIconOffset -
        parent_->big_icon_offset(),
        offset,
        parent_->start_angle(),
        model_->PercentComplete(),
        download_util::BIG);
  }

  // Most of the UI elements in the DownloadItemTabView are represented as
  // child Views and therefore they get mirrored automatically in
  // right-to-left UIs. The download item icon is not contained within a child
  // View so we need to mirror it manually if the locale is RTL.
  SkBitmap* icon = parent_->LookupIcon(model_);
  if (icon) {
    gfx::Rect icon_bounds(kDownloadIconOffset,
                          parent_->big_icon_offset(),
                          icon->width(), icon->height());
    icon_bounds.set_x(MirroredLeftPointForRect(icon_bounds));
    canvas->DrawBitmapInt(*icon, icon_bounds.x(), icon_bounds.y());
  }
}

void DownloadItemTabView::PaintBackground(ChromeCanvas* canvas) {
  if (parent_->ItemIsSelected(model_)) {
    // Before we paint the border and the focus rect, we need to mirror the
    // highlighted area if the View is using a right-to-left UI layout. We need
    // to explicitly mirror the position because the highlighted area is
    // directly painted on the canvas (as opposed to being represented as a
    // child View like the rest of the UI elements in DownloadItemTabView).
    gfx::Rect highlighted_bounds(kDownloadIconOffset -
                                 parent_->big_icon_offset(),
                                 0,
                                 parent_->big_icon_size() +
                                 kInfoPadding + kFilenameSize,
                                 parent_->big_icon_size());
    highlighted_bounds.set_x(MirroredLeftPointForRect(highlighted_bounds));

    canvas->FillRectInt(kSelectedItemColor,
                        highlighted_bounds.x(),
                        highlighted_bounds.y(),
                        highlighted_bounds.width(),
                        highlighted_bounds.height());

    canvas->DrawFocusRect(highlighted_bounds.x(),
                          highlighted_bounds.y(),
                          highlighted_bounds.width(),
                          highlighted_bounds.height());
  }
}

bool DownloadItemTabView::OnMousePressed(const views::MouseEvent& event) {
  gfx::Point point(event.location());

  // If the click is in the highlight region, then highlight this download.
  // Otherwise, remove the highlighting from any download.
  gfx::Rect select_rect(
      kDownloadIconOffset - parent_->big_icon_offset(),
      0,
      kDownloadIconOffset - parent_->big_icon_offset() +
      parent_->big_icon_size() + kInfoPadding + kFilenameSize,
      parent_->big_icon_size());

  // The position of the highlighted region does not take into account the
  // View's UI layout so we have to manually mirror the position if the View is
  // using a right-to-left UI layout.
  gfx::Rect mirrored_rect(select_rect);
  select_rect.set_x(MirroredLeftPointForRect(mirrored_rect));
  if (select_rect.Contains(point)) {
    parent_->ItemBecameSelected(model_);

    // Don't show the right-click menu if we are prompting the user for a
    // dangerous download.
    if (event.IsRightMouseButton() &&
        model_->safety_state() != DownloadItem::DANGEROUS) {
      views::View::ConvertPointToScreen(this, &point);

      download_util::DownloadDestinationContextMenu menu(
          model_, GetWidget()->GetHWND(), point.ToPOINT());
    }
  } else {
    parent_->ItemBecameSelected(NULL);
  }

  return true;
}

// Handle drag (file copy) operations.
bool DownloadItemTabView::OnMouseDragged(const views::MouseEvent& event) {
  if (model_->state() != DownloadItem::COMPLETE ||
      model_->safety_state() == DownloadItem::DANGEROUS)
    return false;

  CPoint point(event.x(), event.y());

  // In order to make sure drag and drop works as expected when the UI is
  // mirrored, we can either flip the mouse X coordinate or flip the X position
  // of the drag rectangle. Flipping the mouse X coordinate is easier.
  point.x = MirroredXCoordinateInsideView(point.x);
  CRect drag_rect(kDownloadIconOffset -
                  parent_->big_icon_offset(),
                  0,
                  kDownloadIconOffset -
                  parent_->big_icon_offset() +
                  parent_->big_icon_size() + kInfoPadding +
                  kFilenameSize,
                  parent_->big_icon_size());

  if (drag_rect.PtInRect(point)) {
    SkBitmap* icon = parent_->LookupIcon(model_);
    if (icon)
      download_util::DragDownload(model_, icon);
  }

  return true;
}

void DownloadItemTabView::LinkActivated(views::Link* source, int event_flags) {
  // There are several links in our view that could have been clicked:
  if (source == file_name_) {
    views::Widget* widget = this->GetWidget();
    HWND parent_window = widget ? widget->GetHWND() : NULL;
    model_->manager()->OpenDownloadInShell(model_, parent_window);
  } else if (source == pause_) {
    model_->TogglePause();
  } else if (source == cancel_) {
    model_->Cancel(true /* update history service */);
  } else if (source == show_) {
    model_->manager()->ShowDownloadInShell(model_);
  } else {
    NOTREACHED();
  }

  parent_->ItemBecameSelected(model_);
}

void DownloadItemTabView::ButtonPressed(views::NativeButton* sender) {
  if (sender == save_button_) {
    parent_->model()->DangerousDownloadValidated(model_);
    // Relayout and repaint to display the right mode (complete or in progress).
    Layout();
    SchedulePaint();
  } else if (sender == discard_button_) {
    model_->Remove(true);
  } else  {
    NOTREACHED();
  }
}

// DownloadTabView implementation ----------------------------------------------

DownloadTabView::DownloadTabView(DownloadManager* model)
    : model_(model),
      big_icon_size_(download_util::GetBigProgressIconSize()),
      big_icon_offset_(download_util::GetBigProgressIconOffset()),
      start_angle_(download_util::kStartAngleDegrees),
      scroll_helper_(kSpacer, big_icon_size_ + kSpacer),
      selected_index_(-1) {
  DCHECK(model_);
}

DownloadTabView::~DownloadTabView() {
  StopDownloadProgress();
  model_->RemoveObserver(this);

  // DownloadManager owns the contents.
  for (OrderedDownloads::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  downloads_.clear();
  ClearDownloadInProgress();
  ClearDangerousDownloads();


  icon_consumer_.CancelAllRequests();
}

void DownloadTabView::Initialize() {
  model_->AddObserver(this);
}

// Start progress animation timers when we get our first (in-progress) download.
void DownloadTabView::StartDownloadProgress() {
  if (progress_timer_.IsRunning())
    return;
  progress_timer_.Start(
      TimeDelta::FromMilliseconds(download_util::kProgressRateMs), this,
      &DownloadTabView::UpdateDownloadProgress);
}

// Stop progress animation when there are no more in-progress downloads.
void DownloadTabView::StopDownloadProgress() {
  progress_timer_.Stop();
}

// Update our animations.
void DownloadTabView::UpdateDownloadProgress() {
  start_angle_ = (start_angle_ + download_util::kUnknownIncrementDegrees) %
                 download_util::kMaxDegrees;
  SchedulePaint();
}

void DownloadTabView::Layout() {
  DetachAllFloatingViews();
  // Dangerous downloads items use NativeButtons, so they need to be attached
  // as NativeControls are not supported yet in floating views.
  gfx::Rect visible_bounds = GetVisibleBounds();
  int row_start = (visible_bounds.y() - kSpacer) /
                  (big_icon_size_ + kSpacer);
  int row_stop = (visible_bounds.y() - kSpacer + visible_bounds.height()) /
                 (big_icon_size_ + kSpacer);
  row_stop = std::min(row_stop, static_cast<int>(downloads_.size()) - 1);
  for (int i = row_start; i <= row_stop; ++i) {
    // The DownloadManager stores downloads earliest first, but this view
    // displays latest first, so adjust the index:
    int index = static_cast<int>(downloads_.size()) - 1 - i;
    if (downloads_[index]->safety_state() == DownloadItem::DANGEROUS)
      ValidateFloatingViewForID(index);
  }
  View* v = GetParent();
  if (v) {
    int h = static_cast<int>(downloads_.size()) *
            (big_icon_size_ + kSpacer) + kSpacer;
    SetBounds(x(), y(), v->width(), h);
  }
}

// Paint our scrolled region
void DownloadTabView::Paint(ChromeCanvas* canvas) {
  views::View::Paint(canvas);

  if (download_util::kBigIconSize == 0 || downloads_.size() == 0)
    return;

  SkRect clip;
  if (canvas->getClipBounds(&clip)) {
    int row_start = (SkScalarRound(clip.fTop) - kSpacer) /
                    (big_icon_size_ + kSpacer);
    int row_stop =
        std::min(static_cast<int>(downloads_.size()) - 1,
                 (SkScalarRound(clip.fBottom) - kSpacer) /
                 (big_icon_size_ + kSpacer));
    SkRect download_rect;
    for (int i = row_start; i <= row_stop; ++i) {
      int y = i * (big_icon_size_ + kSpacer) + kSpacer;
      if (HasFloatingViewForPoint(0, y))
        continue;
      download_rect.set(SkIntToScalar(0),
                        SkIntToScalar(y),
                        SkIntToScalar(width()),
                        SkIntToScalar(y + big_icon_size_));
      if (SkRect::Intersects(clip, download_rect)) {
        // The DownloadManager stores downloads earliest first, but this
        // view displays latest first, so adjust the index:
        int index = static_cast<int>(downloads_.size()) - 1 - i;
        download_renderer_.SetModel(downloads_[index], this);
        PaintFloatingView(canvas, &download_renderer_,
                          0, y, width(), big_icon_size_);
      }
    }
  }
}

// Draw the DownloadItemTabView for the current position.
bool DownloadTabView::GetFloatingViewIDForPoint(int x, int y, int* id) {
  if (y < kSpacer ||
      y > (kSpacer + big_icon_size_) * static_cast<int>(downloads_.size()))
    return false;

  // Are we hovering over a download or the spacer? If we're over the
  // download, create a floating view for it.
  if ((y - kSpacer) % (big_icon_size_ + kSpacer) < big_icon_size_) {
    int row = y / (big_icon_size_ + kSpacer);
    *id = static_cast<int>(downloads_.size()) - 1 - row;
    return true;
  }
  return false;
}

views::View* DownloadTabView::CreateFloatingViewForIndex(int index) {
  if (index >= static_cast<int>(downloads_.size())) {
    // It's possible that the downloads have been cleared via the "Clear
    // Browsing Data" command, so this index is gone.
    return NULL;
  }

  DownloadItemTabView* dl = new DownloadItemTabView();
  // We attach the view before layout as the Save/Discard buttons are native
  // and need to be in the tree hierarchy to compute their preferred size
  // correctly.
  AttachFloatingView(dl, index);
  dl->SetModel(downloads_[index], this);
  int row = static_cast<int>(downloads_.size()) - 1 - index;
  int y_pos = row * (big_icon_size_ + kSpacer) + kSpacer;
  dl->SetBounds(0, y_pos, width(), big_icon_size_);
  dl->Layout();
  return dl;
}

bool DownloadTabView::EnumerateFloatingViews(
      views::View::FloatingViewPosition position,
      int starting_id, int* id) {
  DCHECK(id);
  return View::EnumerateFloatingViewsForInterval(
      0, static_cast<int>(downloads_.size()), false, position, starting_id, id);
}

views::View* DownloadTabView::ValidateFloatingViewForID(int id) {
  return CreateFloatingViewForIndex(id);
}

void DownloadTabView::OnDownloadUpdated(DownloadItem* download) {
  switch (download->state()) {
    case DownloadItem::COMPLETE:
    case DownloadItem::CANCELLED: {
      base::hash_set<DownloadItem*>::iterator d = in_progress_.find(download);
      if (d != in_progress_.end()) {
        // If this is a dangerous download not yet validated by the user, we
        // still need to be notified when the validation happens.
        if (download->safety_state() != DownloadItem::DANGEROUS) {
          (*d)->RemoveObserver(this);
        } else {
          // Add the download to dangerous_downloads_ so we call RemoveObserver
          // on ClearDangerousDownloads().
          dangerous_downloads_.insert(download);
        }
        in_progress_.erase(d);
      }
      if (in_progress_.empty())
        StopDownloadProgress();
      LoadIcon(download);
      break;
    }
    case DownloadItem::IN_PROGRESS: {
      // If all IN_PROGRESS downloads are paused, don't waste CPU issuing any
      // further progress updates until at least one download is active again.
      if (download->is_paused()) {
        bool continue_update = false;
        base::hash_set<DownloadItem*>::iterator it = in_progress_.begin();
        for (; it != in_progress_.end(); ++it) {
          if (!(*it)->is_paused()) {
            continue_update = true;
            break;
          }
        }
        if (!continue_update)
          StopDownloadProgress();
      } else {
        StartDownloadProgress();
      }
      break;
    }
    case DownloadItem::REMOVING:
      // Handled below.
      break;
    default:
      NOTREACHED();
      break;
  }

  OrderedDownloads::iterator it = find(downloads_.begin(),
                                       downloads_.end(),
                                       download);
  if (it == downloads_.end())
    return;

  const int index = static_cast<int>(it - downloads_.begin());
  DownloadItemTabView* view =
      static_cast<DownloadItemTabView*>(RetrieveFloatingViewForID(index));
  if (view) {
    if (download->state() != DownloadItem::REMOVING) {
      view->Layout();
      SchedulePaintForViewAtIndex(index);
    } else if (selected_index_ == index) {
      selected_index_ = -1;
    }
  }
}

// A download has started or been deleted. Query our DownloadManager for the
// current set of downloads, which will call us back in SetDownloads once it
// has retrieved them.
void DownloadTabView::ModelChanged() {
  downloads_.clear();
  ClearDownloadInProgress();
  ClearDangerousDownloads();
  DetachAllFloatingViews();

  // Issue the query.
  model_->GetDownloads(this, search_text_);
}

void DownloadTabView::SetDownloads(std::vector<DownloadItem*>& downloads) {
  // Stop progress timers.
  StopDownloadProgress();

  // Clear out old state and remove self as observer for each download.
  downloads_.clear();
  ClearDownloadInProgress();
  ClearDangerousDownloads();

  // Swap new downloads in.
  downloads_.swap(downloads);
  sort(downloads_.begin(), downloads_.end(), DownloadItemSorter());

  // Scan for any in progress downloads and add ourself to them as an observer.
  for (OrderedDownloads::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    DownloadItem* download = *it;
    if (download->state() == DownloadItem::IN_PROGRESS) {
      download->AddObserver(this);
      in_progress_.insert(download);
    } else if (download->safety_state() == DownloadItem::DANGEROUS) {
      // We need to be notified when the user validates the dangerous download.
      download->AddObserver(this);
      dangerous_downloads_.insert(download);
    }
  }

  // Start any progress timers if required.
  if (!in_progress_.empty())
    StartDownloadProgress();

  // Update the UI.
  selected_index_ = -1;
  GetParent()->GetParent()->Layout();
  SchedulePaint();
}


// If we have the icon in our cache, then return it. If not, look it up via the
// IconManager. Ignore in progress requests (duplicates).
SkBitmap* DownloadTabView::LookupIcon(DownloadItem* download) {
  IconManager* im = g_browser_process->icon_manager();
  // Fast look up.
  SkBitmap* icon = im->LookupIcon(download->full_path().ToWStringHack(),
                                  IconLoader::NORMAL);

  // Expensive look up.
  if (!icon)
    LoadIcon(download);

  return icon;
}

// Bypass the caches and perform the Icon extraction directly. This is useful in
// the case where the download has completed and we want to re-check the file
// to see if it has an embedded icon (which we couldn't do at download start).
void DownloadTabView::LoadIcon(DownloadItem* download) {
  IconManager* im = g_browser_process->icon_manager();
  IconManager::Handle h =
      im->LoadIcon(download->full_path().ToWStringHack(), IconLoader::NORMAL,
                   &icon_consumer_,
                   NewCallback(this, &DownloadTabView::OnExtractIconComplete));
  icon_consumer_.SetClientData(im, h, download);
}

void DownloadTabView::ClearDownloadInProgress() {
  for (base::hash_set<DownloadItem*>::iterator it = in_progress_.begin();
       it != in_progress_.end(); ++it)
    (*it)->RemoveObserver(this);
  in_progress_.clear();
}

void DownloadTabView::ClearDangerousDownloads() {
  base::hash_set<DownloadItem*>::const_iterator it;
  for (it = dangerous_downloads_.begin();
       it != dangerous_downloads_.end(); ++it)
    (*it)->RemoveObserver(this);
  dangerous_downloads_.clear();
}

// Check to see if the download is the latest download on a given day.
// We use this to determine when to draw the date next to a particular
// download view: if the DownloadItem is the latest download on a given
// day, the date gets drawn.
bool DownloadTabView::ShouldDrawDateForDownload(DownloadItem* download) {
  DCHECK(download);
  OrderedDownloads::iterator it = find(downloads_.begin(),
                                       downloads_.end(),
                                       download);
  DCHECK(it != downloads_.end());
  const int index = static_cast<int>(it - downloads_.begin());

  // If download is the last or only download, it draws the date.
  if (downloads_.size() - 1 == index)
    return true;

  const DownloadItem* next = downloads_[index + 1];

  Time next_midnight = next->start_time().LocalMidnight();
  Time curr_midnight = download->start_time().LocalMidnight();
  if (next_midnight == curr_midnight) {
    // 'next' happened today: let it draw the date so we don't have to.
    return false;
  }
  return true;
}

int DownloadTabView::GetPageScrollIncrement(
    views::ScrollView* scroll_view, bool is_horizontal,
    bool is_positive) {
  return scroll_helper_.GetPageScrollIncrement(scroll_view, is_horizontal,
                                               is_positive);
}

int DownloadTabView::GetLineScrollIncrement(
    views::ScrollView* scroll_view, bool is_horizontal,
    bool is_positive) {
  return scroll_helper_.GetLineScrollIncrement(scroll_view, is_horizontal,
                                               is_positive);
}

void DownloadTabView::ItemBecameSelected(const DownloadItem* download) {
  int index = -1;
  if (download != NULL) {
    OrderedDownloads::const_iterator it = find(downloads_.begin(),
                                               downloads_.end(),
                                               download);
    DCHECK(it != downloads_.end());
    index = static_cast<int>(it - downloads_.begin());
    if (index == selected_index_)
      return;  // Avoid unnecessary paint.
  }

  if (selected_index_ >= 0)
    SchedulePaintForViewAtIndex(selected_index_);
  if (index >= 0)
    SchedulePaintForViewAtIndex(index);
  selected_index_ = index;
}

bool DownloadTabView::ItemIsSelected(DownloadItem* download) {
  OrderedDownloads::iterator it = find(downloads_.begin(),
                                       downloads_.end(),
                                       download);
  if (it != downloads_.end())
    return selected_index_ == static_cast<int>(it - downloads_.begin());
  return false;
}

void DownloadTabView::SchedulePaintForViewAtIndex(int index) {
  int y = GetYPositionForIndex(index);
  SchedulePaint(0, y, width(), big_icon_size_);
}

int DownloadTabView::GetYPositionForIndex(int index) {
  int row = static_cast<int>(downloads_.size()) - 1 - index;
  return row * (big_icon_size_ + kSpacer) + kSpacer;
}

void DownloadTabView::SetSearchText(const std::wstring& search_text) {
  search_text_ = search_text;
  model_->GetDownloads(this, search_text_);
}

// The 'icon_bitmap' is ignored here, since it is cached by the IconManager.
// When the paint message runs, we'll use the fast IconManager lookup API to
// retrieve it.
void DownloadTabView::OnExtractIconComplete(IconManager::Handle handle,
                                            SkBitmap* icon_bitmap) {
  IconManager* im = g_browser_process->icon_manager();
  DownloadItem* download = icon_consumer_.GetClientData(im, handle);
  OrderedDownloads::iterator it = find(downloads_.begin(),
                                       downloads_.end(),
                                       download);
  if (it != downloads_.end()) {
    const int index = static_cast<int>(it - downloads_.begin());
    SchedulePaintForViewAtIndex(index);
  }
}

// DownloadTabUIFactory ------------------------------------------------------

class DownloadTabUIFactory : public NativeUIFactory {
 public:
  DownloadTabUIFactory() {}
  virtual ~DownloadTabUIFactory() {}

  virtual NativeUI* CreateNativeUIForURL(const GURL& url,
                                         NativeUIContents* contents) {
    return new DownloadTabUI(contents);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DownloadTabUIFactory);
};

// DownloadTabUI -------------------------------------------------------------

DownloadTabUI::DownloadTabUI(NativeUIContents* contents)
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
    : searchable_container_(this),
      download_tab_view_(NULL),
      contents_(contents) {
  DownloadManager* dlm = contents_->profile()->GetDownloadManager();
  download_tab_view_ = new DownloadTabView(dlm);
  searchable_container_.SetContents(download_tab_view_);
  download_tab_view_->Initialize();

  NotificationService* ns = NotificationService::current();
  ns->AddObserver(this, NotificationType::DOWNLOAD_START,
                  NotificationService::AllSources());
  ns->AddObserver(this, NotificationType::DOWNLOAD_STOP,
                  NotificationService::AllSources());

  // Spin the throbber if there are active downloads, since we may have been
  // created after the NOTIFY_DOWNLOAD_START was sent. If the download manager
  // has not been created, don't bother since it will negatively impact start
  // up time with history requests.
  Profile* profile = contents_->profile();
  if (profile &&
      profile->HasCreatedDownloadManager() &&
      profile->GetDownloadManager()->in_progress_count() > 0)
    contents_->SetIsLoading(true, NULL);
}

DownloadTabUI::~DownloadTabUI() {
  NotificationService* ns = NotificationService::current();
  ns->RemoveObserver(this, NotificationType::DOWNLOAD_START,
                     NotificationService::AllSources());
  ns->RemoveObserver(this, NotificationType::DOWNLOAD_STOP,
                     NotificationService::AllSources());
}

const std::wstring DownloadTabUI::GetTitle() const {
  return l10n_util::GetString(IDS_DOWNLOAD_TITLE);
}

const int DownloadTabUI::GetFavIconID() const {
  return IDR_DOWNLOADS_FAVICON;
}

const int DownloadTabUI::GetSectionIconID() const {
  return IDR_DOWNLOADS_SECTION;
}

const std::wstring DownloadTabUI::GetSearchButtonText() const {
  return l10n_util::GetString(IDS_DOWNLOAD_SEARCH_BUTTON);
}

views::View* DownloadTabUI::GetView() {
  return &searchable_container_;
}

void DownloadTabUI::WillBecomeVisible(NativeUIContents* parent) {
  UserMetrics::RecordAction(L"Destination_Downloads", parent->profile());
}

void DownloadTabUI::WillBecomeInvisible(NativeUIContents* parent) {
}

void DownloadTabUI::Navigate(const PageState& state) {
  std::wstring search_text;
  state.GetProperty(kSearchTextKey, &search_text);
  download_tab_view_->SetSearchText(search_text);
  searchable_container_.GetSearchField()->SetText(search_text);
}

bool DownloadTabUI::SetInitialFocus() {
  searchable_container_.GetSearchField()->RequestFocus();
  return true;
}

// static
GURL DownloadTabUI::GetURL() {
  std::string spec(NativeUIContents::GetScheme());
  spec.append("://downloads");
  return GURL(spec);
}

// static
NativeUIFactory* DownloadTabUI::GetNativeUIFactory() {
  return new DownloadTabUIFactory();
}

void DownloadTabUI::DoSearch(const std::wstring& new_text) {
  download_tab_view_->SetSearchText(new_text);
  PageState* page_state = contents_->page_state().Copy();
  page_state->SetProperty(kSearchTextKey, new_text);
  contents_->SetPageState(page_state);
}

void DownloadTabUI::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::DOWNLOAD_START:
    case NotificationType::DOWNLOAD_STOP:
      DCHECK(profile()->HasCreatedDownloadManager());
      contents_->SetIsLoading(
          profile()->GetDownloadManager()->in_progress_count() > 0,
          NULL);
      break;
    default:
      break;
  }
}
