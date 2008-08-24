// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include "chrome/browser/download_manager.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/timer.h"
#include "base/win_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download_file.h"
#include "chrome/browser/download_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/resource_dispatcher_host.h"
#include "chrome/browser/tab_util.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/common/win_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context.h"

#include "generated_resources.h"

// Periodically update our observers.
class DownloadItemUpdateTask : public Task {
 public:
  explicit DownloadItemUpdateTask(DownloadItem* item) : item_(item) {}
  void Run() { if (item_) item_->UpdateObservers(); }

 private:
  DownloadItem* item_;
};

// Update frequency (milliseconds).
static const int kUpdateTimeMs = 1000;

// Our download table ID starts at 1, so we use 0 to represent a download that
// has started, but has not yet had its data persisted in the table. We use fake
// database handles in incognito mode starting at -1 and progressly getting more
// negative.
static const int kUninitializedHandle = 0;

// Attempts to modify |path| to be a non-existing path.
// Returns true if |path| points to a non-existing path upon return.
static bool UniquifyPath(std::wstring* path) {
  DCHECK(path);
  const int kMaxAttempts = 100;

  if (!file_util::PathExists(*path))
    return true;

  std::wstring new_path;
  for (int count = 1; count <= kMaxAttempts; ++count) {
    new_path.assign(*path);
    file_util::InsertBeforeExtension(&new_path, StringPrintf(L" (%d)", count));

    if (!file_util::PathExists(new_path)) {
      path->swap(new_path);
      return true;
    }
  }

  return false;
}

// DownloadItem implementation -------------------------------------------------

// Constructor for reading from the history service.
DownloadItem::DownloadItem(const DownloadCreateInfo& info)
    : id_(-1),
      full_path_(info.path),
      url_(info.url),
      total_bytes_(info.total_bytes),
      received_bytes_(info.received_bytes),
      start_tick_(0),
      state_(static_cast<DownloadState>(info.state)),
      start_time_(info.start_time),
      db_handle_(info.db_handle),
      update_task_(NULL),
      timer_(NULL),
      manager_(NULL),
      is_paused_(false),
      open_when_complete_(false),
      render_process_id_(-1),
      request_id_(-1) {
  if (state_ == IN_PROGRESS)
    state_ = CANCELLED;
  Init(false /* don't start progress timer */);
}

// Constructor for DownloadItem created via user action in the main thread.
DownloadItem::DownloadItem(int32 download_id,
                           const std::wstring& path,
                           const std::wstring& url,
                           const Time start_time,
                           int64 download_size,
                           int render_process_id,
                           int request_id)
    : id_(download_id),
      full_path_(path),
      url_(url),
      total_bytes_(download_size),
      received_bytes_(0),
      start_tick_(GetTickCount()),
      state_(IN_PROGRESS),
      start_time_(start_time),
      db_handle_(kUninitializedHandle),
      update_task_(NULL),
      timer_(NULL),
      manager_(NULL),
      is_paused_(false),
      open_when_complete_(false),
      render_process_id_(render_process_id),
      request_id_(request_id) {
  Init(true /* start progress timer */);
}

void DownloadItem::Init(bool start_timer) {
  file_name_ = file_util::GetFilenameFromPath(full_path_);
  if (start_timer)
    StartProgressTimer();
}

DownloadItem::~DownloadItem() {
  DCHECK(timer_ == NULL && update_task_ == NULL);
  state_ = REMOVING;
  UpdateObservers();
}

void DownloadItem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DownloadItem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DownloadItem::UpdateObservers() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadUpdated(this));
}

// If we've received more data than we were expecting (bad server info?), revert
// to 'unknown size mode'.
void DownloadItem::UpdateSize(int64 bytes_so_far) {
  received_bytes_ = bytes_so_far;
  if (received_bytes_ > total_bytes_)
    total_bytes_ = 0;
}

// Updates from the download thread may have been posted while this download
// was being cancelled in the UI thread, so we'll accept them unless we're
// complete.
void DownloadItem::Update(int64 bytes_so_far) {
  if (state_ == COMPLETE) {
    NOTREACHED();
    return;
  }
  UpdateSize(bytes_so_far);
  UpdateObservers();
}

