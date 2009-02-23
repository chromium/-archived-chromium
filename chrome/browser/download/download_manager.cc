// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_manager.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/timer.h"
#include "base/rand_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_file.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/stl_util-inl.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context.h"

#if defined(OS_WIN)
// TODO(port): some of these need porting.
#include "base/registry.h"
#include "base/win_util.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/common/win_util.h"
#endif

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
// database handles in incognito mode starting at -1 and progressively getting
// more negative.
static const int kUninitializedHandle = 0;

// Appends the passed the number between parenthesis the path before the
// extension.
static void AppendNumberToPath(FilePath* path, int number) {
  file_util::InsertBeforeExtension(path,
      StringPrintf(FILE_PATH_LITERAL(" (%d)"), number));
}

// Attempts to find a number that can be appended to that path to make it
// unique. If |path| does not exist, 0 is returned.  If it fails to find such
// a number, -1 is returned.
static int GetUniquePathNumber(const FilePath& path) {
  const int kMaxAttempts = 100;

  if (!file_util::PathExists(path))
    return 0;

  FilePath new_path;
  for (int count = 1; count <= kMaxAttempts; ++count) {
    new_path = FilePath(path);
    AppendNumberToPath(&new_path, count);

    if (!file_util::PathExists(new_path))
      return count;
  }

  return -1;
}

#if defined(OS_WIN)
static bool DownloadPathIsDangerous(const FilePath& download_path) {
  FilePath desktop_dir;
  if (!PathService::Get(chrome::DIR_USER_DESKTOP, &desktop_dir)) {
    NOTREACHED();
    return false;
  }
  return (download_path == desktop_dir);
}
#endif

// DownloadItem implementation -------------------------------------------------

// Constructor for reading from the history service.
DownloadItem::DownloadItem(const DownloadCreateInfo& info)
    : id_(-1),
      full_path_(info.path),
      url_(info.url),
      total_bytes_(info.total_bytes),
      received_bytes_(info.received_bytes),
      start_tick_(base::TimeTicks()),
      state_(static_cast<DownloadState>(info.state)),
      start_time_(info.start_time),
      db_handle_(info.db_handle),
      manager_(NULL),
      is_paused_(false),
      open_when_complete_(false),
      safety_state_(SAFE),
      original_name_(info.original_name),
      render_process_id_(-1),
      request_id_(-1) {
  if (state_ == IN_PROGRESS)
    state_ = CANCELLED;
  Init(false /* don't start progress timer */);
}

// Constructor for DownloadItem created via user action in the main thread.
DownloadItem::DownloadItem(int32 download_id,
                           const FilePath& path,
                           int path_uniquifier,
                           const GURL& url,
                           const FilePath& original_name,
                           const base::Time start_time,
                           int64 download_size,
                           int render_process_id,
                           int request_id,
                           bool is_dangerous)
    : id_(download_id),
      full_path_(path),
      path_uniquifier_(path_uniquifier),
      url_(url),
      total_bytes_(download_size),
      received_bytes_(0),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS),
      start_time_(start_time),
      db_handle_(kUninitializedHandle),
      manager_(NULL),
      is_paused_(false),
      open_when_complete_(false),
      safety_state_(is_dangerous ? DANGEROUS : SAFE),
      original_name_(original_name),
      render_process_id_(render_process_id),
      request_id_(request_id) {
  Init(true /* start progress timer */);
}

void DownloadItem::Init(bool start_timer) {
  file_name_ = full_path_.BaseName();
  if (start_timer)
    StartProgressTimer();
}

