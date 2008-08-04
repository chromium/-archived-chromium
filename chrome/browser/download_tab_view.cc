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

#include "chrome/browser/download_tab_view.h"

#include <time.h>

#include <algorithm>
#include <functional>

#include "base/file_util.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/timer.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download_manager.h"
#include "chrome/browser/download_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/common/time_format.h"
#include "chrome/views/background.h"
#include "googleurl/src/gurl.h"
#include "generated_resources.h"

// Approximate spacing, in pixels, taken from initial UI mock up screens
static const int kVerticalPadding = 5;
static const int kHorizontalButtonPadding = 15;

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

// Selected item background color
static const SkColor kSelectedItemColor = SkColorSetRGB(215, 232, 255);

// State key used to identify search text.
static const wchar_t kSearchTextKey[] = L"st";

// The size of the icon displayed.
static const int kIconSize =
    download_util::GetProgressIconSize(download_util::BIG);

// Size of the progress halo icon.
static const int kProgressIconSize =
    download_util::GetProgressIconSize(download_util::BIG);

// Centering the icon in the progress bitmaps
static const int kIconOffset =
    download_util::GetProgressIconOffset(download_util::BIG);

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
      parent_(NULL) {
  // Create our element views using empty strings for now,
  // set them based on the model's state in Layout().
  since_ = new ChromeViews::Label(L"");
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont font = rb.GetFont(ResourceBundle::WebFont);
  since_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  since_->SetFont(font);
  AddChildView(since_);

  date_ = new ChromeViews::Label(L"");
  date_->SetColor(kStatusColor);
  date_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  date_->SetFont(font);
  AddChildView(date_);

  // file_name_ is enabled once the download has finished and we can open
  // it via ShellExecute.
  file_name_ = new ChromeViews::Link(L"");
  file_name_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  file_name_->SetController(this);
  file_name_->SetFont(font);
  AddChildView(file_name_);

  // Set our URL name
  download_url_ = new ChromeViews::Label(L"");
  download_url_->SetColor(kUrlColor);
  download_url_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  download_url_->SetFont(font);
  AddChildView(download_url_);

  // Set our time remaining
  time_remaining_ = new ChromeViews::Label(L"");
  time_remaining_->SetColor(kStatusColor);
  time_remaining_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  time_remaining_->SetFont(font);
  AddChildView(time_remaining_);

  // Set our download progress
  download_progress_ = new ChromeViews::Label(L"");
  download_progress_->SetColor(kStatusColor);
  download_progress_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  download_progress_->SetFont(font);
  AddChildView(download_progress_);

  // Set our 'Pause', 'Cancel' and 'Show in folder' links using
  // actual strings, since these are constant
  pause_ = new ChromeViews::Link(l10n_util::GetString(IDS_DOWNLOAD_LINK_PAUSE));
  pause_->SetController(this);
  pause_->SetFont(font);
  AddChildView(pause_);

  cancel_ = new ChromeViews::Link(
                  l10n_util::GetString(IDS_DOWNLOAD_LINK_CANCEL));
  cancel_->SetController(this);
  cancel_->SetFont(font);
  AddChildView(cancel_);

  show_ = new ChromeViews::Link(l10n_util::GetString(IDS_DOWNLOAD_LINK_SHOW));
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

void DownloadItemTabView::GetPreferredSize(CSize* out) {
  CSize pause_size;
  pause_->GetPreferredSize(&pause_size);
  CSize cancel_size;
  cancel_->GetPreferredSize(&cancel_size);
  CSize show_size;
  show_->GetPreferredSize(&show_size);

  out->cx = kProgressIconSize +
            2 * kSpacer +
            kHorizontalButtonPadding +
            kFilenameSize +
            std::max(pause_size.cx + cancel_size.cx + kHorizontalButtonPadding,
                     show_size.cx);

  out->cy = kProgressIconSize;
}

// Each DownloadItemTabView has reasonably complex layout requirements
// that are based on the state of its model. To make the code much simpler
// to read, Layout() is split into state specific code which will result
// in some redundant code.
void DownloadItemTabView::Layout() {
  DCHECK(model_);
  switch (model_->state()) {
    case DownloadItem::COMPLETE:
      LayoutComplete();
      break;
    case DownloadItem::CANCELLED:
      LayoutCancelled();
      break;
    case DownloadItem::IN_PROGRESS:
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

  CSize since_size;

  since_->SetText(TimeFormat::FriendlyDay(model_->start_time(), NULL));
  since_->GetPreferredSize(&since_size);
  since_->SetBounds(kLeftMargin, kIconOffset, kDateSize, since_size.cy);
  since_->SetVisible(true);

  CSize date_size;
  date_->SetText(TimeFormat::ShortDate(model_->start_time()));
  date_->GetPreferredSize(&date_size);
  date_->SetBounds(kLeftMargin, since_size.cy + kVerticalPadding + kIconOffset,
                   kDateSize, date_size.cy);
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

  LayoutDate();
  int dx = kDownloadIconOffset - kIconOffset + kProgressIconSize +
           kInfoPadding;

  // File name and URL
  CSize file_name_size;
  file_name_->SetText(model_->file_name());
  file_name_->GetPreferredSize(&file_name_size);
  file_name_->SetBounds(dx, kIconOffset,
                        std::min(kFilenameSize,
                                 static_cast<int>(file_name_size.cx)),
                        file_name_size.cy);
  file_name_->SetVisible(true);
  file_name_->SetEnabled(true);

  GURL url(model_->url());
  download_url_->SetURL(url);
  CSize url_size;
  download_url_->GetPreferredSize(&url_size);
  download_url_->SetBounds(dx,
                           file_name_size.cy + kVerticalPadding + kIconOffset,
                           std::min(kFilenameSize,
                                    static_cast<int>(GetWidth() - dx)),
                           url_size.cy);
  download_url_->SetVisible(true);
  dx += kFilenameSize + kSpacer;

  // Action button (text is constant and set in constructor)
  CSize show_size;
  show_->GetPreferredSize(&show_size);
  show_->SetBounds(dx, ((file_name_size.cy + url_size.cy) / 2) + kIconOffset,
                   show_size.cx, show_size.cy);
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

  LayoutDate();
  int dx = kDownloadIconOffset - kIconOffset + kProgressIconSize +
      kInfoPadding;

  // File name and URL, truncated to show cancelled status
  CSize file_name_size;
  file_name_->SetText(model_->file_name());
  file_name_->GetPreferredSize(&file_name_size);
  file_name_->SetBounds(dx, kIconOffset,
                        kFilenameSize - kProgressSize - kSpacer,
                        file_name_size.cy);
  file_name_->SetVisible(true);
  file_name_->SetEnabled(false);

  GURL url(model_->url());
  download_url_->SetURL(url);
  CSize url_size;
  download_url_->GetPreferredSize(&url_size);
  download_url_->SetBounds(dx,
                           file_name_size.cy + kVerticalPadding + kIconOffset,
                           std::min(kFilenameSize - kProgressSize - kSpacer,
                                    static_cast<int>(GetWidth() - dx)),
                           url_size.cy);
  download_url_->SetVisible(true);

  dx += kFilenameSize - kProgressSize;

  // Display cancelled status
  CSize cancel_size;
  time_remaining_->SetColor(kStatusColor);
  time_remaining_->SetText(l10n_util::GetString(IDS_DOWNLOAD_TAB_CANCELLED));
  time_remaining_->GetPreferredSize(&cancel_size);
  time_remaining_->SetBounds(dx, kIconOffset, kProgressSize, cancel_size.cy);
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
    // locale direction as ChromeViews::Label does that for us.
    amount = l10n_util::GetStringF(IDS_DOWNLOAD_TAB_PROGRESS_SIZE,
                                   received_size,
                                   total_text);
  }

  CSize byte_size;
  download_progress_->SetText(amount);
  download_progress_->GetPreferredSize(&byte_size);
  download_progress_->SetBounds(dx,
                                file_name_size.cy + kVerticalPadding +
                                kIconOffset,
                                kProgressSize,
                                byte_size.cy);
  download_progress_->SetVisible(true);
}

// DownloadItem::IN_PROGRESS state layout
void DownloadItemTabView::LayoutInProgress() {
  // Hide unused UI elements
  show_->SetVisible(false);
  show_->SetEnabled(false);

  LayoutDate();
  int dx = kDownloadIconOffset - kIconOffset + kProgressIconSize +
           kInfoPadding;

  // File name and URL, truncated to show progress status
  CSize file_name_size;
  file_name_->SetText(model_->file_name());
  file_name_->GetPreferredSize(&file_name_size);
  file_name_->SetBounds(dx, kIconOffset,
                        kFilenameSize - kProgressSize - kSpacer,
                        file_name_size.cy);
  file_name_->SetVisible(true);
  file_name_->SetEnabled(false);

  GURL url(model_->url());
  download_url_->SetURL(url);
  CSize url_size;
  download_url_->GetPreferredSize(&url_size);
  download_url_->SetBounds(dx, file_name_size.cy + kVerticalPadding +
                           kIconOffset,
                           std::min(kFilenameSize - kProgressSize - kSpacer,
                                    static_cast<int>(GetWidth() - dx)),
                           url_size.cy);
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
  int y_pos = file_name_size.cy + kVerticalPadding + kIconOffset;
  CSize time_size;
  time_remaining_->SetColor(kStatusColor);
  if (model_->is_paused()) {
    time_remaining_->SetColor(kPauseColor);
    time_remaining_->SetText(
        l10n_util::GetString(IDS_DOWNLOAD_PROGRESS_PAUSED));
    time_remaining_->GetPreferredSize(&time_size);
    time_remaining_->SetBounds(dx, kIconOffset, kProgressSize, time_size.cy);
    time_remaining_->SetVisible(true);
  } else if (total > 0)  {
    TimeDelta remaining;
    if (model_->TimeRemaining(&remaining))
      time_remaining_->SetText(TimeFormat::TimeRemaining(remaining));
    time_remaining_->GetPreferredSize(&time_size);
    time_remaining_->SetBounds(dx, kIconOffset, kProgressSize, time_size.cy);
    time_remaining_->SetVisible(true);
  } else {
    time_remaining_->SetText(L"");
    y_pos = ((file_name_size.cy + url_size.cy) / 2) + kIconOffset;
  }

  CSize byte_size;
  download_progress_->SetText(progress);
  download_progress_->GetPreferredSize(&byte_size);
  download_progress_->SetBounds(dx, y_pos,
                                kProgressSize, byte_size.cy);
  download_progress_->SetVisible(true);

  dx += kProgressSize + kSpacer;
  y_pos = ((file_name_size.cy + url_size.cy) / 2) + kIconOffset;

  // Pause (or Resume) / Cancel buttons.
  if (model_->is_paused())
    pause_->SetText(l10n_util::GetString(IDS_DOWNLOAD_LINK_RESUME));
  else
    pause_->SetText(l10n_util::GetString(IDS_DOWNLOAD_LINK_PAUSE));

  CSize pause_size;
  pause_->SetVisible(true);
  pause_->SetEnabled(true);
  pause_->GetPreferredSize(&pause_size);
  pause_->SetBounds(dx, y_pos, pause_size.cx, pause_size.cy);

  dx += pause_size.cx + kHorizontalButtonPadding;

  CSize cancel_size;
  cancel_->GetPreferredSize(&cancel_size);
  cancel_->SetBounds(dx, y_pos, cancel_size.cx, cancel_size.cy);
  cancel_->SetVisible(true);
  cancel_->SetEnabled(true);
}

void DownloadItemTabView::Paint(ChromeCanvas* canvas) {
  PaintBackground(canvas);

  if (model_->state() == DownloadItem::IN_PROGRESS) {
    download_util::PaintDownloadProgress(canvas,
                                         this,
                                         kDownloadIconOffset - kIconOffset,
                                         0,
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
    gfx::Rect icon_bounds(kDownloadIconOffset, kIconOffset,
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
    gfx::Rect highlighted_bounds(kDownloadIconOffset - kIconOffset, 0,
                                 kProgressIconSize + kInfoPadding +
                                 kFilenameSize,
                                 kProgressIconSize);
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

void DownloadItemTabView::DidChangeBounds(const CRect& previous,
                                          const CRect& current) {
  Layout();
}

bool DownloadItemTabView::OnMousePressed(const ChromeViews::MouseEvent& event) {
  CPoint point(event.GetLocation());

  // If the click is in the highlight region, then highlight this download.
  // Otherwise, remove the highlighting from any download.
  CRect select_rect(kDownloadIconOffset - kIconOffset, 0,
                    kDownloadIconOffset - kIconOffset +
                    kProgressIconSize + kInfoPadding + kFilenameSize,
                    kProgressIconSize);

  // The position of the highlighted region does not take into account the
  // View's UI layout so we have to manually mirror the position if the View is
  // using a right-to-left UI layout.
  gfx::Rect mirrored_rect(select_rect);
  select_rect.MoveToX(MirroredLeftPointForRect(mirrored_rect));
  if (select_rect.PtInRect(point)) {
    parent_->ItemBecameSelected(model_);

    if (event.IsRightMouseButton()) {
      ChromeViews::View::ConvertPointToScreen(this, &point);

      download_util::DownloadDestinationContextMenu menu(
          model_, GetViewContainer()->GetHWND(), point);
    }
  } else {
    parent_->ItemBecameSelected(NULL);
  }

  return true;
}

// Handle drag (file copy) operations.
bool DownloadItemTabView::OnMouseDragged(const ChromeViews::MouseEvent& event) {
  if (model_->state() != DownloadItem::COMPLETE)
    return false;

  CPoint point(event.GetLocation());

  // In order to make sure drag and drop works as expected when the UI is
  // mirrored, we can either flip the mouse X coordinate or flip the X position
  // of the drag rectangle. Flipping the mouse X coordinate is easier.
  point.x = MirroredXCoordinateInsideView(point.x);
  CRect drag_rect(kDownloadIconOffset - kIconOffset, 0,
                  kDownloadIconOffset - kIconOffset +
                  kProgressIconSize + kInfoPadding + kFilenameSize,
                  kProgressIconSize);

  if (drag_rect.PtInRect(point)) {
    SkBitmap* icon = parent_->LookupIcon(model_);
    if (icon)
      download_util::DragDownload(model_, icon);
  }

  return true;
}

void DownloadItemTabView::LinkActivated(ChromeViews::Link* source,
                                        int event_flags) {
  // There are several links in our view that could have been clicked:
  if (source == file_name_) {
    ChromeViews::ViewContainer* container = this->GetViewContainer();
    HWND parent_window = container ? container->GetHWND() : NULL;
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


// DownloadTabView implementation ----------------------------------------------

DownloadTabView::DownloadTabView(DownloadManager* model)
    : model_(model),
      progress_timer_(NULL),
      progress_task_(NULL),
      start_angle_(download_util::kStartAngleDegrees),
      scroll_helper_(kSpacer, kProgressIconSize + kSpacer),
      selected_index_(-1) {
  DCHECK(model_);
}

DownloadTabView::~DownloadTabView() {
  StopDownloadProgress();
  model_->RemoveObserver(this);

  // DownloadManager owns the contents.
  downloads_.clear();
  ClearDownloadInProgress();

  icon_consumer_.CancelAllRequests();
}

void DownloadTabView::Initialize() {
  model_->AddObserver(this);
}

// Start progress animation timers when we get our first (in-progress) download.
void DownloadTabView::StartDownloadProgress() {
  if (progress_task_ || progress_timer_)
    return;
  progress_task_ =
      new download_util::DownloadProgressTask<DownloadTabView>(this);
  progress_timer_ =
      MessageLoop::current()->timer_manager()->
          StartTimer(download_util::kProgressRateMs, progress_task_, true);
}

// Stop progress animation when there are no more in-progress downloads.
void DownloadTabView::StopDownloadProgress() {
  if (progress_timer_) {
    DCHECK(progress_task_);
    MessageLoop::current()->timer_manager()->StopTimer(progress_timer_);
    delete progress_timer_;
    progress_timer_ = NULL;
    delete progress_task_;
    progress_task_ = NULL;
  }
}

// Update our animations.
void DownloadTabView::UpdateDownloadProgress() {
  start_angle_ = (start_angle_ + download_util::kUnknownIncrementDegrees) %
                 download_util::kMaxDegrees;
  SchedulePaint();
}

void DownloadTabView::DidChangeBounds(const CRect& previous,
                                      const CRect& current) {
  Layout();
}

void DownloadTabView::Layout() {
  CRect r;
  DetachAllFloatingViews();
  View* v = GetParent();
  if (v) {
    v->GetLocalBounds(&r, true);
    int x = GetX();
    int y = GetY();
    int w = v->GetWidth();
    int h = static_cast<int>(downloads_.size()) *
            (kProgressIconSize + kSpacer) + kSpacer;
    SetBounds(x, y, w, h);
  }
}

// Paint our scrolled region
void DownloadTabView::Paint(ChromeCanvas* canvas) {
  ChromeViews::View::Paint(canvas);

  if (kIconSize == 0 || downloads_.size() == 0)
    return;

  SkRect clip;
  if (canvas->getClipBounds(&clip)) {
    int row_start = (SkScalarRound(clip.fTop) - kSpacer) /
                    (kProgressIconSize + kSpacer);
    int row_stop = SkScalarRound(clip.fBottom) /
                   (kProgressIconSize + kSpacer);
    SkRect download_rect;
    for (int i = row_start; i <= row_stop; ++i) {
      int y = i * (kProgressIconSize + kSpacer) + kSpacer;
      if (HasFloatingViewForPoint(0, y))
        continue;
      download_rect.set(SkIntToScalar(0),
                        SkIntToScalar(y),
                        SkIntToScalar(GetWidth()),
                        SkIntToScalar(y + kProgressIconSize));
      if (SkRect::Intersects(clip, download_rect)) {
        // The DownloadManager stores downloads earliest first, but this
        // view displays latest first, so adjust the index:
        int index = static_cast<int>(downloads_.size()) - 1 - i;
        download_renderer_.SetModel(downloads_[index], this);
        PaintFloatingView(canvas, &download_renderer_,
                          0, y,
                          GetWidth(), kProgressIconSize);
      }
    }
  }
}

// Draw the DownloadItemTabView for the current position.
bool DownloadTabView::GetFloatingViewIDForPoint(int x, int y, int* id) {
  if (y < kSpacer ||
      y > (kSpacer + kProgressIconSize) *  static_cast<int>(downloads_.size()))
    return false;

  // Are we hovering over a download or the spacer? If we're over the
  // download, create a floating view for it.
  if ((y - kSpacer) % (kProgressIconSize + kSpacer) <  kProgressIconSize) {
    int row = y / (kProgressIconSize + kSpacer);
    *id = static_cast<int>(downloads_.size()) - 1 - row;
    return true;
  }
  return false;
}

ChromeViews::View* DownloadTabView::CreateFloatingViewForIndex(int index) {
  if (index >= static_cast<int>(downloads_.size())) {
    // It's possible that the downloads have been cleared via the "Clear
    // Browsing Data" command, so this index is gone.
    return NULL;
  }

  DownloadItemTabView* dl = new DownloadItemTabView();
  dl->SetModel(downloads_[index], this);
  int row = static_cast<int>(downloads_.size()) - 1 - index;
  int y_pos = row * (kProgressIconSize + kSpacer) + kSpacer;
  dl->SetBounds(0, y_pos, GetWidth(), kProgressIconSize);
  dl->Layout();
  AttachFloatingView(dl, index);
  return dl;
}

bool DownloadTabView::EnumerateFloatingViews(
      ChromeViews::View::FloatingViewPosition position,
      int starting_id, int* id) {
  DCHECK(id);
  return View::EnumerateFloatingViewsForInterval(
      0, static_cast<int>(downloads_.size()), false, position, starting_id, id);
}

ChromeViews::View* DownloadTabView::ValidateFloatingViewForID(int id) {
  return CreateFloatingViewForIndex(id);
}

void DownloadTabView::OnDownloadUpdated(DownloadItem* download) {
  switch (download->state()) {
    case DownloadItem::COMPLETE:
    case DownloadItem::CANCELLED: {
      stdext::hash_set<DownloadItem*>::iterator d = in_progress_.find(download);
      if (d != in_progress_.end()) {
        (*d)->RemoveObserver(this);
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
        stdext::hash_set<DownloadItem*>::iterator it = in_progress_.begin();
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
  SkBitmap* icon = im->LookupIcon(download->full_path(), IconLoader::NORMAL);

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
      im->LoadIcon(download->full_path(), IconLoader::NORMAL,
                   &icon_consumer_,
                   NewCallback(this, &DownloadTabView::OnExtractIconComplete));
  icon_consumer_.SetClientData(im, h, download);
}

void DownloadTabView::ClearDownloadInProgress() {
  for (stdext::hash_set<DownloadItem*>::iterator it = in_progress_.begin();
       it != in_progress_.end(); ++it)
    (*it)->RemoveObserver(this);
  in_progress_.clear();
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
    ChromeViews::ScrollView* scroll_view, bool is_horizontal,
    bool is_positive) {
  return scroll_helper_.GetPageScrollIncrement(scroll_view, is_horizontal,
                                               is_positive);
}

int DownloadTabView::GetLineScrollIncrement(
    ChromeViews::ScrollView* scroll_view, bool is_horizontal,
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
  SchedulePaint(0, y, GetWidth(), kProgressIconSize);
}

int DownloadTabView::GetYPositionForIndex(int index) {
  int row = static_cast<int>(downloads_.size()) - 1 - index;
  return row * (kProgressIconSize + kSpacer) + kSpacer;
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
  ns->AddObserver(this, NOTIFY_DOWNLOAD_START,
                  NotificationService::AllSources());
  ns->AddObserver(this, NOTIFY_DOWNLOAD_STOP,
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
  ns->RemoveObserver(this, NOTIFY_DOWNLOAD_START,
                     NotificationService::AllSources());
  ns->RemoveObserver(this, NOTIFY_DOWNLOAD_STOP,
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

ChromeViews::View* DownloadTabUI::GetView() {
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
  switch (type) {
    case NOTIFY_DOWNLOAD_START:
    case NOTIFY_DOWNLOAD_STOP:
      DCHECK(profile()->HasCreatedDownloadManager());
      contents_->SetIsLoading(
          profile()->GetDownloadManager()->in_progress_count() > 0,
          NULL);
      break;
    default:
      break;
  }
}