// Triggered by a user action
void DownloadItem::Cancel(bool update_history) {
  if (state_ != IN_PROGRESS) {
    // Small downloads might be complete before this method has a chance to run.
    return;
  }
  state_ = CANCELLED;
  UpdateObservers();
  StopProgressTimer();
  if (update_history)
    manager_->DownloadCancelled(id_);
}

void DownloadItem::Finished(int64 size) {
  state_ = COMPLETE;
  UpdateSize(size);
  UpdateObservers();
  StopProgressTimer();
}

void DownloadItem::Remove() {
  Cancel(true);
  state_ = REMOVING;
  manager_->RemoveDownload(db_handle_);
}

void DownloadItem::StartProgressTimer() {
  DCHECK(update_task_ == NULL && timer_ == NULL);
  update_task_ = new DownloadItemUpdateTask(this);
  TimerManager* tm = MessageLoop::current()->timer_manager();
  timer_ = tm->StartTimer(kUpdateTimeMs, update_task_, true);
}

void DownloadItem::StopProgressTimer() {
  if (timer_) {
    MessageLoop::current()->timer_manager()->StopTimer(timer_);
    delete timer_;
    timer_ = NULL;
    delete update_task_;
    update_task_ = NULL;
  }
}

bool DownloadItem::TimeRemaining(TimeDelta* remaining) const {
  if (total_bytes_ <= 0)
    return false;  // We never received the content_length for this download.

  int64 speed = CurrentSpeed();
  if (speed == 0)
    return false;

  *remaining =
      TimeDelta::FromSeconds((total_bytes_ - received_bytes_) / speed);
  return true;
}

int64 DownloadItem::CurrentSpeed() const {
  uintptr_t diff = GetTickCount() - start_tick_;
  return diff == 0 ? 0 : received_bytes_ * 1000 / diff;
}

int DownloadItem::PercentComplete() const {
  int percent = -1;
  if (total_bytes_ > 0)
    percent = static_cast<int>(received_bytes_ * 100.0 / total_bytes_);
  return percent;
}

void DownloadItem::Rename(const std::wstring& full_path) {
  DCHECK(!full_path.empty());
  full_path_ = full_path;
  file_name_ = file_util::GetFilenameFromPath(full_path_);
}

void DownloadItem::TogglePause() {
  DCHECK(state_ == IN_PROGRESS);
  manager_->PauseDownload(id_, !is_paused_);
  is_paused_ = !is_paused_;
  UpdateObservers();
}

// DownloadManager implementation ----------------------------------------------

// static
void DownloadManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kPromptForDownload, false);
  prefs->RegisterStringPref(prefs::kDownloadExtensionsToOpen, L"");
}

DownloadManager::DownloadManager()
    : shutdown_needed_(false),
      profile_(NULL),
      file_manager_(NULL),
      ui_loop_(MessageLoop::current()),
      file_loop_(NULL) {
}

DownloadManager::~DownloadManager() {
  if (shutdown_needed_)
    Shutdown();
}

void DownloadManager::Shutdown() {
  DCHECK(shutdown_needed_) << "Shutdown called when not needed.";

  // Stop receiving download updates
  file_manager_->RemoveDownloadManager(this);

  // Stop making history service requests
  cancelable_consumer_.CancelAllRequests();

  // 'in_progress_' may contain DownloadItems that have not finished the start
  // complete (from the history service) and thus aren't in downloads_.
  DownloadMap::iterator it = in_progress_.begin();
  for (; it != in_progress_.end(); ++it) {
    DownloadItem* download = it->second;
    if (download->state() == DownloadItem::IN_PROGRESS) {
      download->Cancel(false);
      UpdateHistoryForDownload(download);
    }
    if (download->db_handle() == kUninitializedHandle) {
      // An invalid handle means that 'download' does not yet exist in
      // 'downloads_', so we have to delete it here.
      delete download;
    }
  }

  in_progress_.clear();
  STLDeleteValues(&downloads_);

  file_manager_ = NULL;

  // Save our file extensions to auto open.
  SaveAutoOpens();

  // Make sure the save as dialog doesn't notify us back if we're gone before
  // it returns.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  shutdown_needed_ = false;
}

