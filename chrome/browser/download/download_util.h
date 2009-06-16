// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utilities.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/task.h"

#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
#include "views/view.h"
#endif

namespace gfx {
class Canvas;
}

class BaseDownloadItemModel;
class DownloadItem;
class SkBitmap;

namespace download_util {

// DownloadProgressTask --------------------------------------------------------

// A class for managing the timed progress animations for a download view. The
// view must implement an UpdateDownloadProgress() method.
template<class DownloadView>
class DownloadProgressTask : public Task {
 public:
  DownloadProgressTask(DownloadView* view) : view_(view) {}
  virtual ~DownloadProgressTask() {}
  virtual void Run() {
    view_->UpdateDownloadProgress();
  }
 private:
  DownloadView* view_;
  DISALLOW_COPY_AND_ASSIGN(DownloadProgressTask);
};

// Download opening ------------------------------------------------------------

// Whether it is OK to open this download.
bool CanOpenDownload(DownloadItem* download);

// Open the file associated with this download (wait for the download to
// complete if it is in progress).
void OpenDownload(DownloadItem* download);

// Download progress animations ------------------------------------------------

// Arc sweep angle for use with downloads of unknown size
const int kUnknownAngleDegrees = 50;

// Rate of progress for use with downloads of unknown size
const int kUnknownIncrementDegrees = 12;

// Start angle for downloads with known size (midnight position)
const int kStartAngleDegrees = -90;

// A circle
const int kMaxDegrees = 360;

// Progress animation timer period, in milliseconds.
const int kProgressRateMs = 150;

// XP and Vista must support icons of this size.
const int kSmallIconSize = 16;
const int kBigIconSize = 32;

// Our progress halo around the icon.
int GetBigProgressIconSize();

const int kSmallProgressIconSize = 39;
const int kBigProgressIconSize = 52;

// The offset required to center the icon in the progress bitmaps.
int GetBigProgressIconOffset();

const int kSmallProgressIconOffset =
    (kSmallProgressIconSize - kSmallIconSize) / 2;

enum PaintDownloadProgressSize {
  SMALL = 0,
  BIG
};

// Paint the common download animation progress foreground and background,
// clipping the foreground to 'percent' full. If percent is -1, then we don't
// know the total size, so we just draw a rotating segment until we're done.
//
// |containing_view| is the View subclass within which the progress animation
// is drawn (generally either DownloadItemTabView or DownloadItemView). We
// require the containing View in addition to the canvas because if we are
// drawing in a right-to-left locale, we need to mirror the position of the
// progress animation within the containing View.
void PaintDownloadProgress(gfx::Canvas* canvas,
#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
                           views::View* containing_view,
#endif
                           int origin_x,
                           int origin_y,
                           int start_angle,
                           int percent,
                           PaintDownloadProgressSize size);

void PaintDownloadComplete(gfx::Canvas* canvas,
#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
                           views::View* containing_view,
#endif
                           int origin_x,
                           int origin_y,
                           double animation_progress,
                           PaintDownloadProgressSize size);

#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
// Drag support ----------------------------------------------------------------

// Helper function for download views to use when acting as a drag source for a
// DownloadItem. If 'icon' is NULL, no image will be accompany the drag.
void DragDownload(const DownloadItem* download, SkBitmap* icon);
#endif

// Executable file support -----------------------------------------------------

// Copy all executable file extensions.
void InitializeExeTypes(std::set<std::string>* exe_extensions);

}  // namespace download_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