DownloadItem::~DownloadItem() {
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

// Triggered by a user action.
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

void DownloadItem::Remove(bool delete_on_disk) {
  Cancel(true);
  state_ = REMOVING;
  if (delete_on_disk)
    manager_->DeleteDownload(full_path_);
  manager_->RemoveDownload(db_handle_);
  // We have now been deleted.
}

void DownloadItem::StartProgressTimer() {
  update_timer_.Start(base::TimeDelta::FromMilliseconds(kUpdateTimeMs), this,
                      &DownloadItem::UpdateObservers);
}

void DownloadItem::StopProgressTimer() {
  update_timer_.Stop();
}

bool DownloadItem::TimeRemaining(base::TimeDelta* remaining) const {
  if (total_bytes_ <= 0)
    return false;  // We never received the content_length for this download.

  int64 speed = CurrentSpeed();
  if (speed == 0)
    return false;

  *remaining =
      base::TimeDelta::FromSeconds((total_bytes_ - received_bytes_) / speed);
  return true;
}

int64 DownloadItem::CurrentSpeed() const {
  base::TimeDelta diff = base::TimeTicks::Now() - start_tick_;
  int64 diff_ms = diff.InMilliseconds();
  return diff_ms == 0 ? 0 : received_bytes_ * 1000 / diff_ms;
}

int DownloadItem::PercentComplete() const {
  int percent = -1;
  if (total_bytes_ > 0)
    percent = static_cast<int>(received_bytes_ * 100.0 / total_bytes_);
  return percent;
}

void DownloadItem::Rename(const FilePath& full_path) {
  DCHECK(!full_path.empty());
  full_path_ = full_path;
  file_name_ = full_path_.BaseName();
}

void DownloadItem::TogglePause() {
  DCHECK(state_ == IN_PROGRESS);
  manager_->PauseDownload(id_, !is_paused_);
  is_paused_ = !is_paused_;
  UpdateObservers();
}

FilePath DownloadItem::GetFileName() const {
  if (safety_state_ == DownloadItem::SAFE)
    return file_name_;
  if (path_uniquifier_ > 0) {
    FilePath name(original_name_);
    AppendNumberToPath(&name, path_uniquifier_);
    return name;
  }
  return original_name_;
}

// DownloadManager implementation ----------------------------------------------

// static
void DownloadManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kPromptForDownload, false);
  prefs->RegisterStringPref(prefs::kDownloadExtensionsToOpen, L"");
  prefs->RegisterBooleanPref(prefs::kDownloadDirUpgraded, false);

// TODO(port): port the necessary bits of chrome_paths.
#if defined(OS_WIN)
  // The default download path is userprofile\download.
  FilePath default_download_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                        &default_download_path)) {
    NOTREACHED();
  }
  prefs->RegisterStringPref(prefs::kDownloadDefaultDirectory,
                            default_download_path.ToWStringHack());

  // If the download path is dangerous we forcefully reset it. But if we do
  // so we set a flag to make sure we only do it once, to avoid fighting
  // the user if he really wants it on an unsafe place such as the desktop.

  if (!prefs->GetBoolean(prefs::kDownloadDirUpgraded)) {
    FilePath current_download_dir = FilePath::FromWStringHack(
        prefs->GetString(prefs::kDownloadDefaultDirectory));
    if (DownloadPathIsDangerous(current_download_dir)) {
      prefs->SetString(prefs::kDownloadDefaultDirectory,
                       default_download_path.ToWStringHack());
    }
    prefs->SetBoolean(prefs::kDownloadDirUpgraded, true);
  }
#endif
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
  std::set<DownloadItem*> to_remove;
  for (; it != in_progress_.end(); ++it) {
    DownloadItem* download = it->second;
    if (download->safety_state() == DownloadItem::DANGEROUS) {
      // Forget about any download that the user did not approve.
      // Note that we cannot call download->Remove() this would invalidate our
      // iterator.
      to_remove.insert(download);
      continue;
    }
    DCHECK_EQ(DownloadItem::IN_PROGRESS, download->state());
    download->Cancel(false);
    UpdateHistoryForDownload(download);
    if (download->db_handle() == kUninitializedHandle) {
      // An invalid handle means that 'download' does not yet exist in
      // 'downloads_', so we have to delete it here.
      delete download;
    }
  }

  // 'dangerous_finished_' contains all complete downloads that have not been
  // approved.  They should be removed.
  it = dangerous_finished_.begin();
  for (; it != dangerous_finished_.end(); ++it)
    to_remove.insert(it->second);

  // Remove the dangerous download that are not approved.
  for (std::set<DownloadItem*>::const_iterator rm_it = to_remove.begin();
       rm_it != to_remove.end(); ++rm_it) {
    DownloadItem* download = *rm_it;
    int64 handle = download->db_handle();
    download->Remove(true);
    // Same as above, delete the download if it is not in 'downloads_' (as the
    // Remove() call above won't have deleted it).
    if (handle == kUninitializedHandle)
      delete download;
  }
  to_remove.clear();

  in_progress_.clear();
  dangerous_finished_.clear();
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