// Issue a history query for downloads matching 'search_text'. If 'search_text'
// is empty, return all downloads that we know about.
void DownloadManager::GetDownloads(Observer* observer,
                                   const std::wstring& search_text) {
  DCHECK(observer);

  // Return a empty list if we've not yet received the set of downloads from the
  // history system (we'll update all observers once we get that list in
  // OnQueryDownloadEntriesComplete), or if there are no downloads at all.
  std::vector<DownloadItem*> download_copy;
  if (downloads_.empty()) {
    observer->SetDownloads(download_copy);
    return;
  }

  // We already know all the downloads and there is no filter, so just return a
  // copy to the observer.
  if (search_text.empty()) {
    download_copy.reserve(downloads_.size());
    for (DownloadMap::iterator it = downloads_.begin();
         it != downloads_.end(); ++it) {
      download_copy.push_back(it->second);
    }

    // We retain ownership of the DownloadItems.
    observer->SetDownloads(download_copy);
    return;
  }

  // Issue a request to the history service for a list of downloads matching
  // our search text.
  HistoryService* hs =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    HistoryService::Handle h =
        hs->SearchDownloads(search_text,
                            &cancelable_consumer_,
                            NewCallback(this,
                                        &DownloadManager::OnSearchComplete));
    cancelable_consumer_.SetClientData(hs, h, observer);
  }
}

// Query the history service for information about all persisted downloads.
bool DownloadManager::Init(Profile* profile) {
  DCHECK(profile);
  DCHECK(!shutdown_needed_)  << "DownloadManager already initialized.";
  shutdown_needed_ = true;

  profile_ = profile;
  request_context_ = profile_->GetRequestContext();

  // 'incognito mode' will have access to past downloads, but we won't store
  // information about new downloads while in that mode.
  QueryHistoryForDownloads();

  ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
  if (!rdh) {
    NOTREACHED();
    return false;
  }

  file_manager_ = rdh->download_file_manager();
  if (!file_manager_) {
    NOTREACHED();
    return false;
  }

  file_loop_ = g_browser_process->file_thread()->message_loop();
  if (!file_loop_) {
    NOTREACHED();
    return false;
  }

  // Get our user preference state.
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  prompt_for_download_.Init(prefs::kPromptForDownload, prefs, NULL);

  // Use the IE download directory on Vista, if available.
  std::wstring default_download_path;
  if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA) {
    RegKey vista_reg(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Internet Explorer", KEY_READ);
    const wchar_t* const vista_dir = L"Download Directory";
    if (vista_reg.ValueExists(vista_dir))
      vista_reg.ReadValue(vista_dir, &default_download_path);
  }
  if (default_download_path.empty()) {
    PathService::Get(chrome::DIR_USER_DOCUMENTS, &default_download_path);
    file_util::AppendToPath(&default_download_path,
                            l10n_util::GetString(IDS_DOWNLOAD_DIRECTORY));
  }
  // Check if the pref has already been registered, as the user profile and the
  // "off the record" profile might register it.
  if (!prefs->IsPrefRegistered(prefs::kDownloadDefaultDirectory))
    prefs->RegisterStringPref(prefs::kDownloadDefaultDirectory,
                              default_download_path);
  download_path_.Init(prefs::kDownloadDefaultDirectory, prefs, NULL);

  // Ensure that the download directory specified in the preferences exists.
  file_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      file_manager_, &DownloadFileManager::CreateDirectory, *download_path_));

  // We store any file extension that should be opened automatically at
  // download completion in this pref.
  download_util::InitializeExeTypes(&exe_types_);

  std::wstring extensions_to_open =
      prefs->GetString(prefs::kDownloadExtensionsToOpen);
  std::vector<std::wstring> extensions;
  SplitString(extensions_to_open, L':', &extensions);
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (!extensions[i].empty() && !IsExecutable(extensions[i]))
      auto_open_.insert(extensions[i]);
  }

  return true;
}

void DownloadManager::QueryHistoryForDownloads() {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->QueryDownloads(
        &cancelable_consumer_,
        NewCallback(this, &DownloadManager::OnQueryDownloadEntriesComplete));
  }
}

