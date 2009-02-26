// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/stl_util-inl.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context.h"

#if defined(OS_WIN)
#include "chrome/common/win_util.h"
#include "chrome/common/win_safe_util.h"
#endif

// Throttle updates to the UI thread so that a fast moving download doesn't
// cause it to become unresponsive (in milliseconds).
static const int kUpdatePeriodMs = 500;

// Timer task for posting UI updates. This task is created and maintained by
// the DownloadFileManager long as there is an in progress download. The task
// is cancelled when all active downloads have completed, or in the destructor
// of the DownloadFileManager.
class DownloadFileUpdateTask : public Task {
 public:
  explicit DownloadFileUpdateTask(DownloadFileManager* manager)
      : manager_(manager) {}
  virtual void Run() {
    manager_->UpdateInProgressDownloads();
  }

 private:
  DownloadFileManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileUpdateTask);
};

// DownloadFile implementation -------------------------------------------------

DownloadFile::DownloadFile(const DownloadCreateInfo* info)
    : file_(NULL),
      id_(info->download_id),
      render_process_id_(info->render_process_id),
      render_view_id_(info->render_view_id),
      request_id_(info->request_id),
      bytes_so_far_(0),
      path_renamed_(false),
      in_progress_(true) {
}

DownloadFile::~DownloadFile() {
  Close();
}

bool DownloadFile::Initialize() {
  if (file_util::CreateTemporaryFileName(&full_path_))
    return Open("wb");
  return false;
}

bool DownloadFile::AppendDataToFile(const char* data, int data_len) {
  if (file_) {
    // FIXME bug 595247: handle errors on file writes.
    size_t written = fwrite(data, 1, data_len, file_);
    bytes_so_far_ += written;
    return true;
  }
  return false;
}

void DownloadFile::Cancel() {
  Close();
  file_util::Delete(full_path_, false);
}

// The UI has provided us with our finalized name.
bool DownloadFile::Rename(const FilePath& new_path) {
#if defined(OS_WIN)
  Close();

  // We cannot rename because rename will keep the same security descriptor
  // on the destination file. We want to recreate the security descriptor
  // with the security that makes sense in the new path.
  if (!file_util::RenameFileAndResetSecurityDescriptor(full_path_, new_path)) {
    return false;
  }

  file_util::Delete(full_path_, false);

  full_path_ = new_path;
  path_renamed_ = true;

  // We don't need to re-open the file if we're done (finished or canceled).
  if (!in_progress_)
    return true;

  if (!Open("a+b"))
    return false;
  return true;
#elif defined(OS_POSIX)
  // TODO(port): Port this function to posix (we need file_util::Rename()).
  NOTIMPLEMENTED();
  return false;
#endif
}

void DownloadFile::Close() {
  if (file_) {
    file_util::CloseFile(file_);
    file_ = NULL;
  }
}

bool DownloadFile::Open(const char* open_mode) {
  DCHECK(!full_path_.empty());
  file_ = file_util::OpenFile(full_path_, open_mode);
  if (!file_) {
    return false;
  }

#if defined(OS_WIN)
  // Sets the Zone to tell Windows that this file comes from the internet.
  // We ignore the return value because a failure is not fatal.
  win_util::SetInternetZoneIdentifier(full_path_);
#elif defined(OS_MACOSX)
  // TODO(port) there should be an equivalent on Mac (there isn't on Linux).
  NOTREACHED();
#endif
  return true;
}

// DownloadFileManager implementation ------------------------------------------

DownloadFileManager::DownloadFileManager(MessageLoop* ui_loop,
                                         ResourceDispatcherHost* rdh)
    : next_id_(0),
      ui_loop_(ui_loop),
      resource_dispatcher_host_(rdh) {
}

DownloadFileManager::~DownloadFileManager() {
  // Check for clean shutdown.
  DCHECK(downloads_.empty());
  ui_progress_.clear();
}

void DownloadFileManager::Initialize() {
  io_loop_ = g_browser_process->io_thread()->message_loop();
  file_loop_ = g_browser_process->file_thread()->message_loop();
}

// Called during the browser shutdown process to clean up any state (open files,
// timers) that live on the download_thread_.
void DownloadFileManager::Shutdown() {
  DCHECK(MessageLoop::current() == ui_loop_);
  StopUpdateTimer();
  file_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadFileManager::OnShutdown));
}

// Cease download thread operations.
void DownloadFileManager::OnShutdown() {
  DCHECK(MessageLoop::current() == file_loop_);
  // Delete any partial downloads during shutdown.
  for (DownloadFileMap::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    DownloadFile* download = it->second;
    if (download->in_progress())
      download->Cancel();
    delete download;
  }
  downloads_.clear();
}