// TODO(port): enable this after implementing chrome_paths for posix.
#if defined(OS_WIN)
  download_path_.Init(prefs::kDownloadDefaultDirectory, prefs, NULL);

  // This variable is needed to resolve which CreateDirectory we want to point
  // to. Without it, the NewRunnableFunction cannot resolve the ambiguity.
  // TODO(estade): when file_util::CreateDirectory(wstring) is removed,
  // get rid of |CreateDirectoryPtr|.
  bool (*CreateDirectoryPtr)(const FilePath&) = &file_util::CreateDirectory;
  // Ensure that the download directory specified in the preferences exists.
  file_loop_->PostTask(FROM_HERE, NewRunnableFunction(
      CreateDirectoryPtr, download_path()));

  // We store any file extension that should be opened automatically at
  // download completion in this pref.
  download_util::InitializeExeTypes(&exe_types_);
#elif defined(OS_POSIX)
  NOTIMPLEMENTED();
#endif

  std::wstring extensions_to_open =
      prefs->GetString(prefs::kDownloadExtensionsToOpen);
  std::vector<std::wstring> extensions;
  SplitString(extensions_to_open, L':', &extensions);
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (!extensions[i].empty() && !IsExecutable(
        FilePath::FromWStringHack(extensions[i]).value()))
      auto_open_.insert(FilePath::FromWStringHack(extensions[i]).value());
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

  // Freeze the user's preference for showing a Save As dialog.  We're going to
  // bounce around a bunch of threads and we don't want to worry about race
  // conditions where the user changes this pref out from under us.
  if (*prompt_for_download_)
    info->save_as = true;

  // Determine the proper path for a download, by choosing either the default
  // download directory, or prompting the user.
  FilePath generated_name;
  GenerateFilename(info, &generated_name);
  if (info->save_as && !last_download_path_.empty())
    info->suggested_path = last_download_path_;
  else
    info->suggested_path = download_path();
  info->suggested_path = info->suggested_path.Append(generated_name);

  if (!info->save_as) {
    // Let's check if this download is dangerous, based on its name.
    info->is_dangerous = IsDangerous(info->suggested_path.BaseName());
  }

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
  FilePath dir = info->suggested_path.DirName();
  FilePath filename = info->suggested_path.BaseName();
  if (!file_util::PathIsWritable(dir)) {
    info->save_as = true;
    PathService::Get(chrome::DIR_USER_DOCUMENTS, &info->suggested_path);
    info->suggested_path = info->suggested_path.Append(filename);
  }

  info->path_uniquifier = GetUniquePathNumber(info->suggested_path);

  // If the download is deemed dangerous, we'll use a temporary name for it.
  if (info->is_dangerous) {
    info->original_name = FilePath(info->suggested_path).BaseName();
    // Create a temporary file to hold the file until the user approves its
    // download.
    FilePath::StringType file_name;
    FilePath path;
    while (path.empty()) {
      SStringPrintf(&file_name, FILE_PATH_LITERAL("unconfirmed %d.download"),
                    base::RandInt(0, 100000));
      path = dir.Append(file_name);
      if (file_util::PathExists(path))
        path = FilePath();
    }
    info->suggested_path = path;
  } else {
    // We know the final path, build it if necessary.
    if (info->path_uniquifier > 0) {
      AppendNumberToPath(&(info->suggested_path), info->path_uniquifier);
      // Setting path_uniquifier to 0 to make sure we don't try to unique it
      // later on.
      info->path_uniquifier = 0;
    } else if (info->path_uniquifier == -1) {
      // We failed to find a unique path.  We have to prompt the user.
      info->save_as = true;
    }
  }

  if (!info->save_as) {
    // Create an empty file at the suggested path so that we don't allocate the
    // same "non-existant" path to multiple downloads.
    // See: http://code.google.com/p/chromium/issues/detail?id=3662
    file_util::WriteFile(info->suggested_path.ToWStringHack(), "", 0);
  }

  // Now we return to the UI thread.
  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadManager::OnPathExistenceAvailable,
                        info));
}

