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
//
// Download utilities.

#ifndef CHROME_BROWSER_DOWNLOAD_UTIL_H__
#define CHROME_BROWSER_DOWNLOAD_UTIL_H__

#include <objidl.h>

#include "base/basictypes.h"
#include "base/task.h"
#include "chrome/browser/views/download_item_view.h"
#include "chrome/views/event.h"
#include "chrome/views/menu.h"
#include "chrome/views/view.h"

class DownloadItem;
class SkBitmap;

namespace download_util {

// DownloadContextMenu ---------------------------------------------------------

// The base class of context menus that provides the various commands.
// Subclasses are responsible for creating and running the menu.
class BaseContextMenu : public Menu::Delegate {
 public:
  explicit BaseContextMenu(DownloadItem* download);
  virtual ~BaseContextMenu();

  enum ContextMenuCommands {
    SHOW_IN_FOLDER = 1,  // Open an Explorer window with the item highlighted
    COPY_LINK,           // Copy the download's URL to the clipboard
    COPY_PATH,           // Copy the download's full path to the clipboard
    COPY_FILE,           // Copy the downloaded file to the clipboard
    OPEN_WHEN_COMPLETE,  // Open the download when it's finished
    ALWAYS_OPEN_TYPE,    // Default this file extension to always open
    REMOVE_ITEM,         // Remove the download
    CANCEL,              // Cancel the download
    MENU_LAST
  };

  // Menu::Delegate interface
  virtual bool IsItemChecked(int id) const;
  virtual bool IsItemDefault(int id) const;
  virtual std::wstring GetLabel(int id) const;
  virtual bool SupportsCommand(int id) const;
  virtual bool IsCommandEnabled(int id) const;
  virtual void ExecuteCommand(int id);

 protected:
  // Information source.
  DownloadItem* download_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(BaseContextMenu);
};

// Menu for the download shelf.
class DownloadShelfContextMenu : public BaseContextMenu {
 public:
  DownloadShelfContextMenu(DownloadItem* download,
                           HWND window,
                           DownloadItemView::BaseDownloadItemModel* model,
                           const CPoint& point);
  virtual ~DownloadShelfContextMenu();

  virtual bool IsItemDefault(int id) const;
  virtual void ExecuteCommand(int id);

 private:
  // A model to control the cancel behavior.
  DownloadItemView::BaseDownloadItemModel* model_;

  DISALLOW_EVIL_CONSTRUCTORS(DownloadShelfContextMenu);
};

// Menu for the download destination view.
class DownloadDestinationContextMenu : public BaseContextMenu {
 public:
  DownloadDestinationContextMenu(DownloadItem* download,
                                 HWND window,
                                 const CPoint& point);
  virtual ~DownloadDestinationContextMenu();

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DownloadDestinationContextMenu);
};

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
  DISALLOW_EVIL_CONSTRUCTORS(DownloadProgressTask);
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

enum PaintDownloadProgressSize {
  SMALL = 0,
  BIG
};

// Returns the expected size of the icon displayed in the progress halo.
int GetIconSize(PaintDownloadProgressSize size);

// Returns the size of our progress halo around the icon.
int GetProgressIconSize(PaintDownloadProgressSize size);

// Returns the offset required to center the icon in the progress bitmaps.
int GetProgressIconOffset(PaintDownloadProgressSize size);

// Paint the common download animation progress foreground and background,
// clipping the foreground to 'percent' full. If percent is -1, then we don't
// know the total size, so we just draw a rotating segment until we're done.
//
// |containing_view| is the View subclass within which the progress animation
// is drawn (generally either DownloadItemTabView or DownloadItemView). We
// require the containing View in addition to the canvas because if we are
// drawing in a right-to-left locale, we need to mirror the position of the
// progress animation within the containing View.
void PaintDownloadProgress(ChromeCanvas* canvas,
                           ChromeViews::View* containing_view,
                           int origin_x,
                           int origin_y,
                           int start_angle,
                           int percent,
                           PaintDownloadProgressSize size);

void PaintDownloadComplete(ChromeCanvas* canvas,
                           ChromeViews::View* containing_view,
                           int origin_x,
                           int origin_y,
                           double animation_progress,
                           PaintDownloadProgressSize size);

// Drag support ----------------------------------------------------------------

// Helper function for download views to use when acting as a drag source for a
// DownloadItem. If 'icon' is NULL, no image will be accompany the drag.
void DragDownload(const DownloadItem* download, SkBitmap* icon);

// Executable file support -----------------------------------------------------

// Copy all executable file extensions.
void InitializeExeTypes(std::set<std::wstring>* exe_extensions);

}  // namespace download_util


#endif  // CHROME_BROWSER_DOWNLOAD_UTIL_H__
