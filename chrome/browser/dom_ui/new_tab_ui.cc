// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/new_tab_ui.h"

#include "base/histogram.h"
#include "base/string_piece.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_resources.h"
#include "chrome/browser/history_tab_ui.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/template_url.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/views/keyword_editor_view.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"

#include "chromium_strings.h"
#include "generated_resources.h"

// The URL scheme used for the new tab.
static const char kNewTabUIScheme[] = "chrome-internal";

// The path used in internal URLs to thumbnail data.
static const char kThumbnailPath[] = "thumb";

// The path used in internal URLs to favicon data.
static const char kFavIconPath[] = "favicon";

// The number of most visited pages we show.
const int kMostVisitedPages = 9;

// The number of days of history we consider for most visited entries.
const int kMostVisitedScope = 90;

// The number of recent bookmarks we show.
static const int kRecentBookmarks = 9;

// Strings sent to the page via jstemplates used to set the direction of the
// HTML document based on locale.
static const wchar_t kRTLHtmlTextDirection[] = L"rtl";
static const wchar_t kDefaultHtmlTextDirection[] = L"ltr";

bool NewTabHTMLSource::first_view_ = true;

namespace {

// To measure end-to-end performance of the new tab page, we observe paint
// messages and wait for the page to stop repainting.
class PaintTimer : public RenderWidgetHost::PaintObserver {
 public:
  PaintTimer() : method_factory_(this) {
    Start();
  }

  // Start the benchmarking and the timer.
  void Start() {
    start_ = TimeTicks::Now();
    last_paint_ = start_;
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        method_factory_.NewRunnableMethod(&PaintTimer::Timeout),
        static_cast<int>(kTimeout.InMilliseconds()));
  }

  // A callback that is invoked whenever our RenderWidgetHost paints.
  virtual void RenderWidgetHostDidPaint(RenderWidgetHost* rwh) {
    last_paint_ = TimeTicks::Now();
  }

  // The timer callback.  If enough time has elapsed since the last paint
  // message, we say we're done painting; otherwise, we keep waiting.
  void Timeout() {
    TimeTicks now = TimeTicks::Now();
    if ((now - last_paint_) >= kTimeout) {
      // Painting has quieted down.  Log this as the full time to run.
      TimeDelta load_time = last_paint_ - start_;
      int load_time_ms = static_cast<int>(load_time.InMilliseconds());
      NotificationService::current()->Notify(
          NOTIFY_INITIAL_NEW_TAB_UI_LOAD,
          NotificationService::AllSources(),
          Details<int>(&load_time_ms));
      UMA_HISTOGRAM_TIMES(L"NewTabUI load", load_time);
    } else {
      // Not enough quiet time has elapsed.
      // Some more paints must've occurred since we set the timeout.
      // Wait some more.
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          method_factory_.NewRunnableMethod(&PaintTimer::Timeout),
          static_cast<int>(kTimeout.InMilliseconds()));
    }
  }

 private:
  // The amount of time there must be no painting for us to consider painting
  // finished.  Observed times are in the ~1200ms range.
  static const TimeDelta kTimeout;
  // The time when we started benchmarking.
  TimeTicks start_;
  // The last time we got a paint notification.
  TimeTicks last_paint_;
  // Scoping so we can be sure our timeouts don't outlive us.
  ScopedRunnableMethodFactory<PaintTimer> method_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(PaintTimer);
};
const TimeDelta PaintTimer::kTimeout(TimeDelta::FromMilliseconds(2000));

// Adds "url" and "title" keys on incoming dictionary, setting title
// as the url as a fallback on empty title.
void SetURLAndTitle(DictionaryValue* dictionary, std::wstring title,
                    const GURL& gurl) {
  std::wstring wstring_url = UTF8ToWide(gurl.spec());
  dictionary->SetString(L"url", wstring_url);

  bool using_url_as_the_title = false;
  if (title.empty()) {
    using_url_as_the_title = true;
    title = wstring_url;
  }

  // Since the title can contain BiDi text, we need to mark the text as either
  // RTL or LTR, depending on the characters in the string. If we use the URL
  // as the title, we mark the title as LTR since URLs are always treated as
  // left to right strings.
  std::wstring title_to_set(title);
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    if (using_url_as_the_title) {
      l10n_util::WrapStringWithLTRFormatting(&title_to_set);
    } else {
      bool success =
          l10n_util::AdjustStringForLocaleDirection(title, &title_to_set);
      DCHECK(success ? (title != title_to_set) : (title == title_to_set));
    }
  }
  dictionary->SetString(L"title", title_to_set);
}

}  // end anonymous namespace