void DownloadManager::OnPathExistenceAvailable(DownloadCreateInfo* info) {
#if defined(OS_WIN)
  DCHECK(MessageLoop::current() == ui_loop_);
  DCHECK(info);

  if (info->save_as) {
    // We must ask the user for the place to put the download.
    if (!select_file_dialog_.get())
      select_file_dialog_ = SelectFileDialog::Create(this);

    WebContents* contents = tab_util::GetWebContentsByID(
        info->render_process_id, info->render_view_id);
    std::wstring filter =
        win_util::GetFileFilterFromPath(info->suggested_path.value());
    HWND owning_hwnd =
        contents ? GetAncestor(contents->GetNativeView(), GA_ROOT) : NULL;
    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
                                    std::wstring(),
                                    info->suggested_path.ToWStringHack(),
                                    filter, std::wstring(),
                                    owning_hwnd, info);
  } else {
    // No prompting for download, just continue with the suggested name.
    ContinueStartDownload(info, info->suggested_path);
  }
#elif defined(OS_POSIX)
  // TODO(port): port this file -- need dialogs.
  NOTIMPLEMENTED();
#endif
}

void DownloadManager::ContinueStartDownload(DownloadCreateInfo* info,
                                            const FilePath& target_path) {
  scoped_ptr<DownloadCreateInfo> infop(info);
  info->path = target_path;

  DownloadItem* download = NULL;
  DownloadMap::iterator it = in_progress_.find(info->download_id);
  if (it == in_progress_.end()) {
    download = new DownloadItem(info->download_id,
                                info->path,
                                info->path_uniquifier,
                                info->url,
                                info->original_name,
                                info->start_time,
                                info->total_bytes,
                                info->render_process_id,
                                info->request_id,
                                info->is_dangerous);
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
    // FIXME(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
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

  // FIXME(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->UpdateDownload(download->received_bytes(),
                       download->state(),
                       download->db_handle());
  }
}

void DownloadManager::RemoveDownloadFromHistory(DownloadItem* download) {
  DCHECK(download);
  // FIXME(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (download->db_handle() > kUninitializedHandle && hs)
    hs->RemoveDownload(download->db_handle());
}

void DownloadManager::RemoveDownloadsFromHistoryBetween(
    const base::Time remove_begin,
    const base::Time remove_end) {
  // FIXME(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
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
  if (it == in_progress_.end()) {
    // The download is done, but the user hasn't selected a final location for
    // it yet (the Save As dialog box is probably still showing), so just keep
    // track of the fact that this download id is complete, when the
    // DownloadItem is constructed later we'll notify its completion then.
    PendingFinishedMap::iterator erase_it =
        pending_finished_downloads_.find(download_id);
    DCHECK(erase_it == pending_finished_downloads_.end());
    pending_finished_downloads_[download_id] = size;
    return;
  }

  // Remove the id from the list of pending ids.
  PendingFinishedMap::iterator erase_it =
      pending_finished_downloads_.find(download_id);
  if (erase_it != pending_finished_downloads_.end())
    pending_finished_downloads_.erase(erase_it);

  DownloadItem* download = it->second;
  download->Finished(size);

  // Clean up will happen when the history system create callback runs if we
  // don't have a valid db_handle yet.
  if (download->db_handle() != kUninitializedHandle) {
    in_progress_.erase(it);
    NotifyAboutDownloadStop();
    UpdateHistoryForDownload(download);
  }

  // If this a dangerous download not yet validated by the user, don't do
  // anything. When the user notifies us, it will trigger a call to
  // ProceedWithFinishedDangerousDownload.
  if (download->safety_state() == DownloadItem::DANGEROUS) {
    dangerous_finished_[download_id] = download;
    return;
  }

  if (download->safety_state() == DownloadItem::DANGEROUS_BUT_VALIDATED) {
    // We first need to rename the downloaded file from its temporary name to
    // its final name before we can continue.
    file_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(
            this, &DownloadManager::ProceedWithFinishedDangerousDownload,
            download->db_handle(),
            download->full_path(), download->original_name()));
    return;
  }
  ContinueDownloadFinished(download);
}

