// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The history system runs on a background thread so that potentially slow
// database operations don't delay the browser. This backend processing is
// represented by HistoryBackend. The HistoryService's job is to dispatch to
// that thread.
//
// Main thread                       History thread
// -----------                       --------------
// HistoryService <----------------> HistoryBackend
//                                   -> HistoryDatabase
//                                      -> SQLite connection to History
//                                   -> ArchivedDatabase
//                                      -> SQLite connection to Archived History
//                                   -> TextDatabaseManager
//                                      -> SQLite connection to one month's data
//                                      -> SQLite connection to one month's data
//                                      ...
//                                   -> ThumbnailDatabase
//                                      -> SQLite connection to Thumbnails
//                                         (and favicons)

#include "chrome/browser/history/history.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/history/download_types.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/visitedlink_master.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"

#include "chromium_strings.h"
#include "generated_resources.h"

using base::Time;
using history::HistoryBackend;

// Sends messages from the backend to us on the main thread. This must be a
// separate class from the history service so that it can hold a reference to
// the history service (otherwise we would have to manually AddRef and
// Release when the Backend has a reference to us).
class HistoryService::BackendDelegate : public HistoryBackend::Delegate {
 public:
  explicit BackendDelegate(HistoryService* history_service)
      : history_service_(history_service),
        message_loop_(MessageLoop::current()) {
  }

  virtual void NotifyTooNew() {
    // Send the backend to the history service on the main thread.
    message_loop_->PostTask(FROM_HERE, NewRunnableMethod(history_service_.get(),
        &HistoryService::NotifyTooNew));
  }

  virtual void SetInMemoryBackend(
      history::InMemoryHistoryBackend* backend) {
    // Send the backend to the history service on the main thread.
    message_loop_->PostTask(FROM_HERE, NewRunnableMethod(history_service_.get(),
        &HistoryService::SetInMemoryBackend, backend));
  }

  virtual void BroadcastNotifications(NotificationType type,
                                      history::HistoryDetails* details) {
    // Send the notification to the history service on the main thread.
    message_loop_->PostTask(FROM_HERE, NewRunnableMethod(history_service_.get(),
        &HistoryService::BroadcastNotifications, type, details));
  }

  virtual void DBLoaded() {
    message_loop_->PostTask(FROM_HERE, NewRunnableMethod(history_service_.get(),
        &HistoryService::OnDBLoaded));
  }

 private:
  scoped_refptr<HistoryService> history_service_;
  MessageLoop* message_loop_;
};

// static
const history::StarID HistoryService::kBookmarkBarID = 1;

HistoryService::HistoryService()
    : thread_(new ChromeThread(ChromeThread::HISTORY)),
      profile_(NULL),
      backend_loaded_(false) {
  if (NotificationService::current()) {  // Is NULL when running generate_profile.
    NotificationService::current()->AddObserver(
        this, NotificationType::HISTORY_URLS_DELETED,
        Source<Profile>(profile_));
  }
}

HistoryService::HistoryService(Profile* profile)
    : thread_(new ChromeThread(ChromeThread::HISTORY)),
      profile_(profile),
      backend_loaded_(false) {
  NotificationService::current()->AddObserver(
      this, NotificationType::HISTORY_URLS_DELETED, Source<Profile>(profile_));
}

HistoryService::~HistoryService() {
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
  Cleanup();

  // Unregister for notifications.
  if (NotificationService::current()) {  // Is NULL when running generate_profile.
    NotificationService::current()->RemoveObserver(
        this, NotificationType::HISTORY_URLS_DELETED,
        Source<Profile>(profile_));
  }
}

bool HistoryService::Init(const FilePath& history_dir,
                          BookmarkService* bookmark_service) {
  if (!thread_->Start())
    return false;

  // Create the history backend.
  scoped_refptr<HistoryBackend> backend(
      new HistoryBackend(history_dir.ToWStringHack(),
                         new BackendDelegate(this),
                         bookmark_service));
  history_backend_.swap(backend);

  ScheduleAndForget(PRIORITY_UI, &HistoryBackend::Init);
  return true;
}