// We have received a message from DownloadFileManager about a new download. We
// create a download item and store it in our download map, and inform the
// history system of a new download. Since this method can be called while the
// history service thread is still reading the persistent state, we do not
// insert the new DownloadItem into 'downloads_' or inform our observers at this
// point. OnCreateDatabaseEntryComplete() handles that finalization of the the
// download creation as a callback from the history thread.
void DownloadManager::StartDownload(DownloadCreateInfo* info) {
  DCHECK(MessageLoop::current() == ui_loop_);
  DCHECK(info);

  // Determine the proper path for a download, by choosing either the default
  // download directory, or prompting the user.
  std::wstring generated_name;
  GenerateFilename(info, &generated_name);
  if (*prompt_for_download_ && !last_download_path_.empty())
    info->suggested_path = last_download_path_;
  else
    info->suggested_path = *download_path_;
  file_util::AppendToPath(&info->suggested_path, generated_name);

  // We need to move over to the download thread because we don't want to stat
  // the suggested path on the UI thread.
  file_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadManager::CheckIfSuggestedPathExists,
                        info));
}

void DownloadManager::CheckIfSuggestedPathExists(DownloadCreateInfo* info) {
  DCHECK(info);

  // Check writability of the suggested path. If we can't write to it, default
  // to the user's "My Documents" directory. We'll prompt them in this case.
  std::wstring path = file_util::GetDirectoryFromPath(info->suggested_path);
  if (!file_util::PathIsWritable(path)) {
    info->save_as = true;
    const std::wstring filename =
        file_util::GetFilenameFromPath(info->suggested_path);
    PathService::Get(chrome::DIR_USER_DOCUMENTS, &info->suggested_path);
    file_util::AppendToPath(&info->suggested_path, filename);
  }

  info->suggested_path_exists = !UniquifyPath(&info->suggested_path);

  // Now we return to the UI thread.
  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadManager::OnPathExistenceAvailable,
                        info));
}

void DownloadManager::OnPathExistenceAvailable(DownloadCreateInfo* info) {
  DCHECK(MessageLoop::current() == ui_loop_);
  DCHECK(info);

  if (*prompt_for_download_ || info->save_as || info->suggested_path_exists) {
    // We must ask the user for the place to put the download.
    if (!select_file_dialog_.get())
      select_file_dialog_ = SelectFileDialog::Create(this);

    TabContents* contents = tab_util::GetTabContentsByID(
        info->render_process_id, info->render_view_id);
    HWND owning_hwnd =
        contents ? GetAncestor(contents->GetContainerHWND(), GA_ROOT) : NULL;
    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
                                    std::wstring(), info->suggested_path,
                                    owning_hwnd, info);
  } else {
    // No prompting for download, just continue with the suggested name.
    ContinueStartDownload(info, info->suggested_path);
  }
}

void DownloadManager::ContinueStartDownload(DownloadCreateInfo* info,
                                            const std::wstring& target_path) {
  scoped_ptr<DownloadCreateInfo> infop(info);
  info->path = target_path;

  DownloadItem* download = NULL;
  DownloadMap::iterator it = in_progress_.find(info->download_id);
  if (it == in_progress_.end()) {
    download = new DownloadItem(info->download_id,
                                info->path,
                                info->url,
                                info->start_time,
                                info->total_bytes,
                                info->render_process_id,
                                info->request_id);
    download->set_manager(this);
    in_progress_[info->download_id] = download;
  } else {
    NOTREACHED();  // Should not exist!
    return;
  }

  // If the download already completed by the time we reached this point, then
  // notify observers that it did.
  PendingFinishedMap::iterator pending_it =
      pending_finished_downloads_.find(info->download_id);
  if (pending_it != pending_finished_downloads_.end())
    DownloadFinished(pending_it->first, pending_it->second);

  download->Rename(target_path);

  file_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &DownloadFileManager::OnFinalDownloadName,
                        download->id(),
                        target_path));

  if (profile_->IsOffTheRecord()) {
    // Fake a db handle for incognito mode, since nothing is actually stored in
    // the database in this mode. We have to make sure that these handles don't
    // collide with normal db handles, so we use a negative value. Eventually,
    // they could overlap, but you'd have to do enough downloading that your ISP
    // would likely stab you in the neck first. YMMV.
    static int64 fake_db_handle = kUninitializedHandle - 1;
    OnCreateDownloadEntryComplete(*info, fake_db_handle--);
  } else {
    // Update the history system with the new download.
    // FIXME(acw|paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
    HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (hs) {
      hs->CreateDownload(
          *info, &cancelable_consumer_,
          NewCallback(this, &DownloadManager::OnCreateDownloadEntryComplete));
    }
  }
}