void DownloadManager::ContinueDownloadFinished(DownloadItem* download) {
  // If this was a dangerous download, it has now been approved and must be
  // removed from dangerous_finished_ so it does not get deleted on shutdown.
  DownloadMap::iterator it = dangerous_finished_.find(download->id());
  if (it != dangerous_finished_.end())
    dangerous_finished_.erase(it);

  // Notify our observers that we are complete (the call to Finished() set the
  // state to complete but did not notify).
  download->UpdateObservers();

  // Open the download if the user or user prefs indicate it should be.
  FilePath::StringType extension = download->full_path().Extension();
  // Drop the leading period.
  if (extension.size() > 0)
    extension = extension.substr(1);
  if (download->open_when_complete() || ShouldOpenFileExtension(extension))
    OpenDownloadInShell(download, NULL);
}

// Called on the file thread.  Renames the downloaded file to its original name.
void DownloadManager::ProceedWithFinishedDangerousDownload(
    int64 download_handle,
    const FilePath& path,
    const FilePath& original_name) {
  bool success = false;
  FilePath new_path;
  int uniquifier = 0;
  if (file_util::PathExists(path)) {
    new_path = path.DirName().Append(original_name);
    // Make our name unique at this point, as if a dangerous file is downloading
    // and a 2nd download is started for a file with the same name, they would
    // have the same path.  This is because we uniquify the name on download
    // start, and at that time the first file does not exists yet, so the second
    // file gets the same name.
    uniquifier = GetUniquePathNumber(new_path);
    if (uniquifier > 0)
      AppendNumberToPath(&new_path, uniquifier);
    success = file_util::Move(path, new_path);
  } else {
    NOTREACHED();
  }

  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &DownloadManager::DangerousDownloadRenamed,
                        download_handle, success, new_path, uniquifier));
}

// Call from the file thread when the finished dangerous download was renamed.
void DownloadManager::DangerousDownloadRenamed(int64 download_handle,
                                               bool success,
                                               const FilePath& new_path,
                                               int new_path_uniquifier) {
  DownloadMap::iterator it = downloads_.find(download_handle);
  if (it == downloads_.end()) {
    NOTREACHED();
    return;
  }

  DownloadItem* download = it->second;
  // If we failed to rename the file, we'll just keep the name as is.
  if (success) {
    // We need to update the path uniquifier so that the UI shows the right
    // name when calling GetFileName().
    download->set_path_uniquifier(new_path_uniquifier);
    RenameDownload(download, new_path);
  }

  // Continue the download finished sequence.
  ContinueDownloadFinished(download);
}

