// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/history_ui.h"

#include "base/message_loop.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/time_format.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profile.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/time_format.h"
#include "net/base/escape.h"

#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

using base::Time;
using base::TimeDelta;

// HistoryUI is accessible from chrome-ui://history.
static const char kHistoryHost[] = "history";

// Maximum number of search results to return in a given search. We should
// eventually remove this.
static const int kMaxSearchResults = 100;

////////////////////////////////////////////////////////////////////////////////
//
// HistoryHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

HistoryUIHTMLSource::HistoryUIHTMLSource()
    : DataSource(kHistoryHost, MessageLoop::current()) {
}

void HistoryUIHTMLSource::StartDataRequest(const std::string& path,
                                           int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString(LIT16("title"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_TITLE)));
  localized_strings.SetString(LIT16("loading"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_LOADING)));
  localized_strings.SetString(LIT16("newest"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_NEWEST)));
  localized_strings.SetString(LIT16("newer"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_NEWER)));
  localized_strings.SetString(LIT16("older"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_OLDER)));
  localized_strings.SetString(LIT16("searchresultsfor"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_SEARCHRESULTSFOR)));
  localized_strings.SetString(LIT16("history"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_BROWSERESULTS)));
  localized_strings.SetString(LIT16("cont"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_CONTINUED)));
  localized_strings.SetString(LIT16("searchbutton"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_SEARCH_BUTTON)));
  localized_strings.SetString(LIT16("noresults"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_NO_RESULTS)));
  localized_strings.SetString(LIT16("noitems"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_NO_ITEMS)));
  localized_strings.SetString(LIT16("deleteday"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_DELETE_PRIOR_VISITS_LINK)));
  localized_strings.SetString(LIT16("deletedaywarning"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING)));

  static const StringPiece history_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_HISTORY_HTML));
  const std::string full_html = jstemplate_builder::GetTemplateHtml(
      history_html, &localized_strings, "t");

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// HistoryHandler
//
////////////////////////////////////////////////////////////////////////////////
BrowsingHistoryHandler::BrowsingHistoryHandler(DOMUI* dom_ui)
    : DOMMessageHandler(dom_ui),
      search_text_(),
      remover_(NULL) {
  dom_ui_->RegisterMessageCallback("getHistory",
      NewCallback(this, &BrowsingHistoryHandler::HandleGetHistory));
  dom_ui_->RegisterMessageCallback("searchHistory",
    NewCallback(this, &BrowsingHistoryHandler::HandleSearchHistory));
  dom_ui_->RegisterMessageCallback("deleteDay",
    NewCallback(this, &BrowsingHistoryHandler::HandleDeleteDay));

  // Create our favicon data source.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
                        &ChromeURLDataManager::AddDataSource,
                        new FavIconSource(dom_ui_->get_profile())));

  // Get notifications when history is cleared.
  NotificationService* service = NotificationService::current();
  service->AddObserver(this, NotificationType::HISTORY_URLS_DELETED,
                       Source<Profile>(dom_ui_->get_profile()));
}

BrowsingHistoryHandler::~BrowsingHistoryHandler() {
  cancelable_consumer_.CancelAllRequests();

  NotificationService* service = NotificationService::current();
  service->RemoveObserver(this, NotificationType::HISTORY_URLS_DELETED,
                          Source<Profile>(dom_ui_->get_profile()));

  if (remover_.get())
    remover_->RemoveObserver(this);
}

void BrowsingHistoryHandler::HandleGetHistory(const Value* value) {
  // Anything in-flight is invalid.
  cancelable_consumer_.CancelAllRequests();

  // Get arguments (if any).
  int day = 0;
  ExtractIntegerValue(value, &day);

  // Set our query options.
  history::QueryOptions options;
  options.begin_time = Time::Now().LocalMidnight();
  options.begin_time -= TimeDelta::FromDays(day);
  options.end_time = Time::Now().LocalMidnight();
  options.end_time -= TimeDelta::FromDays(day - 1);

  // Need to remember the query string for our results.
  search_text_ = std::wstring();

  HistoryService* hs =
      dom_ui_->get_profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text_,
      options,
      &cancelable_consumer_,
      NewCallback(this, &BrowsingHistoryHandler::QueryComplete));
}

void BrowsingHistoryHandler::HandleSearchHistory(const Value* value) {
  // Anything in-flight is invalid.
  cancelable_consumer_.CancelAllRequests();

  // Get arguments (if any).
  int month = 0;
  std::wstring query;
  ExtractSearchHistoryArguments(value, &month, &query);

  // Set the query ranges for the given month.
  history::QueryOptions options = CreateMonthQueryOptions(month);

  // When searching, limit the number of results returned and only show the
  // most recent matches.
  options.max_count = kMaxSearchResults;
  options.most_recent_visit_only = true;

  // Need to remember the query string for our results.
  search_text_ = query;
  HistoryService* hs =
      dom_ui_->get_profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text_,
      options,
      &cancelable_consumer_,
      NewCallback(this, &BrowsingHistoryHandler::QueryComplete));
}

void BrowsingHistoryHandler::HandleDeleteDay(const Value* value) {
  if (BrowsingDataRemover::is_removing()) {
    dom_ui_->CallJavascriptFunction(L"deleteFailed");
    return;
  }
  
  // Anything in-flight is invalid.
  cancelable_consumer_.CancelAllRequests();

  // Get time.
  Time time;
  bool success = Time::FromString(ExtractStringValue(value).c_str(), &time);
  DCHECK(success);

  Time begin_time = time.LocalMidnight();
  Time end_time = begin_time + TimeDelta::FromDays(1);

  remover_.reset(new BrowsingDataRemover(dom_ui_->get_profile(),
                                         begin_time,
                                         end_time));
  remover_->AddObserver(this);
  remover_->Remove(BrowsingDataRemover::REMOVE_HISTORY |
                   BrowsingDataRemover::REMOVE_COOKIES |
                   BrowsingDataRemover::REMOVE_CACHE);
}

