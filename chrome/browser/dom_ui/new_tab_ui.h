// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H__
#define CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H__

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/dom_ui/dom_ui_host.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/template_url_model.h"

class GURL;
class Profile;
class Value;
enum TabContentsType;

// Return the URL for the new tab page.
GURL NewTabUIURL();

// If a |url| is a chrome-internal: URL, this method sets up |url|, and 
// |result_type| to the appropriate values for displaying the new tab page 
// and returns true. Exposed for use by BrowserURLHandler.
bool NewTabUIHandleURL(GURL* url, TabContentsType* result_type);

// The following classes aren't used outside of new_tab_ui.cc but are
// put here for clarity.

class NewTabHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  NewTabHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

  // Setters and getters for first_view.
  static void set_first_view(bool first_view) { first_view_ = first_view; }
  static bool first_view() { return first_view_; }
 private:
  // Whether this is the is the first viewing of the new tab page and
  // we think it is the user's startup page.
  static bool first_view_;

  DISALLOW_EVIL_CONSTRUCTORS(NewTabHTMLSource);
};

class IncognitoTabHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  // Creates our datasource and sets our user message to a specific message
  // from our string bundle.
  IncognitoTabHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(IncognitoTabHTMLSource);
};

// The handler for Javascript messages related to the "most visited" view.
class MostVisitedHandler : public DOMMessageHandler,
                           public NotificationObserver {
 public:
  explicit MostVisitedHandler(DOMUIHost* dom_ui_host);
  virtual ~MostVisitedHandler();

  // Callback for the "getMostVisited" message.
  void HandleGetMostVisited(const Value* value);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  const std::vector<GURL>& most_visited_urls() const {
    return most_visited_urls_;
  }

 private:
  // Callback from the history system when the most visited list is available.
  void OnSegmentUsageAvailable(CancelableRequestProvider::Handle handle,
                               std::vector<PageUsageData*>* data);

  DOMUIHost* dom_ui_host_;

  // Our consumer for the history service.
  CancelableRequestConsumerT<PageUsageData*, NULL> cancelable_consumer_;

  // The most visited URLs, in priority order.
  // Only used for matching up clicks on the page to which most visited entry
  // was clicked on for metrics purposes.
  std::vector<GURL> most_visited_urls_;

  DISALLOW_EVIL_CONSTRUCTORS(MostVisitedHandler);
};

// The handler for Javascript messages related to the "common searches" view.
class TemplateURLHandler : public DOMMessageHandler,
                           public TemplateURLModelObserver {
 public:
  explicit TemplateURLHandler(DOMUIHost* dom_ui_host);
  virtual ~TemplateURLHandler();

  // Callback for the "getMostSearched" message, sent when the page requests
  // the list of available searches.
  void HandleGetMostSearched(const Value* content);
  // Callback for the "doSearch" message, sent when the user wants to
  // run a search.  Content of the message is an array containing
  // [<the search keyword>, <the search term>].
  void HandleDoSearch(const Value* content);

  // TemplateURLModelObserver implementation.
  virtual void OnTemplateURLModelChanged();

 private:
  DOMUIHost* dom_ui_host_;
  TemplateURLModel* template_url_model_;  // Owned by profile.

  DISALLOW_EVIL_CONSTRUCTORS(TemplateURLHandler);
};

class RecentlyBookmarkedHandler : public DOMMessageHandler,
                                  public BookmarkModelObserver {
 public:
  explicit RecentlyBookmarkedHandler(DOMUIHost* dom_ui_host);
  ~RecentlyBookmarkedHandler();

  // Callback which navigates to the bookmarks page.
  void HandleShowBookmarkPage(const Value*);

  // Callback for the "getRecentlyBookmarked" message.
  // It takes no arguments.
  void HandleGetRecentlyBookmarked(const Value*);

 private:
  void SendBookmarksToPage();

  // BookmarkModelObserver methods. These invoke SendBookmarksToPage.
  virtual void Loaded(BookmarkModel* model);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node);

  // These two won't effect what is shown, so they do nothing.
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node) {}

  DOMUIHost* dom_ui_host_;
  // The model we're getting bookmarks from. The model is owned by the Profile.
  BookmarkModel* model_;

  DISALLOW_EVIL_CONSTRUCTORS(RecentlyBookmarkedHandler);
};