NewTabHTMLSource::NewTabHTMLSource()
    : DataSource("new-tab", MessageLoop::current()) {
}

void NewTabHTMLSource::StartDataRequest(const std::string& path,
                                        int request_id) {
  if (!path.empty()) {
    // A path under new-tab was requested; it's likely a bad relative
    // URL from the new tab page, but in any case it's an error.
    NOTREACHED();
    return;
  }
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_NEW_TAB_TITLE));
  localized_strings.SetString(L"mostvisited",
      l10n_util::GetString(IDS_NEW_TAB_MOST_VISITED));
  localized_strings.SetString(L"searches",
      l10n_util::GetString(IDS_NEW_TAB_SEARCHES));
  localized_strings.SetString(L"bookmarks",
      l10n_util::GetString(IDS_NEW_TAB_BOOKMARKS));
  localized_strings.SetString(L"showhistory",
      l10n_util::GetString(IDS_NEW_TAB_HISTORY_SHOW));
  localized_strings.SetString(L"searchhistory",
      l10n_util::GetString(IDS_NEW_TAB_HISTORY_SEARCH));
  localized_strings.SetString(L"closedtabs",
      l10n_util::GetString(IDS_NEW_TAB_CLOSED_TABS));
  localized_strings.SetString(L"mostvisitedintro",
      l10n_util::GetStringF(IDS_NEW_TAB_MOST_VISITED_INTRO,
          l10n_util::GetString(IDS_WELCOME_PAGE_URL)));

  localized_strings.SetString(L"textdirection",
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
       kRTLHtmlTextDirection : kDefaultHtmlTextDirection);

  // Let the tab know whether it's the first tab being viewed.
  localized_strings.SetString(L"firstview",
                              first_view_ ? L"true" : std::wstring());
  first_view_ = false;

  static const StringPiece new_tab_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_NEW_TAB_HTML));

  const std::string full_html = jstemplate_builder::GetTemplateHtml(
      new_tab_html, &localized_strings, "t" /* template root node id */);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

IncognitoTabHTMLSource::IncognitoTabHTMLSource()
    : DataSource("new-tab", MessageLoop::current()) {
}

void IncognitoTabHTMLSource::StartDataRequest(const std::string& path,
                                              int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_NEW_TAB_TITLE));
  localized_strings.SetString(L"content",
      l10n_util::GetStringF(IDS_NEW_TAB_OTR_MESSAGE,
          l10n_util::GetString(IDS_LEARN_MORE_INCOGNITO_URL)));

  localized_strings.SetString(L"textdirection",
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
       kRTLHtmlTextDirection : kDefaultHtmlTextDirection);

  static const StringPiece incognito_tab_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_INCOGNITO_TAB_HTML));

  const std::string full_html = jstemplate_builder::GetTemplateHtml(
      incognito_tab_html, &localized_strings, "t" /* template root node id */);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

ThumbnailSource::ThumbnailSource(Profile* profile)
    : DataSource(kThumbnailPath, MessageLoop::current()), profile_(profile) {}

void ThumbnailSource::StartDataRequest(const std::string& path,
                                       int request_id) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    HistoryService::Handle handle = hs->GetPageThumbnail(
        GURL(path),
        &cancelable_consumer_,
        NewCallback(this, &ThumbnailSource::OnThumbnailDataAvailable));
    // Attach the ChromeURLDataManager request ID to the history request.
    cancelable_consumer_.SetClientData(hs, handle, request_id);
  } else {
    // Tell the caller that no thumbnail is available.
    SendResponse(request_id, NULL);
  }
}