void HistoryService::Cleanup() {
  if (!thread_) {
    // We've already cleaned up.
    return;
  }

  // Shutdown is a little subtle. The backend's destructor must run on the
  // history thread since it is not threadsafe. So this thread must not be the
  // last thread holding a reference to the backend, or a crash could happen.
  //
  // We have a reference to the history backend. There is also an extra
  // reference held by our delegate installed in the backend, which
  // HistoryBackend::Closing will release. This means if we scheduled a call
  // to HistoryBackend::Closing and *then* released our backend reference, there
  // will be a race between us and the backend's Closing function to see who is
  // the last holder of a reference. If the backend thread's Closing manages to
  // run before we release our backend refptr, the last reference will be held
  // by this thread and the destructor will be called from here.
  //
  // Therefore, we create a task to run the Closing operation first. This holds
  // a reference to the backend. Then we release our reference, then we schedule
  // the task to run. After the task runs, it will delete its reference from
  // the history thread, ensuring everything works properly.
  Task* closing_task =
      NewRunnableMethod(history_backend_.get(), &HistoryBackend::Closing);
  history_backend_ = NULL;
  ScheduleTask(PRIORITY_NORMAL, closing_task);

  // Delete the thread, which joins with the background thread. We defensively
  // NULL the pointer before deleting it in case somebody tries to use it
  // during shutdown, but this shouldn't happen.
  ChromeThread* thread = thread_;
  thread_ = NULL;
  delete thread;
}

void HistoryService::NotifyRenderProcessHostDestruction(const void* host) {
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::NotifyRenderProcessHostDestruction, host);
}

history::URLDatabase* HistoryService::in_memory_database() const {
  if (in_memory_backend_.get())
    return in_memory_backend_->db();
  return NULL;
}

void HistoryService::SetSegmentPresentationIndex(int64 segment_id, int index) {
  ScheduleAndForget(PRIORITY_UI,
                    &HistoryBackend::SetSegmentPresentationIndex,
                    segment_id, index);
}

void HistoryService::SetKeywordSearchTermsForURL(const GURL& url,
                                                 TemplateURL::IDType keyword_id,
                                                 const std::wstring& term) {
  ScheduleAndForget(PRIORITY_UI,
                    &HistoryBackend::SetKeywordSearchTermsForURL,
                    url, keyword_id, term);
}

void HistoryService::DeleteAllSearchTermsForKeyword(
    TemplateURL::IDType keyword_id) {
  ScheduleAndForget(PRIORITY_UI,
                    &HistoryBackend::DeleteAllSearchTermsForKeyword,
                    keyword_id);
}

HistoryService::Handle HistoryService::GetMostRecentKeywordSearchTerms(
    TemplateURL::IDType keyword_id,
    const std::wstring& prefix,
    int max_count,
    CancelableRequestConsumerBase* consumer,
    GetMostRecentKeywordSearchTermsCallback* callback) {
  return Schedule(PRIORITY_UI, &HistoryBackend::GetMostRecentKeywordSearchTerms,
                  consumer,
                  new history::GetMostRecentKeywordSearchTermsRequest(callback),
                  keyword_id, prefix, max_count);
}

void HistoryService::URLsNoLongerBookmarked(const std::set<GURL>& urls) {
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::URLsNoLongerBookmarked,
                    urls);
}

HistoryService::Handle HistoryService::ScheduleDBTask(
    HistoryDBTask* task,
    CancelableRequestConsumerBase* consumer) {
  history::HistoryDBTaskRequest* request = new history::HistoryDBTaskRequest(
      NewCallback(task, &HistoryDBTask::DoneRunOnMainThread));
  request->value = task;  // The value is the task to execute.
  return Schedule(PRIORITY_UI, &HistoryBackend::ProcessDBTask, consumer,
                  request);
}

HistoryService::Handle HistoryService::QuerySegmentUsageSince(
    CancelableRequestConsumerBase* consumer,
    const Time from_time,
    SegmentQueryCallback* callback) {
  return Schedule(PRIORITY_UI, &HistoryBackend::QuerySegmentUsage,
                  consumer, new history::QuerySegmentUsageRequest(callback),
                  from_time);
}

void HistoryService::SetOnBackendDestroyTask(Task* task) {
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::SetOnBackendDestroyTask,
                    MessageLoop::current(), task);
}

void HistoryService::AddPage(const GURL& url,
                             const void* id_scope,
                             int32 page_id,
                             const GURL& referrer,
                             PageTransition::Type transition,
                             const RedirectList& redirects) {
  AddPage(url, Time::Now(), id_scope, page_id, referrer, transition, redirects);
}

