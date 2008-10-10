// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_DONWLOAD_TAB_VIEW_H__
#define CHROME_BROWSER_VIEWS_DONWLOAD_TAB_VIEW_H__

#include "base/hash_tables.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/native_ui_contents.h"
#include "chrome/views/event.h"
#include "chrome/views/label.h"
#include "chrome/views/link.h"
#include "chrome/views/scroll_view.h"

class DownloadTabView;
class SkBitmap;
class Task;

namespace base {
class Timer;
}

class DownloadItemTabView : public ChromeViews::View,
                            public ChromeViews::LinkController,
                            public ChromeViews::NativeButton::Listener {
 public:
  DownloadItemTabView();
  virtual ~DownloadItemTabView();

  // View overrides
  virtual void Layout();
  virtual void Paint(ChromeCanvas* canvas);
  void PaintBackground(ChromeCanvas* canvas);
  virtual void GetPreferredSize(CSize* out);
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual bool OnMousePressed(const ChromeViews::MouseEvent& event);
  virtual bool OnMouseDragged(const ChromeViews::MouseEvent& event);

  // Mode specific layouts
  void LayoutDate();
  void LayoutComplete();
  void LayoutCancelled();
  void LayoutInProgress();
  void LayoutPromptDangerousDownload();

  // LinkController overrides
  virtual void LinkActivated(ChromeViews::Link* source, int event_flags);

  // NativeButton Listener overrides.
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // Used to set our model temporarily during layout and paint operations
  void SetModel(DownloadItem* model, DownloadTabView* parent);

private:
  // Our model.
  DownloadItem* model_;

  // Containing view.
  DownloadTabView* parent_;

  // Whether we are the renderer for floating views.
  bool is_floating_view_renderer_;

  // Time display.
  ChromeViews::Label* since_;
  ChromeViews::Label* date_;

  // The name of the file. Clicking this link will open the download.
  ChromeViews::Link* file_name_;

  // The name of the downloaded URL.
  ChromeViews::Label* download_url_;

  // The current status of the download.
  ChromeViews::Label* time_remaining_;
  ChromeViews::Label* download_progress_;

  // The message warning of a dangerous download.
  ChromeViews::Label* dangerous_download_warning_;

  // Actions that can be initiated.
  ChromeViews::Link* pause_;
  ChromeViews::Link* cancel_;
  ChromeViews::Link* show_;

  // The buttons used to prompt the user when a dangerous download has been
  // initiated.
  ChromeViews::NativeButton* save_button_;
  ChromeViews::NativeButton* discard_button_;

  DISALLOW_EVIL_CONSTRUCTORS(DownloadItemTabView);
};