// Lookup one in-progress download.
DownloadFile* DownloadFileManager::LookupDownload(int id) {
  DownloadFileMap::iterator it = downloads_.find(id);
  return it == downloads_.end() ? NULL : it->second;
}

// The UI progress is updated on the file thread and removed on the UI thread.
void DownloadFileManager::RemoveDownloadFromUIProgress(int id) {
  DCHECK(MessageLoop::current() == ui_loop_);
  AutoLock lock(progress_lock_);
  if (ui_progress_.find(id) != ui_progress_.end())
    ui_progress_.erase(id);
}

// Throttle updates to the UI thread by only posting update notifications at a
// regularly controlled interval.
void DownloadFileManager::StartUpdateTimer() {
  DCHECK(MessageLoop::current() == ui_loop_);
  if (!update_timer_.IsRunning()) {
    update_timer_.Start(base::TimeDelta::FromMilliseconds(kUpdatePeriodMs),
                        this, &DownloadFileManager::UpdateInProgressDownloads);
  }
}

void DownloadFileManager::StopUpdateTimer() {
  DCHECK(MessageLoop::current() == ui_loop_);
  update_timer_.Stop();
}

// Called on the IO thread once the ResourceDispatcherHost has decided that a
// request is a download.
int DownloadFileManager::GetNextId() {
  DCHECK(MessageLoop::current() == io_loop_);
  return next_id_++;
}

// Notifications sent from the IO thread and run on the download thread:

// The IO thread created 'info', but the download thread (this method) uses it
// to create a DownloadFile, then passes 'info' to the UI thread where it is
// finally consumed and deleted.
void DownloadFileManager::StartDownload(DownloadCreateInfo* info) {
  DCHECK(MessageLoop::current() == file_loop_);
  DCHECK(info);

  DownloadFile* download = new DownloadFile(info);
  if (!download->Initialize()) {
    // Couldn't open, cancel the operation. The UI thread does not yet know
    // about this download so we have to clean up 'info'. We need to get back
    // to the IO thread to cancel the network request and CancelDownloadRequest
    // on the UI thread is the safe way to do that.
    ui_loop_->PostTask(FROM_HERE,
        NewRunnableFunction(&DownloadManager::CancelDownloadRequest,
                            info->render_process_id,
                            info->request_id));
    delete info;
    delete download;
    return;
  }

  DCHECK(LookupDownload(info->download_id) == NULL);
  downloads_[info->download_id] = download;
  info->path = download->full_path();
  {
    AutoLock lock(progress_lock_);
    ui_progress_[info->download_id] = info->received_bytes;
  }

  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadFileManager::OnStartDownload,
                        info));
}

// We don't forward an update to the UI thread here, since we want to throttle
// the UI update rate via a periodic timer. If the user has cancelled the
// download (in the UI thread), we may receive a few more updates before the IO
// thread gets the cancel message: we just delete the data since the
// DownloadFile has been deleted.
void DownloadFileManager::UpdateDownload(int id, DownloadBuffer* buffer) {
  DCHECK(MessageLoop::current() == file_loop_);
  std::vector<DownloadBuffer::Contents> contents;
  {
    AutoLock auto_lock(buffer->lock);
    contents.swap(buffer->contents);
  }

  DownloadFile* download = LookupDownload(id);
  for (size_t i = 0; i < contents.size(); ++i) {
    net::IOBuffer* data = contents[i].first;
    const int data_len = contents[i].second;
    if (download)
      download->AppendDataToFile(data->data(), data_len);
    data->Release();
  }

  if (download) {
    AutoLock lock(progress_lock_);
    ui_progress_[download->id()] = download->bytes_so_far();
  }
}

void DownloadFileManager::DownloadFinished(int id, DownloadBuffer* buffer) {
  DCHECK(MessageLoop::current() == file_loop_);
  delete buffer;
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it != downloads_.end()) {
    DownloadFile* download = it->second;
    download->set_in_progress(false);

    ui_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &DownloadFileManager::OnDownloadFinished,
                          id,
                          download->bytes_so_far()));

    // We need to keep the download around until the UI thread has finalized
    // the name.
    if (download->path_renamed()) {
      downloads_.erase(it);
      delete download;
    }
  }

  if (downloads_.empty())
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &DownloadFileManager::StopUpdateTimer));
}