void HistoryService::AddPage(const GURL& url,
                             Time time,
                             const void* id_scope,
                             int32 page_id,
                             const GURL& referrer,
                             PageTransition::Type transition,
                             const RedirectList& redirects) {
  DCHECK(history_backend_) << "History service being called after cleanup";

  // Filter out unwanted URLs. We don't add auto-subframe URLs. They are a
  // large part of history (think iframes for ads) and we never display them in
  // history UI. We will still add manual subframes, which are ones the user
  // has clicked on to get.
  if (!CanAddURL(url) || PageTransition::StripQualifier(transition) ==
                         PageTransition::AUTO_SUBFRAME)
    return;

  // Add link & all redirects to visited link list.
  VisitedLinkMaster* visited_links;
  if (profile_ && (visited_links = profile_->GetVisitedLinkMaster())) {
    visited_links->AddURL(url);

    if (!redirects.empty()) {
      // We should not be asked to add a page in the middle of a redirect chain.
      DCHECK(redirects[redirects.size() - 1] == url);

      // We need the !redirects.empty() condition above since size_t is unsigned
      // and will wrap around when we subtract one from a 0 size.
      for (size_t i = 0; i < redirects.size() - 1; i++)
        visited_links->AddURL(redirects[i]);
    }
  }

  scoped_refptr<history::HistoryAddPageArgs> request(
      new history::HistoryAddPageArgs(url, time, id_scope, page_id,
                                      referrer, redirects, transition));
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::AddPage, request);
}

void HistoryService::SetPageTitle(const GURL& url,
                                  const std::wstring& title) {
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::SetPageTitle, url, title);
}

void HistoryService::AddPageWithDetails(const GURL& url,
                                        const std::wstring& title,
                                        int visit_count,
                                        int typed_count,
                                        Time last_visit,
                                        bool hidden) {
  // Filter out unwanted URLs.
  if (!CanAddURL(url))
    return;

  // Add to the visited links system.
  VisitedLinkMaster* visited_links;
  if (profile_ && (visited_links = profile_->GetVisitedLinkMaster()))
    visited_links->AddURL(url);

  history::URLRow row(url);
  row.set_title(title);
  row.set_visit_count(visit_count);
  row.set_typed_count(typed_count);
  row.set_last_visit(last_visit);
  row.set_hidden(hidden);

  std::vector<history::URLRow> rows;
  rows.push_back(row);

  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::AddPagesWithDetails, rows);
}

void HistoryService::AddPagesWithDetails(
    const std::vector<history::URLRow>& info) {

  // Add to the visited links system.
  VisitedLinkMaster* visited_links;
  if (profile_ && (visited_links = profile_->GetVisitedLinkMaster())) {
    std::vector<GURL> urls;
    urls.reserve(info.size());
    for (std::vector<history::URLRow>::const_iterator i = info.begin();
         i != info.end();
         ++i)
      urls.push_back(i->url());

    visited_links->AddURLs(urls);
  }

  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::AddPagesWithDetails, info);
}

void HistoryService::SetPageContents(const GURL& url,
                                     const std::wstring& contents) {
  if (!CanAddURL(url))
    return;
  ScheduleAndForget(PRIORITY_LOW, &HistoryBackend::SetPageContents,
                    url, contents);
}

void HistoryService::SetPageThumbnail(const GURL& page_url,
                                      const SkBitmap& thumbnail,
                                      const ThumbnailScore& score) {
  if (!CanAddURL(page_url))
    return;

  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::SetPageThumbnail,
                    page_url, thumbnail, score);
}

HistoryService::Handle HistoryService::GetPageThumbnail(
    const GURL& page_url,
    CancelableRequestConsumerBase* consumer,
    ThumbnailDataCallback* callback) {
  return Schedule(PRIORITY_NORMAL, &HistoryBackend::GetPageThumbnail, consumer,
                  new history::GetPageThumbnailRequest(callback), page_url);
}

HistoryService::Handle HistoryService::GetFavIcon(
    const GURL& icon_url,
    CancelableRequestConsumerBase* consumer,
    FavIconDataCallback* callback) {
  // We always do image requests at lower-than-UI priority even though they
  // appear in the UI, since they can take a long time and the user can use the
  // program without them.
  return Schedule(PRIORITY_NORMAL, &HistoryBackend::GetFavIcon, consumer,
                  new history::GetFavIconRequest(callback), icon_url);
}

HistoryService::Handle HistoryService::UpdateFavIconMappingAndFetch(
    const GURL& page_url,
    const GURL& icon_url,
    CancelableRequestConsumerBase* consumer,
    FavIconDataCallback* callback) {
  return Schedule(PRIORITY_NORMAL,
                  &HistoryBackend::UpdateFavIconMappingAndFetch, consumer,
                  new history::GetFavIconRequest(callback), page_url, icon_url);
}

HistoryService::Handle HistoryService::GetFavIconForURL(
    const GURL& page_url,
    CancelableRequestConsumerBase* consumer,
    FavIconDataCallback* callback) {
  return Schedule(PRIORITY_UI, &HistoryBackend::GetFavIconForURL,
                  consumer, new history::GetFavIconRequest(callback),
                  page_url);
}