void ThumbnailSource::OnThumbnailDataAvailable(
    HistoryService::Handle request_handle,
    scoped_refptr<RefCountedBytes> data) {
  HistoryService* hs =
    profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  int request_id = cancelable_consumer_.GetClientData(hs, request_handle);
  // Forward the data along to the networking system.
  if (data.get() && !data->data.empty()) {
    SendResponse(request_id, data);
  } else {
    if (!default_thumbnail_.get()) {
      default_thumbnail_ = new RefCountedBytes;
      ResourceBundle::GetSharedInstance().LoadImageResourceBytes(
          IDR_DEFAULT_THUMBNAIL, &default_thumbnail_->data);
    }

    SendResponse(request_id, default_thumbnail_);
  }
}

FavIconSource::FavIconSource(Profile* profile)
    : DataSource(kFavIconPath, MessageLoop::current()), profile_(profile) {}

void FavIconSource::StartDataRequest(const std::string& path, int request_id) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    HistoryService::Handle handle;
    if (path.size() > 8 && path.substr(0, 8) == "iconurl/") {
      handle = hs->GetFavIcon(
          GURL(path.substr(8)),
          &cancelable_consumer_,
          NewCallback(this, &FavIconSource::OnFavIconDataAvailable));
    } else {
      handle = hs->GetFavIconForURL(
          GURL(path),
          &cancelable_consumer_,
          NewCallback(this, &FavIconSource::OnFavIconDataAvailable));
    }
    // Attach the ChromeURLDataManager request ID to the history request.
    cancelable_consumer_.SetClientData(hs, handle, request_id);
  } else {
    SendResponse(request_id, NULL);
  }
}

void FavIconSource::OnFavIconDataAvailable(
    HistoryService::Handle request_handle,
    bool know_favicon,
    scoped_refptr<RefCountedBytes> data,
    bool expired,
    GURL icon_url) {
  HistoryService* hs =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  int request_id = cancelable_consumer_.GetClientData(hs, request_handle);

  if (know_favicon && data.get() && !data->data.empty()) {
    // Forward the data along to the networking system.
    SendResponse(request_id, data);
  } else {
    if (!default_favicon_.get()) {
      default_favicon_ = new RefCountedBytes;
      ResourceBundle::GetSharedInstance().LoadImageResourceBytes(
          IDR_DEFAULT_FAVICON, &default_favicon_->data);
    }

    SendResponse(request_id, default_favicon_);
  }
}

MostVisitedHandler::MostVisitedHandler(DOMUIHost* dom_ui_host)
    : dom_ui_host_(dom_ui_host) {
  // Register ourselves as the handler for the "mostvisited" message from
  // Javascript.
  dom_ui_host_->RegisterMessageCallback("getMostVisited",
      NewCallback(this, &MostVisitedHandler::HandleGetMostVisited));

  // Set up our sources for thumbnail and favicon data.
  // Ownership is passed to the ChromeURLDataManager.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
                        &ChromeURLDataManager::AddDataSource,
                        new ThumbnailSource(dom_ui_host->profile())));
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
                        &ChromeURLDataManager::AddDataSource,
                        new FavIconSource(dom_ui_host->profile())));

  // Get notifications when history is cleared.
  NotificationService* service = NotificationService::current();
  service->AddObserver(this, NOTIFY_HISTORY_URLS_DELETED,
                       Source<Profile>(dom_ui_host_->profile()));
}

MostVisitedHandler::~MostVisitedHandler() {
  NotificationService* service = NotificationService::current();
  service->RemoveObserver(this, NOTIFY_HISTORY_URLS_DELETED,
                          Source<Profile>(dom_ui_host_->profile()));
}

void MostVisitedHandler::HandleGetMostVisited(const Value* value) {
  HistoryService* hs =
      dom_ui_host_->profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QuerySegmentUsageSince(
      &cancelable_consumer_,
      Time::Now() - TimeDelta::FromDays(kMostVisitedScope),
      NewCallback(this, &MostVisitedHandler::OnSegmentUsageAvailable));
}

void MostVisitedHandler::OnSegmentUsageAvailable(
    CancelableRequestProvider::Handle handle,
    std::vector<PageUsageData*>* data) {
  most_visited_urls_.clear();

  ListValue pages_value;
  const size_t count = std::min<size_t>(kMostVisitedPages, data->size());
  for (size_t i = 0; i < count; ++i) {
    const PageUsageData& page = *(*data)[i];
    DictionaryValue* page_value = new DictionaryValue;
    SetURLAndTitle(page_value, page.GetTitle(), page.GetURL());
    pages_value.Append(page_value);
    most_visited_urls_.push_back(page.GetURL());
  }
  dom_ui_host_->CallJavascriptFunction(L"mostVisitedPages", pages_value);
}