// static
// We have to tell the ResourceDispatcherHost to cancel the download from this
// thread, since we can't forward tasks from the file thread to the IO thread
// reliably (crash on shutdown race condition).
void DownloadManager::CancelDownloadRequest(int render_process_id,
                                            int request_id) {
  ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
  base::Thread* io_thread = g_browser_process->io_thread();
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
    base::Thread* io_thread = g_browser_process->io_thread();
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

bool DownloadManager::IsDangerous(const FilePath& file_name) {
  // TODO(jcampan): Improve me.
  FilePath::StringType extension = file_name.Extension();
  // Drop the leading period.
  if (extension.size() > 0)
    extension = extension.substr(1);
  return IsExecutable(extension);
}

void DownloadManager::RenameDownload(DownloadItem* download,
                                     const FilePath& new_path) {
  download->Rename(new_path);

  // Update the history.

  // No update necessary if the download was initiated while in incognito mode.
  if (download->db_handle() <= kUninitializedHandle)
    return;

  // FIXME(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->UpdateDownloadPath(new_path.ToWStringHack(), download->db_handle());
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
  it = dangerous_finished_.find(download->id());
  if (it != dangerous_finished_.end())
    dangerous_finished_.erase(it);

  // Tell observers to refresh their views.
  FOR_EACH_OBSERVER(Observer, observers_, ModelChanged());

  delete download;
}

int DownloadManager::RemoveDownloadsBetween(const base::Time remove_begin,
                                            const base::Time remove_end) {
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
      downloads_.erase(it++);

      // Also remove it from any completed dangerous downloads.
      DownloadMap::iterator dit = dangerous_finished_.find(download->id());
      if (dit != dangerous_finished_.end())
        dangerous_finished_.erase(dit);

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

int DownloadManager::RemoveDownloads(const base::Time remove_begin) {
  return RemoveDownloadsBetween(remove_begin, base::Time());
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
  NotificationService::current()->Notify(
      NotificationType::DOWNLOAD_START,
      NotificationService::AllSources(),
      NotificationService::NoDetails());
}

void DownloadManager::NotifyAboutDownloadStop() {
  NotificationService::current()->Notify(
      NotificationType::DOWNLOAD_STOP,
      NotificationService::AllSources(),
      NotificationService::NoDetails());
}

void DownloadManager::GenerateExtension(
    const FilePath& file_name,
    const std::string& mime_type,
    FilePath::StringType* generated_extension) {
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

  static const FilePath::CharType default_extension[] =
      FILE_PATH_LITERAL("download");

  // See if our file name already contains an extension.
  FilePath::StringType extension(
      file_util::GetFileExtensionFromPath(file_name));

#if defined(OS_WIN)
  // Rename shell-integrated extensions.
  if (win_util::IsShellIntegratedExtension(extension))
    extension.assign(default_extension);
#endif

  std::string mime_type_from_extension;
  net::GetMimeTypeFromFile(file_name,
                           &mime_type_from_extension);
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
    // Append extension generated from the mime type if:
    // 1. New extension is not ".txt"
    // 2. New extension is not the same as the already existing extension.
    // 3. New extension is not executable. This action mitigates the case when
    //    an executable is hidden in a benign file extension;
    //    E.g. my-cat.jpg becomes my-cat.jpg.js if content type is
    //         application/x-javascript.
    FilePath::StringType append_extension;
    if (net::GetPreferredExtensionForMimeType(mime_type, &append_extension)) {
      if (append_extension != FILE_PATH_LITERAL("txt") &&
          append_extension != extension &&
          !IsExecutable(append_extension)) {
        extension += FILE_PATH_LITERAL(".");
        extension += append_extension;
      }
    }
  }

  generated_extension->swap(extension);
}

void DownloadManager::GenerateFilename(DownloadCreateInfo* info,
                                       FilePath* generated_name) {
  *generated_name = FilePath::FromWStringHack(
      net::GetSuggestedFilename(GURL(info->url),
                                info->content_disposition,
                                L"download"));
  DCHECK(!generated_name->empty());

  GenerateSafeFilename(info->mime_type, generated_name);
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
                        FilePath(download->full_path())));
}

void DownloadManager::OpenDownloadInShell(const DownloadItem* download,
                                          gfx::NativeView parent_window) {
  DCHECK(file_manager_);
  file_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &DownloadFileManager::OnOpenDownloadInShell,
                        download->full_path(), download->url(), parent_window));
}

void DownloadManager::OpenFilesOfExtension(
    const FilePath::StringType& extension, bool open) {
  if (open && !IsExecutable(extension))
    auto_open_.insert(extension);
  else
    auto_open_.erase(extension);
  SaveAutoOpens();
}

bool DownloadManager::ShouldOpenFileExtension(
    const FilePath::StringType& extension) {
  // Special-case Chrome extensions as always-open.
  if (!IsExecutable(extension) &&
      (auto_open_.find(extension) != auto_open_.end() ||
       extension == chrome::kExtensionFileExtension))
    return true;
  return false;
}