// Convenience function for updating the history service for a download.
void DownloadManager::UpdateHistoryForDownload(DownloadItem* download) {
  DCHECK(download);

  // Don't store info in the database if the download was initiated while in
  // incognito mode or if it hasn't been initialized in our database table.
  if (download->db_handle() <= kUninitializedHandle)
    return;

  // FIXME(acw|paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->UpdateDownload(download->received_bytes(),
                       download->state(),
                       download->db_handle());
  }
}

void DownloadManager::RemoveDownloadFromHistory(DownloadItem* download) {
  DCHECK(download);
  // FIXME(acw|paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (download->db_handle() > kUninitializedHandle && hs)
    hs->RemoveDownload(download->db_handle());
}

void DownloadManager::RemoveDownloadsFromHistoryBetween(const Time remove_begin,
                                                        const Time remove_end) {
  // FIXME(acw|paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->RemoveDownloadsBetween(remove_begin, remove_end);
}

void DownloadManager::UpdateDownload(int32 download_id, int64 size) {
  DownloadMap::iterator it = in_progress_.find(download_id);
  if (it != in_progress_.end()) {
    DownloadItem* download = it->second;
    download->Update(size);
    UpdateHistoryForDownload(download);
  }
}

void DownloadManager::DownloadFinished(int32 download_id, int64 size) {
  DownloadMap::iterator it = in_progress_.find(download_id);
  if (it != in_progress_.end()) {
    // Remove the id from the list of pending ids.
    PendingFinishedMap::iterator erase_it =
        pending_finished_downloads_.find(download_id);
    if (erase_it != pending_finished_downloads_.end())
      pending_finished_downloads_.erase(erase_it);

    DownloadItem* download = it->second;
    download->Finished(size);

    // Open the download if the user or user prefs indicate it should be.
    const std::wstring extension =
        file_util::GetFileExtensionFromPath(download->full_path());
    if (download->open_when_complete() || ShouldOpenFileExtension(extension))
      OpenDownloadInShell(download, NULL);

    // Clean up will happen when the history system create callback runs if we
    // don't have a valid db_handle yet.
    if (download->db_handle() != kUninitializedHandle) {
      in_progress_.erase(it);
      NotifyAboutDownloadStop();
      UpdateHistoryForDownload(download);
    }
  } else {
    // The download is done, but the user hasn't selected a final location for
    // it yet (the Save As dialog box is probably still showing), so just keep
    // track of the fact that this download id is complete, when the
    // DownloadItem is constructed later we'll notify its completion then.
    PendingFinishedMap::iterator erase_it =
        pending_finished_downloads_.find(download_id);
    DCHECK(erase_it == pending_finished_downloads_.end());
    pending_finished_downloads_[download_id] = size;
  }
}

// static
// We have to tell the ResourceDispatcherHost to cancel the download from this
// thread, since we can't forward tasks from the file thread to the io thread
// reliably (crash on shutdown race condition).
void DownloadManager::CancelDownloadRequest(int render_process_id,
                                            int request_id) {
  ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
  Thread* io_thread = g_browser_process->io_thread();
  if (!io_thread || !rdh)
    return;
  io_thread->message_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(&DownloadManager::OnCancelDownloadRequest,
                          rdh,
                          render_process_id,
                          request_id));
}

// static
void DownloadManager::OnCancelDownloadRequest(ResourceDispatcherHost* rdh,
                                              int render_process_id,
                                              int request_id) {
  rdh->CancelRequest(render_process_id, request_id, false);
}

void DownloadManager::DownloadCancelled(int32 download_id) {
  DownloadMap::iterator it = in_progress_.find(download_id);
  if (it == in_progress_.end())
    return;
  DownloadItem* download = it->second;

  CancelDownloadRequest(download->render_process_id(), download->request_id());

  // Clean up will happen when the history system create callback runs if we
  // don't have a valid db_handle yet.
  if (download->db_handle() != kUninitializedHandle) {
    in_progress_.erase(it);
    NotifyAboutDownloadStop();
    UpdateHistoryForDownload(download);
  }

  // Tell the file manager to cancel the download.
  file_manager_->RemoveDownload(download->id(), this);  // On the UI thread
  file_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &DownloadFileManager::CancelDownload,
                        download->id()));
}