void MostVisitedHandler::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type != NOTIFY_HISTORY_URLS_DELETED) {
    NOTREACHED();
    return;
  }

  // Some URLs were deleted from history.  Reload the most visited list.
  HandleGetMostVisited(NULL);
}


TemplateURLHandler::TemplateURLHandler(DOMUIHost* dom_ui_host)
    : dom_ui_host_(dom_ui_host), template_url_model_(NULL) {
  dom_ui_host->RegisterMessageCallback("getMostSearched",
      NewCallback(this, &TemplateURLHandler::HandleGetMostSearched));
  dom_ui_host->RegisterMessageCallback("doSearch",
      NewCallback(this, &TemplateURLHandler::HandleDoSearch));
}

TemplateURLHandler::~TemplateURLHandler() {
  if (template_url_model_)
    template_url_model_->RemoveObserver(this);
}

void TemplateURLHandler::HandleGetMostSearched(const Value* content) {
  // The page Javascript has requested the list of keyword searches.
  // Start loading them from the template URL backend.
  if (!template_url_model_) {
    template_url_model_ = dom_ui_host_->profile()->GetTemplateURLModel();
    template_url_model_->AddObserver(this);
  }
  if (template_url_model_->loaded()) {
    OnTemplateURLModelChanged();
  } else {
    template_url_model_->Load();
  }
}

void TemplateURLHandler::HandleDoSearch(const Value* content) {
  // Extract the parameters out of the input list.
  if (!content || !content->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }
  const ListValue* args = static_cast<const ListValue*>(content);
  if (args->GetSize() != 2) {
    NOTREACHED();
    return;
  }
  std::wstring keyword, search;
  Value* value = NULL;
  if (!args->Get(0, &value) || !value->GetAsString(&keyword)) {
    NOTREACHED();
    return;
  }
  if (!args->Get(1, &value) || !value->GetAsString(&search)) {
    NOTREACHED();
    return;
  }

  // Combine the keyword and search into a URL.
  const TemplateURL* template_url =
      template_url_model_->GetTemplateURLForKeyword(keyword);
  if (!template_url) {
    // The keyword seems to have changed out from under us.
    // Not an error, but nothing we can do...
    return;
  }
  const TemplateURLRef* url_ref = template_url->url();
  if (!url_ref || !url_ref->SupportsReplacement()) {
    NOTREACHED();
    return;
  }
  std::wstring url = url_ref->ReplaceSearchTerms(*template_url, search,
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring());

  // Load the URL.
  if (!url.empty()) {
    dom_ui_host_->OpenURL(GURL(WideToUTF8(url)), CURRENT_TAB,
                          PageTransition::LINK);
  }
}

// A helper function for sorting TemplateURLs where the most used ones show up
// first.
static bool TemplateURLSortByUsage(const TemplateURL* a,
                                   const TemplateURL* b) {
  return a->usage_count() > b->usage_count();
}

void TemplateURLHandler::OnTemplateURLModelChanged() {
  // We've loaded some template URLs.  Send them to the page.
  const int kMaxURLs = 3;  // The maximum number of URLs we're willing to send.
  std::vector<const TemplateURL*> urls = template_url_model_->GetTemplateURLs();
  sort(urls.begin(), urls.end(), TemplateURLSortByUsage);
  ListValue urls_value;
  for (size_t i = 0; i < std::min<size_t>(urls.size(), kMaxURLs); ++i) {
    if (urls[i]->usage_count() == 0)
      break;  // urls is sorted by usage count; the remainder would be no good.

    const TemplateURLRef* urlref = urls[i]->url();
    if (!urlref)
      continue;
    DictionaryValue* entry_value = new DictionaryValue;
    entry_value->SetString(L"short_name", urls[i]->short_name());
    entry_value->SetString(L"keyword", urls[i]->keyword());

    const GURL& url = urls[i]->GetFavIconURL();
    if (url.is_valid())
     entry_value->SetString(L"favIconURL", UTF8ToWide(url.spec()));

    urls_value.Append(entry_value);
  }
  dom_ui_host_->CallJavascriptFunction(L"searchURLs", urls_value);
}

