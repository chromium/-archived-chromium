// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_HISTORY_UI_H__
#define CHROME_BROWSER_DOM_UI_HISTORY_UI_H__

#include "base/scoped_ptr.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/dom_ui/dom_ui_contents.h"
#include "chrome/browser/cancelable_request.h"

class GURL;

class HistoryUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  HistoryUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryUIHTMLSource);
};

// The handler for Javascript messages related to the "history" view.
class BrowsingHistoryHandler : public DOMMessageHandler,
                               public NotificationObserver,
                               public BrowsingDataRemover::Observer {
 public:
  explicit BrowsingHistoryHandler(DOMUI* dom_ui_);
  virtual ~BrowsingHistoryHandler();

  // Callback for the "getHistory" message.
  void HandleGetHistory(const Value* value);

  // Callback for the "searchHistory" message.
  void HandleSearchHistory(const Value* value);

  // Callback for the "deleteDay" message.
  void HandleDeleteDay(const Value* value);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // BrowsingDataRemover observer implementation.
  void OnBrowsingDataRemoverDone();

 private:
  // Callback from the history system when the history list is available.
  void QueryComplete(HistoryService::Handle request_handle,
                     history::QueryResults* results);

  // Extract the arguments from the call to HandleSearchHistory.
  void ExtractSearchHistoryArguments(const Value* value,
                                     int* month,
                                     std::wstring* query);

  // Figure out the query options for a month-wide query.
  history::QueryOptions CreateMonthQueryOptions(int month);

  // Current search text.
  std::wstring search_text_;

  // Browsing history remover
  scoped_ptr<BrowsingDataRemover> remover_;

  // Our consumer for the history service.
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryHandler);
};

class HistoryUI : public DOMUI {
 public:
  explicit HistoryUI(DOMUIContents* contents);

  // Return the URL for the front page of this UI.
  static GURL GetBaseURL();

  // Return the URL for a given search term.
  static const GURL GetHistoryURLWithSearchText(const std::wstring& text);

  // DOMUI Implementation
  virtual void Init();

 private:

  DISALLOW_COPY_AND_ASSIGN(HistoryUI);
};

#endif  // CHROME_BROWSER_DOM_UI_HISTORY_UI_H__