class RecentlyClosedTabsHandler : public DOMMessageHandler,
                                  public TabRestoreService::Observer {
 public:
  explicit RecentlyClosedTabsHandler(DOMUIHost* dom_ui_host);
  virtual ~RecentlyClosedTabsHandler();

  // Callback for the "reopenTab" message. Rewrites the history of the
  // currently displayed tab to be the one in TabRestoreService with a
  // history of a session passed in through the content pointer.
  void HandleReopenTab(const Value* content);

  // Callback for the "getRecentlyClosedTabs" message.
  void HandleGetRecentlyClosedTabs(const Value* content);

  // Observer callback for TabRestoreService::Observer. Sends data on
  // recently closed tabs to the javascript side of this page to
  // display to the user.
  virtual void TabRestoreServiceChanged(TabRestoreService* service);

  // Observer callback to notice when our associated TabRestoreService
  // is destroyed.
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service);

 private:
  // Converts a closed tab to the value sent down to the NTP. Returns true on
  // success, false if the value shouldn't be sent down.
  bool TabToValue(const TabRestoreService::Tab& tab,
                  DictionaryValue* dictionary);

  // Converts a closed window to the value sent down to the NTP. Returns true
  // on success, false if the value shouldn't be sent down.
  bool WindowToValue(const TabRestoreService::Window& window,
                     DictionaryValue* dictionary);

  DOMUIHost* dom_ui_host_;

  /// TabRestoreService that we are observing.
  TabRestoreService* tab_restore_service_;

  DISALLOW_EVIL_CONSTRUCTORS(RecentlyClosedTabsHandler);
};

class HistoryHandler : public DOMMessageHandler {
 public:
  explicit HistoryHandler(DOMUIHost* dom_ui_host);

  // Callback which navigates to the history page.
  void HandleShowHistoryPage(const Value*);

  // Callback which navigates to the history page and performs a search.
  void HandleSearchHistoryPage(const Value* content);

 private:
  DOMUIHost* dom_ui_host_;
  DISALLOW_EVIL_CONSTRUCTORS(HistoryHandler);
};

// Let the page contents record UMA actions. Only use when you can't do it from
// C++. For example, we currently use it to let the NTP log the postion of the
// Most Visited or Bookmark the user clicked on, as we don't get that 
// information through RequestOpenURL. You will need to update the metrics 
// dashboard with the action names you use, as our processor won't catch that
// information (treat it as RecordComputedMetrics)
class MetricsHandler : public DOMMessageHandler {
 public:
  explicit MetricsHandler(DOMUIHost* dom_ui_host);

  // Callback which records a user action.
  void HandleMetrics(const Value* content);

 private:
  DOMUIHost* dom_ui_host_;
  DISALLOW_EVIL_CONSTRUCTORS(MetricsHandler);
};

// The TabContents used for the New Tab page.
class NewTabUIContents : public DOMUIHost {
 public:
  NewTabUIContents(Profile* profile,
                   SiteInstance* instance,
                   RenderViewHostFactory* render_view_factory);

  // Set the title that overrides any other title provided for the tab.
  // This lets you set the title that's displayed before the content loads,
  // as well as override any "Loading..." text.
  void set_forced_title(const std::wstring& title) {
    forced_title_ = title;
  }

  // DOMUIHost implementation.
  virtual void AttachMessageHandlers();

  // WebContents overrides.
  // Overriden to force the title of the page to forced_title_.
  virtual bool NavigateToPendingEntry(bool reload);
  // We don't want a favicon on the new tab page.
  virtual bool ShouldDisplayFavIcon() { return false; }
  // The bookmark bar is always visible on the new tab.
  virtual bool IsBookmarkBarAlwaysVisible() { return true; }
  // Return forced_title_ if it's available.
  virtual const std::wstring& GetTitle() const;
  // When we get the initial focus, focus the URL bar.
  virtual void SetInitialFocus();
  // The URL bar should not display the current page's URL.
  virtual bool ShouldDisplayURL() { return false; }
  virtual bool SupportsURL(GURL* url);
  // Clicking a URL on the page should count as an autobookmark click.
  virtual void RequestOpenURL(const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition);
 private:
  // The message id that should be displayed in this NewTabUIContents
  // instance's motd area.
  int motd_message_id_;

  // Whether the user is in incognito mode or not, used to determine
  // what HTML to load.
  bool incognito_;

  // A title for the page we force display of.
  // This prevents intermediate titles (like "Loading...") from displaying.
  std::wstring forced_title_;

  // A pointer to the handler for most visited.
  // Owned by the DOMUIHost.
  MostVisitedHandler* most_visited_handler_;

  DISALLOW_EVIL_CONSTRUCTORS(NewTabUIContents);
};

#endif  // CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H__