RecentlyBookmarkedHandler::RecentlyBookmarkedHandler(DOMUIHost* dom_ui_host)
    : dom_ui_host_(dom_ui_host),
      model_(NULL) {
  dom_ui_host->RegisterMessageCallback("getRecentlyBookmarked",
      NewCallback(this,
                  &RecentlyBookmarkedHandler::HandleGetRecentlyBookmarked));
}

RecentlyBookmarkedHandler::~RecentlyBookmarkedHandler() {
  if (model_)
    model_->RemoveObserver(this);
}

void RecentlyBookmarkedHandler::HandleGetRecentlyBookmarked(const Value*) {
  if (!model_) {
    model_ = dom_ui_host_->profile()->GetBookmarkModel();
    model_->AddObserver(this);
  }
  // If the model is loaded, synchronously send the bookmarks down. Otherwise
  // when the model loads we'll send the bookmarks down.
  if (model_->IsLoaded())
    SendBookmarksToPage();
}

void RecentlyBookmarkedHandler::SendBookmarksToPage() {
  std::vector<BookmarkNode*> recently_bookmarked;
  model_->GetMostRecentlyAddedEntries(kRecentBookmarks, &recently_bookmarked);
  ListValue list_value;
  for (size_t i = 0; i < recently_bookmarked.size(); ++i) {
    BookmarkNode* node = recently_bookmarked[i];
    DictionaryValue* entry_value = new DictionaryValue;
    SetURLAndTitle(entry_value, node->GetTitle(), node->GetURL());
    list_value.Append(entry_value);
  }
  dom_ui_host_->CallJavascriptFunction(L"recentlyBookmarked", list_value);
}

void RecentlyBookmarkedHandler::Loaded(BookmarkModel* model) {
  SendBookmarksToPage();
}

void RecentlyBookmarkedHandler::BookmarkNodeAdded(BookmarkModel* model,
                                                  BookmarkNode* parent,
                                                  int index) {
  SendBookmarksToPage();
}

void RecentlyBookmarkedHandler::BookmarkNodeRemoved(BookmarkModel* model,
                                                    BookmarkNode* parent,
                                                    int index) {
  SendBookmarksToPage();
}

void RecentlyBookmarkedHandler::BookmarkNodeChanged(BookmarkModel* model,
                                                    BookmarkNode* node) {
  SendBookmarksToPage();
}

RecentlyClosedTabsHandler::RecentlyClosedTabsHandler(DOMUIHost* dom_ui_host)
    : dom_ui_host_(dom_ui_host),
      tab_restore_service_(NULL),
      handle_recently_closed_tab_factory_(this) {
  dom_ui_host->RegisterMessageCallback("getRecentlyClosedTabs",
      NewCallback(this,
                  &RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs));
  dom_ui_host->RegisterMessageCallback("reopenTab",
      NewCallback(this, &RecentlyClosedTabsHandler::HandleReopenTab));
}

RecentlyClosedTabsHandler::~RecentlyClosedTabsHandler() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void RecentlyClosedTabsHandler::HandleReopenTab(const Value* content) {
  UserMetrics::RecordAction(L"NewTabPage_ReopenTab",
                            dom_ui_host_->profile());

  NavigationController* controller = dom_ui_host_->controller();
  Browser* browser = Browser::GetBrowserForController(
      controller, NULL);
  if (!browser)
    return;

  // Extract the integer value of the tab session to restore from the
  // incoming string array. This will be greatly simplified when
  // DOMUIBindings::send() is generalized to all data types instead of
  // silently failing when passed anything other then an array of
  // strings.
  if (content->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(content);
    Value* list_member;
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      std::wstring wstring_value;
      if (string_value->GetAsString(&wstring_value)) {
        int session_to_restore = _wtoi(wstring_value.c_str());

        const TabRestoreService::Tabs& tabs = tab_restore_service_->tabs();
        for (TabRestoreService::Tabs::const_iterator it = tabs.begin();
             it != tabs.end(); ++it) {
          if (it->id == session_to_restore) {
            browser->ReplaceRestoredTab(
                 it->navigations, it->current_navigation_index);
            // The current tab has been nuked at this point;
            // don't touch any member variables.
            break;
          }
        }
      }
    }
  }
}

void RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs(
    const Value* content) {
  if (!tab_restore_service_) {
    tab_restore_service_ = dom_ui_host_->profile()->GetTabRestoreService();

    // GetTabRestoreService() can return NULL (i.e., when in Off the
    // Record mode)
    if (tab_restore_service_)
      tab_restore_service_->AddObserver(this);
  }

  if (tab_restore_service_)
    TabRestoreServiceChanged(tab_restore_service_);
}

void RecentlyClosedTabsHandler::TabRestoreServiceChanged(
    TabRestoreService* service) {
  handle_recently_closed_tab_factory_.RevokeAll();

  const TabRestoreService::Tabs& tabs = service->tabs();
  ListValue list_value;
  int added_count = 0;

  Time now = Time::Now();
  TimeDelta expire_delta = TimeDelta::FromMinutes(5);
  Time five_minutes_ago = now - expire_delta;
  Time oldest_item = now;

  // We filter the list of recently closed to only show 'interesting'
  // tabs, where an interesting tab has navigations, was closed within
  // the last five minutes and is not the new tab ui.
  for (TabRestoreService::Tabs::const_iterator it = tabs.begin();
       it != tabs.end() && added_count < 3; ++it) {
    if (it->navigations.empty())
      continue;

    if (five_minutes_ago > it->close_time)
      continue;

    if (it->close_time < oldest_item)
      oldest_item = it->close_time;

    const TabNavigation& navigator =
        it->navigations.at(it->current_navigation_index);
    if (navigator.url == NewTabUIURL())
      continue;

    DictionaryValue* dictionary = new DictionaryValue;
    SetURLAndTitle(dictionary, navigator.title, navigator.url);
    dictionary->SetInteger(L"sessionId", it->id);

    list_value.Append(dictionary);
    added_count++;
  }

  // If we displayed anything, we must schedule a redisplay when the
  // oldest item expires.
  if (added_count) {
    TimeDelta next_run = (oldest_item + expire_delta) - now;

    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        handle_recently_closed_tab_factory_.NewRunnableMethod(
            &RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs,
            reinterpret_cast<const Value*>(NULL)),
        static_cast<int>(next_run.InMilliseconds()));
  }

  dom_ui_host_->CallJavascriptFunction(L"recentlyClosedTabs", list_value);
}

void RecentlyClosedTabsHandler::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

HistoryHandler::HistoryHandler(DOMUIHost* dom_ui_host)
    : dom_ui_host_(dom_ui_host) {
  dom_ui_host->RegisterMessageCallback("showHistoryPage",
      NewCallback(this, &HistoryHandler::HandleShowHistoryPage));
  dom_ui_host->RegisterMessageCallback("searchHistoryPage",
      NewCallback(this, &HistoryHandler::HandleSearchHistoryPage));
}

void HistoryHandler::HandleShowHistoryPage(const Value*) {
  NavigationController* controller = dom_ui_host_->controller();
  if (controller)
    controller->LoadURL(HistoryTabUI::GetURL(), PageTransition::LINK);
}

void HistoryHandler::HandleSearchHistoryPage(const Value* content) {
  if (content && content->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(content);
    Value* list_member;
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      std::wstring wstring_value;
      if (string_value->GetAsString(&wstring_value)) {
        NavigationController* controller = dom_ui_host_->controller();
        controller->LoadURL(
            HistoryTabUI::GetHistoryURLWithSearchText(wstring_value),
            PageTransition::LINK);
      }
    }
  }
}


// This is the top-level URL handler for chrome-internal: URLs, and exposed in
// our header file.
bool NewTabUIHandleURL(GURL* url,
                       TabContentsType* result_type) {
  if (!url->SchemeIs(kNewTabUIScheme))
    return false;

  *result_type = TAB_CONTENTS_NEW_TAB_UI;
  *url = GURL("chrome-resource://new-tab/");

  return true;
}

GURL NewTabUIURL() {
  std::string url(kNewTabUIScheme);
  url += ":";
  return GURL(url);
}