static const char* kExecutableWhiteList[] = {
  // JavaScript is just as powerful as EXE.
  "text/javascript",
  "text/javascript;version=*",
  // Some sites use binary/octet-stream to mean application/octet-stream.
  // See http://code.google.com/p/chromium/issues/detail?id=1573
  "binary/octet-stream"
};

static const char* kExecutableBlackList[] = {
  // These application types are not executable.
  "application/*+xml",
  "application/xml"
};

// static
bool DownloadManager::IsExecutableMimeType(const std::string& mime_type) {
  for (size_t i = 0; i < arraysize(kExecutableWhiteList); ++i) {
    if (net::MatchesMimeType(kExecutableWhiteList[i], mime_type))
      return true;
  }
  for (size_t i = 0; i < arraysize(kExecutableBlackList); ++i) {
    if (net::MatchesMimeType(kExecutableBlackList[i], mime_type))
      return false;
  }
  // We consider only other application types to be executable.
  return net::MatchesMimeType("application/*", mime_type);
}

bool DownloadManager::IsExecutable(const FilePath::StringType& extension) {
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
    FilePath::StringType extensions;
    for (std::set<FilePath::StringType>::iterator it = auto_open_.begin();
         it != auto_open_.end(); ++it) {
      extensions += *it + FILE_PATH_LITERAL(":");
    }
    if (!extensions.empty())
      extensions.erase(extensions.size() - 1);

    std::wstring extensions_w;
#if defined(OS_WIN)
    extensions_w = extensions;
#elif defined(OS_POSIX)
    extensions_w = SysNativeMBToWide(extensions);
#endif

    prefs->SetString(prefs::kDownloadExtensionsToOpen, extensions_w);
  }
}

void DownloadManager::FileSelected(const std::wstring& path_string,
                                   void* params) {
  FilePath path = FilePath::FromWStringHack(path_string);
  DownloadCreateInfo* info = reinterpret_cast<DownloadCreateInfo*>(params);
  if (info->save_as)
    last_download_path_ = path.DirName();
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

void DownloadManager::DeleteDownload(const FilePath& path) {
  file_loop_->PostTask(FROM_HERE, NewRunnableFunction(
      &DownloadFileManager::DeleteFile, FilePath(path)));
}


void DownloadManager::DangerousDownloadValidated(DownloadItem* download) {
  DCHECK_EQ(DownloadItem::DANGEROUS, download->safety_state());
  download->set_safety_state(DownloadItem::DANGEROUS_BUT_VALIDATED);
  download->UpdateObservers();

  // If the download is not complete, nothing to do.  The required
  // post-processing will be performed when it does complete.
  if (download->state() != DownloadItem::COMPLETE)
    return;

  file_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadManager::ProceedWithFinishedDangerousDownload,
                        download->db_handle(), download->full_path(),
                        download->original_name()));
}

void DownloadManager::GenerateSafeFilename(const std::string& mime_type,
                                           FilePath* file_name) {
  // Make sure we get the right file extension
  FilePath::StringType extension;
  GenerateExtension(*file_name, mime_type, &extension);
  file_util::ReplaceExtension(file_name, extension);

  // Prepend "_" to the file name if it's a reserved name
  FilePath::StringType leaf_name = file_name->BaseName().value();
  DCHECK(!leaf_name.empty());
#if defined(OS_WIN)
  if (win_util::IsReservedName(leaf_name)) {
    leaf_name = FilePath::StringType(FILE_PATH_LITERAL("_")) + leaf_name;
    *file_name = file_name->DirName();
    if (file_name->value() == FilePath::kCurrentDirectory) {
      *file_name = FilePath(leaf_name);
    } else {
      *file_name = file_name->Append(leaf_name);
    }
  }
#elif defined(OS_POSIX)
  NOTIMPLEMENTED();
#endif
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
      tab_util::GetWebContentsByID(info.render_process_id, info.render_view_id);

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

// Clears the last download path, used to initialize "save as" dialogs.
void DownloadManager::ClearLastDownloadPath() {
  last_download_path_ = FilePath();
}
