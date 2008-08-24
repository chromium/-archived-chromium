// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Base class used by history view. BaseHistoryModel provides support for
// fetching thumbnails and favicons, but not the actual contents that are
// displayed.

#ifndef CHROME_BROWSER_BASE_HISTORY_MODEL_H__
#define CHROME_BROWSER_BASE_HISTORY_MODEL_H__

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/history/history.h"
#include "chrome/common/mru_cache.h"
#include "SkBitmap.h"

class Profile;

// Defines functions that allow a view associated with this model to
// learn that it should relayout/paint itself.
class BaseHistoryModelObserver {
 public:
  // Called when the data in the model has changed.
  // |result_set_changed| is true when the model item counts have changed,
  // false when the change is just item metadata (like thumbnails or
   // starredness).
  virtual void ModelChanged(bool result_set_changed) = 0;

  // Called when a long operation has begun, to allow the observer to show
  // that work is pending.
  virtual void ModelBeginWork() = 0;

  // Called when a long operation has completed.
  virtual void ModelEndWork() = 0;
};

class BaseHistoryModel {
 public:
  explicit BaseHistoryModel(Profile* profile);
  virtual ~BaseHistoryModel();

  // The observer, notified of changes to the model or when processing
  // begins/ends.
  void SetObserver(BaseHistoryModelObserver* observer);
  BaseHistoryModelObserver* GetObserver() const;

  // Returns the number of history items currently in the model.
  virtual int GetItemCount() = 0;

  // Returns the time of the visit with the given index.
  virtual Time GetVisitTime(int index) = 0;

  // Returns the title at the specified index.
  virtual const std::wstring& GetTitle(int index) = 0;

  // Returns the URL at the specified index.
  virtual const GURL& GetURL(int index) = 0;

  // Returns the id of the URL at the specified index.
  virtual history::URLID GetURLID(int index) = 0;

  // Returns true if the page at the specified index is starred.
  virtual bool IsStarred(int index) = 0;

  // Returns true if the entries are search results (this affects the UI shown).
  virtual bool IsSearchResults() { return is_search_results_; }

  // Returns the snippet at the specified index.
  virtual const Snippet& GetSnippet(int index) = 0;

  // Returns the favicon or thumbnail for the specified page. If the image is
  // not available, NULL is returned. If the image has NOT been loaded, it is
  // loaded and the observer is called back when done loading.
  SkBitmap* GetThumbnail(int index);
  SkBitmap* GetFavicon(int index);

  // Sets the new value of the search text, and requeries if the new value
  // is different from the previous value.
  virtual void SetSearchText(const std::wstring& search_text) { }

  // Returns the search text.
  virtual const std::wstring& GetSearchText() const = 0;

  // Change the starred state of a given index.
  virtual void SetPageStarred(int index, bool state) = 0;

  // The following methods are only invoked by HistoryView if the HistoryView
  // is configured to show bookmark controls.

  // Returns true if the item is shown on the bookmark bar.
  virtual bool IsOnBookmarkBar(int index) { return false; }

  // Returns the path of the item on the bookmark bar. This may return the
  // empty string.
  virtual std::wstring GetBookmarkBarPath(int index) { return std::wstring(); }

  // Edits the specified entry. This is intended for bookmarks where the user
  // can edit various properties of the bookmark.
  virtual void Edit(int index) {}

  // Removes the specified entry.
  virtual void Remove(int index) {}

  // Removes the specified range from the model. This should NOT update the
  // backend, rather it should just update the model and notify listeners
  // appropriately. This need only be implemented if you turn on delete controls
  // in the hosting HistoryView.
  virtual void RemoveFromModel(int start, int length) { NOTREACHED(); }

  // Reloads the model.
  virtual void Refresh() = 0;

  Profile* profile() const { return profile_; }

  // Number of months history requests should go back for.
  static const int kHistoryScopeMonths;

 protected:
  // Invoke when you're about to schedule a request on the history service. If
  // no requests have been made, the observer is notified of ModelBeginWork.
  void AboutToScheduleRequest();

  // Invoke from a callback from the history service. If no requests are pending
  // on the history service the observer is notified of ModelEndWork.
  void RequestCompleted();

#ifndef NDEBUG
  // For debugging, meant to be wrapped in a DCHECK.
  bool IsValidIndex(int index) {
    return (index >= 0 && index < GetItemCount());
  }
#endif

  // The user profile associated with the page that this model feeds.
  Profile* profile_;

  // For history requests. This is used when requesting favicons and
  // thumbnails, subclasses may also use this.
  CancelableRequestConsumerT<history::URLID, 0> cancelable_consumer_;

  // Notified of changes to the content, typically HistoryView.
  BaseHistoryModelObserver* observer_;

  // Whether the last returned result was a set of search results.
  bool is_search_results_;

 private:
  // Enum of the types of image we cache. Used by GetImage.
  enum ImageType {
    THUMBNAIL,
    FAVICON
  };

  typedef MRUCache<history::URLID, SkBitmap> CacheType;

  // Returns the image at the specified index. If the image has not been loaded
  // it is loaded.
  SkBitmap* GetImage(ImageType type, int index);

  // Called when a request for the thumbnail data is complete. Updates the
  // thumnails_ cache and notifies the observer (assuming the image is valid).
  void OnThumbnailDataAvailable(
      HistoryService::Handle request_handle,
      scoped_refptr<RefCountedBytes> data);

  // Callback when a favicon is available. Updates the favicons_ cache and
  // notifies the observer (assuming we can extract a valid image).
  void OnFaviconDataAvailable(
      HistoryService::Handle handle,
      bool know_favicon,
      scoped_refptr<RefCountedBytes> data,
      bool expired,
      GURL icon_url);

  // Keeps track of thumbnails for pages
  CacheType thumbnails_;

  // Keeps track of favicons for pages
  CacheType favicons_;

  DISALLOW_EVIL_CONSTRUCTORS(BaseHistoryModel);
};

#endif  // CHROME_BROWSER_BASE_HISTORY_MODEL_H__

