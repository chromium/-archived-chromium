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

// Describes a class that can represent the information displayed in a history
// view; that is, a list of visited pages.  This object knows how to
// talk to the HistoryService to update its state.

#ifndef CHROME_BROWSER_HISTORY_MODEL_H__
#define CHROME_BROWSER_HISTORY_MODEL_H__

#include "chrome/browser/base_history_model.h"
#include "chrome/common/notification_service.h"

typedef BaseHistoryModelObserver HistoryModelObserver;

class HistoryModel : public BaseHistoryModel,
                     public NotificationObserver {
 public:
  HistoryModel(Profile* profile, const std::wstring& search_text);
  virtual ~HistoryModel();

  // BaseHistoryModel methods (see BaseHistoryModel for description).
  virtual int GetItemCount();
  virtual Time GetVisitTime(int index);
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

  // How many months back the current query has gone.
  int search_depth_;

  // The time that the current query was started.
  Time search_start_;

  DISALLOW_EVIL_CONSTRUCTORS(HistoryModel);
};

#endif  // CHROME_BROWSER_HISTORY_MODEL_H__