void BrowsingHistoryHandler::OnBrowsingDataRemoverDone() {
  dom_ui_->CallJavascriptFunction(L"deleteComplete");
  remover_->RemoveObserver(this);
  remover_.release();
}

void BrowsingHistoryHandler::QueryComplete(
    HistoryService::Handle request_handle,
    history::QueryResults* results) {

  ListValue results_value;
  Time midnight_today = Time::Now().LocalMidnight();

  for (size_t i = 0; i < results->size(); ++i) {
    history::URLResult const &page = (*results)[i];
    DictionaryValue* page_value = new DictionaryValue();
    SetURLAndTitle(page_value, page.title(), page.url());

    // Need to pass the time in epoch time (fastest JS conversion).
    page_value->SetInteger(LIT16("time"),
        static_cast<int>(page.visit_time().ToTimeT()));

    // Until we get some JS i18n infrastructure, we also need to
    // pass the dates in as strings. This could use some
    // optimization.

    // Only pass in the strings we need (search results need a shortdate
    // and snippet, browse results need day and time information).
    if (search_text_.empty()) {
      // Figure out the relative date string.
      std::wstring date_str = TimeFormat::RelativeDate(page.visit_time(),
                                                       &midnight_today);
      if (date_str.empty()) {
        date_str = base::TimeFormatFriendlyDate(page.visit_time());
      } else {
        date_str = l10n_util::GetStringF(
            IDS_HISTORY_DATE_WITH_RELATIVE_TIME,
            date_str, base::TimeFormatFriendlyDate(page.visit_time()));
      }
      page_value->SetString(LIT16("dateRelativeDay"),
                            WideToUTF16Hack(date_str));
      page_value->SetString(
          LIT16("dateTimeOfDay"),
          WideToUTF16Hack(base::TimeFormatTimeOfDay(page.visit_time())));
    } else {
      page_value->SetString(
          LIT16("dateShort"),
          WideToUTF16Hack(base::TimeFormatShortDate(page.visit_time())));
      page_value->SetString(
          LIT16("snippet"), WideToUTF16Hack(page.snippet().text()));
    }
    page_value->SetBoolean(L"starred", 
        dom_ui_->get_profile()->GetBookmarkModel()->IsBookmarked(page.url()));
    results_value.Append(page_value);
  }

  StringValue temp(search_text_);
  dom_ui_->CallJavascriptFunction(L"historyResult", temp, results_value);
}

void BrowsingHistoryHandler::ExtractSearchHistoryArguments(const Value* value,
    int* month, std::wstring* query) {
  *month = 0;

  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    Value* list_member;

    // Get search string.
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
        static_cast<const StringValue*>(list_member);
      string_value->GetAsString(query);
    }

    // Get search month.
    if (list_value->Get(1, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
        static_cast<const StringValue*>(list_member);
      std::wstring wstring_value;
      string_value->GetAsString(&wstring_value);
      *month = StringToInt(WideToUTF16Hack(wstring_value));
    }
  }
}

history::QueryOptions BrowsingHistoryHandler::CreateMonthQueryOptions(
    int month) {
  history::QueryOptions options;

  // Configure the begin point of the search to the start of the
  // current month.
  Time::Exploded exploded;
  Time::Now().LocalMidnight().LocalExplode(&exploded);
  exploded.day_of_month = 1;

  if (month == 0) {
    options.begin_time = Time::FromLocalExploded(exploded);

    // Set the end time of this first search to null (which will
    // show results from the future, should the user's clock have
    // been set incorrectly).
    options.end_time = Time();
  } else {
    // Set the end-time of this search to the end of the month that is
    // |depth| months before the search end point. The end time is not
    // inclusive, so we should feel free to set it to midnight on the
    // first day of the following month.
    exploded.month -= month - 1;
    while (exploded.month < 1) {
      exploded.month += 12;
      exploded.year--;
    }
    options.end_time = Time::FromLocalExploded(exploded);

    // Set the begin-time of the search to the start of the month
    // that is |depth| months prior to search_start_.
    if (exploded.month > 1) {
      exploded.month--;
    } else {
      exploded.month = 12;
      exploded.year--;
    }
    options.begin_time = Time::FromLocalExploded(exploded);
  }

  return options;
}

void BrowsingHistoryHandler::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type != NotificationType::HISTORY_URLS_DELETED) {
    NOTREACHED();
    return;
  }

  // Some URLs were deleted from history.  Reload the most visited list.
  HandleGetHistory(NULL);
}

////////////////////////////////////////////////////////////////////////////////
//
// HistoryUIContents
//
////////////////////////////////////////////////////////////////////////////////

HistoryUI::HistoryUI(DOMUIContents* contents) : DOMUI(contents) {
}

void HistoryUI::Init() {
  AddMessageHandler(new BrowsingHistoryHandler(this));

  HistoryUIHTMLSource* html_source = new HistoryUIHTMLSource();

  // Set up the chrome-ui://history/ source.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
          &ChromeURLDataManager::AddDataSource,
          html_source));
}

// static
GURL HistoryUI::GetBaseURL() {
  std::string url = DOMUIContents::GetScheme();
  url += "://";
  url += kHistoryHost;
  return GURL(url);
}

// static
const GURL HistoryUI::GetHistoryURLWithSearchText(
  const std::wstring& text) {
    return GURL(GetBaseURL().spec() + "#q=" +
      EscapeQueryParamValue(WideToUTF8(text)));
}