void HistoryService::SetFavIcon(const GURL& page_url,
                                const GURL& icon_url,
                                const std::vector<unsigned char>& image_data) {
  if (!CanAddURL(page_url))
    return;

  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::SetFavIcon,
      page_url, icon_url,
      scoped_refptr<RefCountedBytes>(new RefCountedBytes(image_data)));
}

void HistoryService::SetFavIconOutOfDateForPage(const GURL& page_url) {
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::SetFavIconOutOfDateForPage, page_url);
}

void HistoryService::SetImportedFavicons(
    const std::vector<history::ImportedFavIconUsage>& favicon_usage) {
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::SetImportedFavicons, favicon_usage);
}

void HistoryService::IterateURLs(URLEnumerator* enumerator) {
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::IterateURLs, enumerator);
}

HistoryService::Handle HistoryService::QueryURL(
    const GURL& url,
    bool want_visits,
    CancelableRequestConsumerBase* consumer,
    QueryURLCallback* callback) {
  return Schedule(PRIORITY_UI, &HistoryBackend::QueryURL, consumer,
                  new history::QueryURLRequest(callback), url, want_visits);
}

// Downloads -------------------------------------------------------------------

// Handle creation of a download by creating an entry in the history service's
// 'downloads' table.
HistoryService::Handle HistoryService::CreateDownload(
    const DownloadCreateInfo& create_info,
    CancelableRequestConsumerBase* consumer,
    HistoryService::DownloadCreateCallback* callback) {
  return Schedule(PRIORITY_NORMAL, &HistoryBackend::CreateDownload, consumer,
                  new history::DownloadCreateRequest(callback), create_info);
}

// Handle queries for a list of all downloads in the history database's
// 'downloads' table.
HistoryService::Handle HistoryService::QueryDownloads(
    CancelableRequestConsumerBase* consumer,
    DownloadQueryCallback* callback) {
  return Schedule(PRIORITY_NORMAL, &HistoryBackend::QueryDownloads, consumer,
                  new history::DownloadQueryRequest(callback));
}

// Handle updates for a particular download. This is a 'fire and forget'
// operation, so we don't need to be called back.
void HistoryService::UpdateDownload(int64 received_bytes,
                                    int32 state,
                                    int64 db_handle) {
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::UpdateDownload,
                    received_bytes, state, db_handle);
}

void HistoryService::UpdateDownloadPath(const std::wstring& path,
                                        int64 db_handle) {
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::UpdateDownloadPath,
                    path, db_handle);
}

void HistoryService::RemoveDownload(int64 db_handle) {
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::RemoveDownload, db_handle);
}

void HistoryService::RemoveDownloadsBetween(Time remove_begin,
                                            Time remove_end) {
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::RemoveDownloadsBetween,
                    remove_begin,
                    remove_end);
}

HistoryService::Handle HistoryService::SearchDownloads(
    const std::wstring& search_text,
    CancelableRequestConsumerBase* consumer,
    DownloadSearchCallback* callback) {
  return Schedule(PRIORITY_NORMAL, &HistoryBackend::SearchDownloads, consumer,
                  new history::DownloadSearchRequest(callback), search_text);
}

HistoryService::Handle HistoryService::QueryHistory(
    const std::wstring& text_query,
    const history::QueryOptions& options,
    CancelableRequestConsumerBase* consumer,
    QueryHistoryCallback* callback) {
  return Schedule(PRIORITY_UI, &HistoryBackend::QueryHistory, consumer,
                  new history::QueryHistoryRequest(callback),
                  text_query, options);
}

HistoryService::Handle HistoryService::QueryRedirectsFrom(
    const GURL& from_url,
    CancelableRequestConsumerBase* consumer,
    QueryRedirectsCallback* callback) {
  return Schedule(PRIORITY_UI, &HistoryBackend::QueryRedirectsFrom, consumer,
      new history::QueryRedirectsRequest(callback), from_url);
}

HistoryService::Handle HistoryService::GetVisitCountToHost(
    const GURL& url,
    CancelableRequestConsumerBase* consumer,
    GetVisitCountToHostCallback* callback) {
  return Schedule(PRIORITY_UI, &HistoryBackend::GetVisitCountToHost, consumer,
      new history::GetVisitCountToHostRequest(callback), url);
}