// This method will be sent via a user action, or shutdown on the UI thread, and
// run on the download thread. Since this message has been sent from the UI
// thread, the download may have already completed and won't exist in our map.
void DownloadFileManager::CancelDownload(int id) {
  DCHECK(MessageLoop::current() == file_loop_);
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it != downloads_.end()) {
    DownloadFile* download = it->second;
    download->set_in_progress(false);

    download->Cancel();

    ui_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &DownloadFileManager::RemoveDownloadFromUIProgress,
                          download->id()));

    if (download->path_renamed()) {
      downloads_.erase(it);
      delete download;
    }
  }

  if (downloads_.empty())
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &DownloadFileManager::StopUpdateTimer));
}

// Our periodic timer has fired so send the UI thread updates on all in progress
// downloads.
void DownloadFileManager::UpdateInProgressDownloads() {
  DCHECK(MessageLoop::current() == ui_loop_);
  AutoLock lock(progress_lock_);
  ProgressMap::iterator it = ui_progress_.begin();
  for (; it != ui_progress_.end(); ++it) {
    const int id = it->first;
    DownloadManager* manager = LookupManager(id);
    if (manager)
      manager->UpdateDownload(id, it->second);
  }
}

// Notifications sent from the download thread and run on the UI thread.

// Lookup the DownloadManager for this WebContents' profile and inform it of
// a new download.
// TODO(paulg): When implementing download restart via the Downloads tab,
//              there will be no 'render_process_id' or 'render_view_id'.
void DownloadFileManager::OnStartDownload(DownloadCreateInfo* info) {
  DCHECK(MessageLoop::current() == ui_loop_);
  DownloadManager* manager =
      DownloadManagerFromRenderIds(info->render_process_id,
                                   info->render_view_id);
  if (!manager) {
    DownloadManager::CancelDownloadRequest(info->render_process_id,
                                           info->request_id);
    delete info;
    return;
  }

  StartUpdateTimer();

  // Add the download manager to our request maps for future updates. We want to
  // be able to cancel all in progress downloads when a DownloadManager is
  // deleted, such as when a profile is closed. We also want to be able to look
  // up the DownloadManager associated with a given request without having to
  // rely on using tab information, since a tab may be closed while a download
  // initiated from that tab is still in progress.
  DownloadRequests& downloads = requests_[manager];
  downloads.insert(info->download_id);

  // TODO(paulg): The manager will exist when restarts are implemented.
  DownloadManagerMap::iterator dit = managers_.find(info->download_id);
  if (dit == managers_.end())
    managers_[info->download_id] = manager;
  else
    NOTREACHED();

  // StartDownload will clean up |info|.
  manager->StartDownload(info);
}

// Update the Download Manager with the finish state, and remove the request
// tracking entries.
void DownloadFileManager::OnDownloadFinished(int id,
                                             int64 bytes_so_far) {
  DCHECK(MessageLoop::current() == ui_loop_);
  DownloadManager* manager = LookupManager(id);
  if (manager)
    manager->DownloadFinished(id, bytes_so_far);
  RemoveDownload(id, manager);
  RemoveDownloadFromUIProgress(id);
}

void DownloadFileManager::DownloadUrl(const GURL& url,
                                      const GURL& referrer,
                                      int render_process_host_id,
                                      int render_view_id,
                                      URLRequestContext* request_context) {
  DCHECK(MessageLoop::current() == ui_loop_);
  base::Thread* thread = g_browser_process->io_thread();
  if (thread) {
    thread->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &DownloadFileManager::OnDownloadUrl,
                          url,
                          referrer,
                          render_process_host_id,
                          render_view_id,
                          request_context));
  }
}

// Relate a download ID to its owning DownloadManager.
DownloadManager* DownloadFileManager::LookupManager(int download_id) {
  DCHECK(MessageLoop::current() == ui_loop_);
  DownloadManagerMap::iterator it = managers_.find(download_id);
  if (it != managers_.end())
    return it->second;
  return NULL;
}

// Utility function for look up table maintenance, called on the UI thread.
// A manager may have multiple downloads in progress, so we just look up the
// one download (id) and remove it from the set, and remove the set if it
// becomes empty.
void DownloadFileManager::RemoveDownload(int id, DownloadManager* manager) {
  DCHECK(MessageLoop::current() == ui_loop_);
  if (manager) {
    RequestMap::iterator it = requests_.find(manager);
    if (it != requests_.end()) {
      DownloadRequests& downloads = it->second;
      DownloadRequests::iterator rit = downloads.find(id);
      if (rit != downloads.end())
        downloads.erase(rit);
      if (downloads.empty())
        requests_.erase(it);
    }
  }

  // A download can only have one manager, so remove it if it exists.
  DownloadManagerMap::iterator dit = managers_.find(id);
  if (dit != managers_.end())
    managers_.erase(dit);
}

