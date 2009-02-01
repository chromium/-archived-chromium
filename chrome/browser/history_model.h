// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Describes a class that can represent the information displayed in a history
// view; that is, a list of visited pages.  This object knows how to
// talk to the HistoryService to update its state.

#ifndef CHROME_BROWSER_HISTORY_MODEL_H_
#define CHROME_BROWSER_HISTORY_MODEL_H_

#include "chrome/browser/base_history_model.h"
#include "chrome/common/notification_observer.h"

typedef BaseHistoryModelObserver HistoryModelObserver;

class HistoryModel : public BaseHistoryModel,
                     public NotificationObserver {
 public:
  HistoryModel(Profile* profile, const std::wstring& search_text);
  virtual ~HistoryModel();

  // BaseHistoryModel methods (see BaseHistoryModel for description).
  virtual int GetItemCount();
  virtual base::Time GetVisitTime(int index);
  virtual const std::wstring& GetTitle(int index);
  virtual const GURL& GetURL(int index);
  virtual history::URLID GetURLID(int index);
  virtual bool IsStarred(int index);
  virtual const Snippet& GetSnippet(int index);
  virtual void RemoveFromModel(int start, int length);

  // Sets the new value of the search text, and requeries if the new value
  // is different from the previous value.
  virtual void SetSearchText(const std::wstring& search_text);

  // Returns the search text.
  virtual const std::wstring& GetSearchText() const { return search_text_; }

  // Change the starred state of a given index.
  virtual void SetPageStarred(int index, bool state);

  // To be called when the user wants to manually refresh this view.
  virtual void Refresh();

  // NotificationObserver implementation. If the type is NOTIFY_URLS_STARRED,
  // the model is updated appropriately.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Initiates a request for the current visit list.
  void InitVisitRequest(int depth);

  // Callback for visited page requests of the history system.
  void VisitedPagesQueryComplete(HistoryService::Handle request_handle,
                                 history::QueryResults* results);

  // Sets the starred state of each instance of the given URL in the result set
  // to the given value. Returns true if anything was updated.
  bool UpdateStarredStateOfURL(const GURL& url, bool is_starred);

  // The current search string.
  std::wstring search_text_;

  // Contents of the current query.
  history::QueryResults results_;

  // We lazily ask the BookmarkModel for whether a URL is starred. This enum
  // gives the state of a particular entry.
  enum StarState {
    UNKNOWN = 0,  // Indicates we haven't determined the state yet.
    STARRED,
    NOT_STARRED
  };

  // star_state_ has an entry for each element of results_ indicating whether
  // the URL is starred.
  scoped_array<StarState> star_state_;

  // How many months back the current query has gone.
  int search_depth_;

  // The time that the current query was started.
  base::Time search_start_;

  DISALLOW_COPY_AND_ASSIGN(HistoryModel);
};

#endif  // CHROME_BROWSER_HISTORY_MODEL_H_