void HistoryService::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type != NotificationType::HISTORY_URLS_DELETED) {
    NOTREACHED();
    return;
  }

  // Update the visited link system for deleted URLs. We will update the
  // visited link system for added URLs as soon as we get the add
  // notification (we don't have to wait for the backend, which allows us to
  // be faster to update the state).
  //
  // For deleted URLs, we don't typically know what will be deleted since
  // delete notifications are by time. We would also like to be more
  // respectful of privacy and never tell the user something is gone when it
  // isn't. Therefore, we update the delete URLs after the fact.
  if (!profile_)
    return;  // No profile, probably unit testing.
  Details<history::URLsDeletedDetails> deleted_details(details);
  VisitedLinkMaster* visited_links = profile_->GetVisitedLinkMaster();
  if (!visited_links)
    return;  // Nobody to update.
  if (deleted_details->all_history)
    visited_links->DeleteAllURLs();
  else  // Delete individual ones.
    visited_links->DeleteURLs(deleted_details->urls);
}

void HistoryService::ScheduleAutocomplete(HistoryURLProvider* provider,
                                          HistoryURLProviderParams* params) {
  ScheduleAndForget(PRIORITY_UI, &HistoryBackend::ScheduleAutocomplete,
                    scoped_refptr<HistoryURLProvider>(provider), params);
}

void HistoryService::ScheduleTask(SchedulePriority priority,
                                  Task* task) {
  // FIXME(brettw) do prioritization.
  thread_->message_loop()->PostTask(FROM_HERE, task);
}

bool HistoryService::CanAddURL(const GURL& url) const {
  if (!url.is_valid())
    return false;

  if (url.SchemeIs("javascript") ||
      url.SchemeIs("chrome") ||
      url.SchemeIs("view-source"))
    return false;

  if (url.SchemeIs("about")) {
    std::string path = url.path();
    if (path.empty() || LowerCaseEqualsASCII(path, "blank"))
      return false;
    // We allow all other about URLs since the user may like to see things
    // like "about:memory" or "about:histograms" in their history and
    // autocomplete.
  }

  return true;
}

void HistoryService::SetInMemoryBackend(
    history::InMemoryHistoryBackend* mem_backend) {
  DCHECK(!in_memory_backend_.get()) << "Setting mem DB twice";
  in_memory_backend_.reset(mem_backend);

  // The database requires additional initialization once we own it.
  in_memory_backend_->AttachToHistoryService(profile_);
}

void HistoryService::NotifyTooNew() {
  // Find the last browser window to display our message box from.
  Browser* cur_browser = BrowserList::GetLastActive();
  // TODO(brettw): Do this some other way or beng will kick you. e.g. move to
  //               BrowserView.
  HWND parent_hwnd =
      reinterpret_cast<HWND>(cur_browser->window()->GetNativeHandle());
  HWND cur_hwnd = cur_browser ? parent_hwnd : NULL;

  std::wstring title = l10n_util::GetString(IDS_PRODUCT_NAME);
  std::wstring message = l10n_util::GetString(IDS_PROFILE_TOO_NEW_ERROR);
  MessageBox(cur_hwnd, message.c_str(), title.c_str(),
             MB_OK | MB_ICONWARNING | MB_TOPMOST);
}

void HistoryService::DeleteURL(const GURL& url) {
  // We will update the visited links when we observe the delete notifications.
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::DeleteURL, url);
}

void HistoryService::ExpireHistoryBetween(
    Time begin_time, Time end_time,
    CancelableRequestConsumerBase* consumer,
    ExpireHistoryCallback* callback) {

  // We will update the visited links when we observe the delete notifications.
  Schedule(PRIORITY_UI, &HistoryBackend::ExpireHistoryBetween, consumer,
                        new history::ExpireHistoryRequest(callback),
                        begin_time, end_time);
}

void HistoryService::BroadcastNotifications(
    NotificationType type,
    history::HistoryDetails* details_deleted) {
  // We take ownership of the passed-in pointer and delete it. It was made for
  // us on another thread, so the caller doesn't know when we will handle it.
  scoped_ptr<history::HistoryDetails> details(details_deleted);
  // TODO(evanm): this is currently necessitated by generate_profile, which
  // runs without a browser process. generate_profile should really create
  // a browser process, at which point this check can then be nuked.
  if (!g_browser_process)
    return;

  // The source of all of our notifications is the profile. Note that this
  // pointer is NULL in unit tests.
  Source<Profile> source(profile_);

  // The details object just contains the pointer to the object that the
  // backend has allocated for us. The receiver of the notification will cast
  // this to the proper type.
  Details<history::HistoryDetails> det(details_deleted);

  NotificationService::current()->Notify(type, source, det);
}

void HistoryService::OnDBLoaded() {
  LOG(INFO) << "History backend finished loading";
  backend_loaded_ = true;
  NotificationService::current()->Notify(NotificationType::HISTORY_LOADED,
                                         Source<Profile>(profile_),
                                         Details<HistoryService>(this));
}