void DownloadManager::PauseDownload(int32 download_id, bool pause) {
  DownloadMap::iterator it = in_progress_.find(download_id);
  if (it != in_progress_.end()) {
    DownloadItem* download = it->second;
    if (pause == download->is_paused())
      return;

    // Inform the ResourceDispatcherHost of the new pause state.
    Thread* io_thread = g_browser_process->io_thread();
    ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
    if (!io_thread || !rdh)
      return;

    io_thread->message_loop()->PostTask(FROM_HERE,
        NewRunnableFunction(&DownloadManager::OnPauseDownloadRequest,
                            rdh,
                            download->render_process_id(),
                            download->request_id(),
                            pause));
  }
}

// static
void DownloadManager::OnPauseDownloadRequest(ResourceDispatcherHost* rdh,
                                             int render_process_id,
                                             int request_id,
                                             bool pause) {
  rdh->PauseRequest(render_process_id, request_id, pause);
}

void DownloadManager::RemoveDownload(int64 download_handle) {
  DownloadMap::iterator it = downloads_.find(download_handle);
  if (it == downloads_.end())
    return;

  // Make history update.
  DownloadItem* download = it->second;
  RemoveDownloadFromHistory(download);

  // Remove from our tables and delete.
  downloads_.erase(it);
  delete download;

  // Tell observers to refresh their views.
  FOR_EACH_OBSERVER(Observer, observers_, ModelChanged());
}

int DownloadManager::RemoveDownloadsBetween(const Time remove_begin,
                                            const Time remove_end) {
  RemoveDownloadsFromHistoryBetween(remove_begin, remove_end);

  int num_deleted = 0;
  DownloadMap::iterator it = downloads_.begin();
  while (it != downloads_.end()) {
    DownloadItem* download = it->second;
    DownloadItem::DownloadState state = download->state();
    if (download->start_time() >= remove_begin &&
        (remove_end.is_null() || download->start_time() < remove_end) &&
        (state == DownloadItem::COMPLETE ||
         state == DownloadItem::CANCELLED)) {
      // Remove from the map and move to the next in the list.
      it = downloads_.erase(it);
      delete download;

      ++num_deleted;
      continue;
    }

    ++it;
  }

  // Tell observers to refresh their views.
  if (num_deleted > 0)
    FOR_EACH_OBSERVER(Observer, observers_, ModelChanged());

  return num_deleted;
}

int DownloadManager::RemoveDownloads(const Time remove_begin) {
  return RemoveDownloadsBetween(remove_begin, Time());
}

// Initiate a download of a specific URL. We send the request to the
// ResourceDispatcherHost, and let it send us responses like a regular
// download.
void DownloadManager::DownloadUrl(const GURL& url,
                                  const GURL& referrer,
                                  WebContents* web_contents) {
  DCHECK(web_contents);
  file_manager_->DownloadUrl(url,
                             referrer,
                             web_contents->process()->host_id(),
                             web_contents->render_view_host()->routing_id(),
                             request_context_.get());
}

void DownloadManager::NotifyAboutDownloadStart() {
  NotificationService::current()->
      Notify(NOTIFY_DOWNLOAD_START, NotificationService::AllSources(),
             NotificationService::NoDetails());
}

void DownloadManager::NotifyAboutDownloadStop() {
  NotificationService::current()->
      Notify(NOTIFY_DOWNLOAD_STOP, NotificationService::AllSources(),
             NotificationService::NoDetails());
}