// Utility function for converting request IDs to a TabContents. Must be called
// only on the UI thread since Profile operations may create UI objects, such as
// the first call to profile->GetDownloadManager().
// static
DownloadManager* DownloadFileManager::DownloadManagerFromRenderIds(
    int render_process_id, int render_view_id) {
  WebContents* contents = tab_util::GetWebContentsByID(render_process_id,
                                                       render_view_id);
  if (contents) {
    Profile* profile = contents->profile();
    if (profile)
      return profile->GetDownloadManager();
  }

  return NULL;
}

// Called by DownloadManagers in their destructor, and only on the UI thread.
void DownloadFileManager::RemoveDownloadManager(DownloadManager* manager) {
  DCHECK(MessageLoop::current() == ui_loop_);
  DCHECK(manager);
  RequestMap::iterator it = requests_.find(manager);
  if (it == requests_.end())
    return;

  const DownloadRequests& requests = it->second;
  DownloadRequests::const_iterator i = requests.begin();
  for (; i != requests.end(); ++i) {
    DownloadManagerMap::iterator dit = managers_.find(*i);
    if (dit != managers_.end()) {
      DCHECK(dit->second == manager);
      managers_.erase(dit);
    }
  }

  requests_.erase(it);
}


// Notifications from the UI thread and run on the IO thread

// Initiate a request for URL to be downloaded.
void DownloadFileManager::OnDownloadUrl(const GURL& url,
                                        const GURL& referrer,
                                        int render_process_host_id,
                                        int render_view_id,
                                        URLRequestContext* request_context) {
  DCHECK(MessageLoop::current() == io_loop_);
  resource_dispatcher_host_->BeginDownload(url,
                                           referrer,
                                           render_process_host_id,
                                           render_view_id,
                                           request_context);
}

// Actions from the UI thread and run on the download thread

// Open a download, or show it in a Windows Explorer window. We run on this
// thread to avoid blocking the UI with (potentially) slow Shell operations.
// TODO(paulg): File 'stat' operations.
void DownloadFileManager::OnShowDownloadInShell(const FilePath& full_path) {
#if defined(OS_WIN)
  DCHECK(MessageLoop::current() == file_loop_);
  win_util::ShowItemInFolder(full_path.value());
#else
  // TODO(port) implement me.
  NOTREACHED();
#endif
}

// Launches the selected download using ShellExecute 'open' verb. If there is
// a valid parent window, the 'safer' version will be used which can
// display a modal dialog asking for user consent on dangerous files.
void DownloadFileManager::OnOpenDownloadInShell(const FilePath& full_path,
                                                const GURL& url,
                                                gfx::NativeView parent_window) {
#if defined(OS_WIN)
  DCHECK(MessageLoop::current() == file_loop_);
  if (NULL != parent_window) {
    win_util::SaferOpenItemViaShell(parent_window, L"", full_path,
                                    UTF8ToWide(url.spec()), true);
  } else {
    win_util::OpenItemViaShell(full_path, true);
  }
#else
  // TODO(port) implement me.
  NOTREACHED();
#endif
}

// The DownloadManager in the UI thread has provided a final name for the
// download specified by 'id'. Rename the in progress download, and remove it
// from our table if it has been completed or cancelled already.
void DownloadFileManager::OnFinalDownloadName(int id,
                                              const FilePath& full_path) {
  DCHECK(MessageLoop::current() == file_loop_);
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it == downloads_.end())
    return;

  file_util::CreateDirectory(full_path.DirName());

  DownloadFile* download = it->second;
  if (!download->Rename(full_path)) {
    // Error. Between the time the UI thread generated 'full_path' to the time
    // this code runs, something happened that prevents us from renaming.
    DownloadManagerMap::iterator dmit = managers_.find(download->id());
    if (dmit != managers_.end()) {
      DownloadManager* dlm = dmit->second;
      ui_loop_->PostTask(FROM_HERE,
          NewRunnableMethod(dlm,
                            &DownloadManager::DownloadCancelled,
                            id));
    } else {
      ui_loop_->PostTask(FROM_HERE,
          NewRunnableFunction(&DownloadManager::CancelDownloadRequest,
                              download->render_process_id(),
                              download->request_id()));
    }
  }

  // If the download has completed before we got this final name, we remove it
  // from our in progress map.
  if (!download->in_progress()) {
    downloads_.erase(it);
    delete download;
  }

  if (downloads_.empty())
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &DownloadFileManager::StopUpdateTimer));
}

// static
void DownloadFileManager::DeleteFile(const FilePath& path) {
  // Make sure we only delete files.
  if (!file_util::DirectoryExists(path))
    file_util::Delete(path, false);
}