// A view that manages each of the individual download views
// (DownloadItemTabView) in the destination tab.
class DownloadTabView : public ChromeViews::View,
                        public DownloadItem::Observer,
                        public DownloadManager::Observer {
 public:
  DownloadTabView(DownloadManager* model);
  ~DownloadTabView();

  void Initialize();

  DownloadManager* model() const { return model_; }

  // View overrides
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void Layout();
  virtual void Paint(ChromeCanvas* canvas);
  virtual bool GetFloatingViewIDForPoint(int x, int y, int* id);
  virtual bool EnumerateFloatingViews(
      ChromeViews::View::FloatingViewPosition position,
      int starting_id, int* id);
  virtual ChromeViews::View* ValidateFloatingViewForID(int id);

  // DownloadItem::Observer interface
  virtual void OnDownloadUpdated(DownloadItem* download);

  // DownloadManager::Observer interface
  virtual void ModelChanged();
  virtual void SetDownloads(std::vector<DownloadItem*>& downloads);

  // IconManager callback interface
  void OnExtractIconComplete(IconManager::Handle handle, SkBitmap* icon_bitmap);

  // Progress animation task interface and timer management.
  void UpdateDownloadProgress();
  void StartDownloadProgress();
  void StopDownloadProgress();

  // Returns a non owning pointer to an icon in the icon_cache_. If
  // that file extension doesn't exist in our cache, we query Windows
  // and add it to our cache. Returns NULL if we can't determine the
  // icon.
  SkBitmap* LookupIcon(DownloadItem* download);

  // Determine if we should draw the date beside a particular download
  bool ShouldDrawDateForDownload(DownloadItem* download);

  virtual int GetPageScrollIncrement(ChromeViews::ScrollView* scroll_view,
                                     bool is_horizontal, bool is_positive);
  virtual int GetLineScrollIncrement(ChromeViews::ScrollView* scroll_view,
                                     bool is_horizontal, bool is_positive);

  int start_angle() const { return start_angle_; }

  // Called by a DownloadItemTabView when it becomes selected. Passing a NULL
  // for 'download' causes any selected download to become unselected.
  void ItemBecameSelected(const DownloadItem* download);
  bool ItemIsSelected(DownloadItem* download);

  // The destination view's search box text has changed.
  void SetSearchText(const std::wstring& search_text);

 private:
  // Creates and attaches to the view the floating view at |index|.
  ChromeViews::View* CreateFloatingViewForIndex(int index);

  // Utility functions for operating on DownloadItemTabViews by index.
  void SchedulePaintForViewAtIndex(int index);
  int GetYPositionForIndex(int index);

  // Initiates an asynchronous icon extraction.
  void LoadIcon(DownloadItem* download);

  // Clears the list of "in progress" downloads and removes this DownloadTabView
  // from their observer list.
  void ClearDownloadInProgress();

  // Clears the list of dangerous downloads and removes this DownloadTabView
  // from their observer list.
  void ClearDangerousDownloads();

  // Our model
  DownloadManager* model_;

  // For drawing individual download items
  DownloadItemTabView download_renderer_;

  // The current set of visible DownloadItems for this view received from the
  // DownloadManager. DownloadManager owns the DownloadItems. The vector is
  // kept in order, sorted by ascending start time.
  typedef std::vector<DownloadItem*> OrderedDownloads;
  OrderedDownloads downloads_;

  // Progress animations
  base::RepeatingTimer<DownloadTabView> progress_timer_;

  // Since this view manages the progress animation timers for all the floating
  // views, we need to track the current in progress downloads. This container
  // does not own the DownloadItems.
  base::hash_set<DownloadItem*> in_progress_;

  // Keeps track of the downloads we are an observer for as a consequence of
  // being a dangerous download.
  base::hash_set<DownloadItem*> dangerous_downloads_;

  // Provide a start position for downloads with no known size.
  int start_angle_;

  ChromeViews::FixedRowHeightScrollHelper scroll_helper_;

  // Keep track of the currently selected view, so that we can inform it when
  // the user changes the selection.
  int selected_index_;

  // Text in the download search box input by the user.
  std::wstring search_text_;

  // For requesting icons from the IconManager.
  CancelableRequestConsumerT<DownloadItem*, 0> icon_consumer_;

  DISALLOW_EVIL_CONSTRUCTORS(DownloadTabView);
};

// DownloadTabUI -------------------------------------------------------------

// DownloadTabUI provides the glue to make DownloadTabView available in
// NativeUIContents.

class DownloadTabUI : public NativeUI,
                      public SearchableUIContainer::Delegate,
                      public NotificationObserver {
 public:
  explicit DownloadTabUI(NativeUIContents* contents);
  virtual ~DownloadTabUI();

  virtual const std::wstring GetTitle() const;
  virtual const int GetFavIconID() const;
  virtual const int GetSectionIconID() const;
  virtual const std::wstring GetSearchButtonText() const;
  virtual ChromeViews::View* GetView();
  virtual void WillBecomeVisible(NativeUIContents* parent);
  virtual void WillBecomeInvisible(NativeUIContents* parent);
  virtual void Navigate(const PageState& state);
  virtual bool SetInitialFocus();

  // Return the URL that can be used to show this view in a NativeUIContents.
  static GURL GetURL();

  // Return the NativeUIFactory object for application views. This object is
  // owned by the caller.
  static NativeUIFactory* GetNativeUIFactory();

 private:
  // Sent from the search box, updates the search text appropriately.
  virtual void DoSearch(const std::wstring& new_text);

  // NotificationObserver method, updates loading state based on whether
  // any downloads are in progress.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  Profile* profile() const { return contents_->profile(); }

  // Our host.
  NativeUIContents* contents_;

  // The view we return from GetView. The contents of this is the
  // bookmarks_view_
  SearchableUIContainer searchable_container_;

  DownloadTabView* download_tab_view_;

  DISALLOW_EVIL_CONSTRUCTORS(DownloadTabUI);
};

#endif  // CHROME_BROWSER_VIEWS_DONWLOAD_TAB_VIEW_H__