NewTabUIContents::NewTabUIContents(Profile* profile,
    SiteInstance* instance, RenderViewHostFactory* render_view_factory) :
    DOMUIHost(profile, instance, render_view_factory),
    motd_message_id_(0),
    incognito_(false),
    most_visited_handler_(NULL) {
  type_ = TAB_CONTENTS_NEW_TAB_UI;
  set_forced_title(l10n_util::GetString(IDS_NEW_TAB_TITLE));

  if (profile->IsOffTheRecord())
    incognito_ = true;

  if (NewTabHTMLSource::first_view() && 
      (profile->GetPrefs()->GetInteger(prefs::kRestoreOnStartup) != 0 ||
       !profile->GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage))
     ) {
    NewTabHTMLSource::set_first_view(false);
  }

  render_view_host()->SetPaintObserver(new PaintTimer);
}

void NewTabUIContents::AttachMessageHandlers() {
  // Regretfully, DataSources are global, instead of
  // per-TabContents. Because of the motd_message_id_ member, each
  // NewTabUIContents instance could theoretically have a different
  // message. Moving this from the constructor to here means that we
  // reconnect this source each time we reload so we should no longer
  // have the bug where we open a normal new tab page (no motd), open
  // another OTR new tab page (blurb motd describing what 'incognito'
  // means), refresh the normal new page (which now displays the
  // incognito blurb because that was the last NewTabHTMLSource hooked
  // up).
  //
  // This is a workaround until http://b/issue?id=1230312 is fixed.

  if (incognito_) {
    IncognitoTabHTMLSource* html_source = new IncognitoTabHTMLSource();

    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(&chrome_url_data_manager,
            &ChromeURLDataManager::AddDataSource,
            html_source));
  } else {
    AddMessageHandler(new TemplateURLHandler(this));
    most_visited_handler_ = new MostVisitedHandler(this);
    AddMessageHandler(most_visited_handler_);  // Takes ownership.
    AddMessageHandler(new RecentlyBookmarkedHandler(this));
    AddMessageHandler(new RecentlyClosedTabsHandler(this));
    AddMessageHandler(new HistoryHandler(this));

    NewTabHTMLSource* html_source = new NewTabHTMLSource();

    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(&chrome_url_data_manager,
            &ChromeURLDataManager::AddDataSource,
            html_source));
  }
}

bool NewTabUIContents::NavigateToPendingEntry(bool reload) {
  const bool result = WebContents::NavigateToPendingEntry(reload);
  controller()->GetPendingEntry()->set_title(forced_title_);
  return result;
}

const std::wstring& NewTabUIContents::GetTitle() const {
  if (!forced_title_.empty())
    return forced_title_;
  return WebContents::GetTitle();
}

void NewTabUIContents::SetInitialFocus() {
  // TODO(evanm): this code is duplicated in three places now.
  // Should probably be refactored.
  // Focus the location bar when we first get the focus.
  int tab_index;
  Browser* browser = Browser::GetBrowserForController(
      this->controller(), &tab_index);
  if (browser)
    browser->FocusLocationBar();
  else
    ::SetFocus(GetHWND());
}

bool NewTabUIContents::SupportsURL(GURL* url) {
  if (url->SchemeIs("javascript"))
    return true;
  return DOMUIHost::SupportsURL(url);
}

void NewTabUIContents::RequestOpenURL(const GURL& url,
                                      WindowOpenDisposition disposition) {
  // The user opened a URL on the page (including "open in new window").
  // We count all such clicks as AUTO_BOOKMARK, which increments the site's
  // visit count (which is used for ranking the most visited entries).
  // Note this means we're including clicks on not only most visited thumbnails,
  // but also clicks on recently bookmarked.
  OpenURL(url, disposition, PageTransition::AUTO_BOOKMARK);

  // Figure out if this was a click on a MostVisited entry, and log it if so.
  if (most_visited_handler_) {
    const std::vector<GURL>& urls = most_visited_handler_->most_visited_urls();
    for (size_t i = 0; i < urls.size(); ++i) {
      if (url == urls[i]) {
        UserMetrics::RecordComputedAction(StringPrintf(L"MostVisited%d", i),
                                          profile());
        break;
      }
    }
  }
}