void DownloadManager::GenerateExtension(const std::wstring& file_name,
                                        const std::string& mime_type,
                                        std::wstring* generated_extension) {
  // We're worried about three things here:
  //
  // 1) Security.  Many sites let users upload content, such as buddy icons, to
  //    their web sites.  We want to mitigate the case where an attacker
  //    supplies a malicious executable with an executable file extension but an
  //    honest site serves the content with a benign content type, such as
  //    image/jpeg.
  //
  // 2) Usability.  If the site fails to provide a file extension, we want to
  //    guess a reasonable file extension based on the content type.
  //
  // 3) Shell integration.  Some file extensions automatically integrate with
  //    the shell.  We block these extensions to prevent a malicious web site
  //    from integrating with the user's shell.

  static const wchar_t default_extension[] = L"download";

  // See if our file name already contains an extension.
  std::wstring extension(file_util::GetFileExtensionFromPath(file_name));

  // Rename shell-integrated extensions.
  if (win_util::IsShellIntegratedExtension(extension))
    extension.assign(default_extension);

  std::string mime_type_from_extension;
  net::GetMimeTypeFromFile(file_name, &mime_type_from_extension);
  if (mime_type == mime_type_from_extension) {
    // The hinted extension matches the mime type.  It looks like a winner.
    generated_extension->swap(extension);
    return;
  }

  if (IsExecutable(extension) && !IsExecutableMimeType(mime_type)) {
    // We want to be careful about executable extensions.  The worry here is
    // that a trusted web site could be tricked into dropping an executable file
    // on the user's filesystem.
    if (!net::GetPreferredExtensionForMimeType(mime_type, &extension)) {
      // We couldn't find a good extension for this content type.  Use a dummy
      // extension instead.
      extension.assign(default_extension);
    }
  }

  if (extension.empty()) {
    net::GetPreferredExtensionForMimeType(mime_type, &extension);
  } else {
    // Append entension generated from the mime type if:
    // 1. New extension is not ".txt"
    // 2. New extension is not the same as the already existing extension.
    // 3. New extension is not executable. This action mitigates the case when
    //    an execuatable is hidden in a benign file extension;
    //    E.g. my-cat.jpg becomes my-cat.jpg.js if content type is
    //         application/x-javascript.
    std::wstring append_extension;
    if (net::GetPreferredExtensionForMimeType(mime_type, &append_extension)) {
      if (append_extension != L".txt" && append_extension != extension &&
          !IsExecutable(append_extension))
        extension += append_extension;
    }
  }

  generated_extension->swap(extension);
}

void DownloadManager::GenerateFilename(DownloadCreateInfo* info,
                                       std::wstring* generated_name) {
  std::wstring file_name =
      net::GetSuggestedFilename(GURL(info->url),
                                info->content_disposition,
                                L"download");
  DCHECK(!file_name.empty());

  // Make sure we get the right file extension.
  std::wstring extension;
  GenerateExtension(file_name, info->mime_type, &extension);
  file_util::ReplaceExtension(&file_name, extension);

  // Prepend "_" to the file name if it's a reserved name
  if (win_util::IsReservedName(file_name))
    file_name = std::wstring(L"_") + file_name;

  generated_name->assign(file_name);
}

void DownloadManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  observer->ModelChanged();
}

void DownloadManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// Post Windows Shell operations to the Download thread, to avoid blocking the
// user interface.
void DownloadManager::ShowDownloadInShell(const DownloadItem* download) {
  DCHECK(file_manager_);
  file_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &DownloadFileManager::OnShowDownloadInShell,
                        download->full_path()));
}

void DownloadManager::OpenDownloadInShell(const DownloadItem* download,
                                          HWND parent_window) {
  DCHECK(file_manager_);
  file_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &DownloadFileManager::OnOpenDownloadInShell,
                        download->full_path(), download->url(), parent_window));
}

void DownloadManager::OpenFilesOfExtension(const std::wstring& extension,
                                           bool open) {
  if (open && !IsExecutable(extension))
    auto_open_.insert(extension);
  else
    auto_open_.erase(extension);
  SaveAutoOpens();
}

bool DownloadManager::ShouldOpenFileExtension(const std::wstring& extension) {
  if (!IsExecutable(extension) &&
      auto_open_.find(extension) != auto_open_.end())
      return true;
  return false;
}

// static
bool DownloadManager::IsExecutableMimeType(const std::string& mime_type) {
  // JavaScript is just as powerful as EXE.
  if (net::MatchesMimeType("text/javascript", mime_type))
    return true;
  if (net::MatchesMimeType("text/javascript;version=*", mime_type))
    return true;

  // We don't consider other non-application types to be executable.
  if (!net::MatchesMimeType("application/*", mime_type))
    return false;

  // These application types are not executable.
  if (net::MatchesMimeType("application/*+xml", mime_type))
    return false;
  if (net::MatchesMimeType("application/xml", mime_type))
    return false;

  return true;
}

bool DownloadManager::IsExecutable(const std::wstring& extension) {
  return exe_types_.find(extension) != exe_types_.end();
}

void DownloadManager::ResetAutoOpenFiles() {
  auto_open_.clear();
  SaveAutoOpens();
}

bool DownloadManager::HasAutoOpenFileTypesRegistered() const {
  return !auto_open_.empty();
}

void DownloadManager::SaveAutoOpens() {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs) {
    std::wstring extensions;
    for (std::set<std::wstring>::iterator it = auto_open_.begin();
         it != auto_open_.end(); ++it) {
      extensions += *it + L":";
    }
    if (!extensions.empty())
      extensions.erase(extensions.size() - 1);
    prefs->SetString(prefs::kDownloadExtensionsToOpen, extensions);
  }
}

void DownloadManager::FileSelected(const std::wstring& path, void* params) {
  DownloadCreateInfo* info = reinterpret_cast<DownloadCreateInfo*>(params);
  if (*prompt_for_download_)
    last_download_path_ = file_util::GetDirectoryFromPath(path);
  ContinueStartDownload(info, path);
}

void DownloadManager::FileSelectionCanceled(void* params) {
  // The user didn't pick a place to save the file, so need to cancel the
  // download that's already in progress to the temporary location.
  DownloadCreateInfo* info = reinterpret_cast<DownloadCreateInfo*>(params);
  file_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(file_manager_, &DownloadFileManager::CancelDownload,
                        info->download_id));
}

// Operations posted to us from the history service ----------------------------

// The history service has retrieved all download entries. 'entries' contains
// 'DownloadCreateInfo's in sorted order (by ascending start_time).
void DownloadManager::OnQueryDownloadEntriesComplete(
    std::vector<DownloadCreateInfo>* entries) {
  for (size_t i = 0; i < entries->size(); ++i) {
    DownloadItem* download = new DownloadItem(entries->at(i));
    DCHECK(downloads_.find(download->db_handle()) == downloads_.end());
    downloads_[download->db_handle()] = download;
    download->set_manager(this);
  }
  FOR_EACH_OBSERVER(Observer, observers_, ModelChanged());
}


// Once the new DownloadItem's creation info has been committed to the history
// service, we associate the DownloadItem with the db handle, update our
// 'downloads_' map and inform observers.
void DownloadManager::OnCreateDownloadEntryComplete(DownloadCreateInfo info,
                                                    int64 db_handle) {
  DownloadMap::iterator it = in_progress_.find(info.download_id);
  DCHECK(it != in_progress_.end());

  DownloadItem* download = it->second;
  DCHECK(download->db_handle() == kUninitializedHandle);
  download->set_db_handle(db_handle);

  // Insert into our full map.
  DCHECK(downloads_.find(download->db_handle()) == downloads_.end());
  downloads_[download->db_handle()] = download;

  // The 'contents' may no longer exist if the user closed the tab before we get
  // this start completion event. If it does, tell the origin WebContents to
  // display its download shelf.
  TabContents* contents =
      tab_util::GetTabContentsByID(info.render_process_id, info.render_view_id);

  // If the contents no longer exists or is no longer active, we start the
  // download in the last active browser. This is not ideal but better than
  // fully hiding the download from the user. Note: non active means that the
  // user navigated away from the tab contents. This has nothing to do with
  // tab selection.
  if (!contents || !contents->is_active()) {
    Browser* last_active = BrowserList::GetLastActive();
    if (last_active)
      contents = last_active->GetSelectedTabContents();
  }

  if (contents)
    contents->OnStartDownload(download);

  // Inform interested objects about the new download.
  FOR_EACH_OBSERVER(Observer, observers_, ModelChanged());
  NotifyAboutDownloadStart();

  // If this download has been completed before we've received the db handle,
  // post one final message to the history service so that it can be properly
  // in sync with the DownloadItem's completion status, and also inform any
  // observers so that they get more than just the start notification.
  if (download->state() != DownloadItem::IN_PROGRESS) {
    in_progress_.erase(it);
    NotifyAboutDownloadStop();
    UpdateHistoryForDownload(download);
    download->UpdateObservers();
  }
}

// Called when the history service has retrieved the list of downloads that
// match the search text.
void DownloadManager::OnSearchComplete(HistoryService::Handle handle,
                                       std::vector<int64>* results) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  Observer* requestor = cancelable_consumer_.GetClientData(hs, handle);
  if (!requestor)
    return;

  std::vector<DownloadItem*> searched_downloads;
  for (std::vector<int64>::iterator it = results->begin();
       it != results->end(); ++it) {
    DownloadMap::iterator dit = downloads_.find(*it);
    if (dit != downloads_.end())
      searched_downloads.push_back(dit->second);
  }

  requestor->SetDownloads(searched_downloads);
}

