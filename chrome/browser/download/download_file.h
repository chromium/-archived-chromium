// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Objects that handle file operations for downloads, on the download thread.
//
// The DownloadFileManager owns a set of DownloadFile objects, each of which
// represent one in progress download and performs the disk IO for that
// download. The DownloadFileManager itself is a singleton object owned by the
// ResourceDispatcherHost.
//
// The DownloadFileManager uses the file_thread for performing file write
// operations, in order to avoid disk activity on either the IO (network) thread
// and the UI thread. It coordinates the notifications from the network and UI.
//
// A typical download operation involves multiple threads:
//
// Updating an in progress download
// io_thread
//      |----> data ---->|
//                     file_thread (writes to disk)
//                              |----> stats ---->|
//                                              ui_thread (feedback for user and
//                                                         updates to history)
//
// Cancel operations perform the inverse order when triggered by a user action:
// ui_thread (user click)
//    |----> cancel command ---->|
//                          file_thread (close file)
//                                 |----> cancel command ---->|
//                                                    io_thread (stops net IO
//                                                               for download)
//
// The DownloadFileManager tracks download requests, mapping from a download
// ID (unique integer created in the IO thread) to the DownloadManager for the
// tab (profile) where the download was initiated. In the event of a tab closure
// during a download, the DownloadFileManager will continue to route data to the
// appropriate DownloadManager. In progress downloads are cancelled for a
// DownloadManager that exits (such as when closing a profile).

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/hash_tables.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "base/timer.h"
#include "chrome/browser/history/download_types.h"

namespace net {
class IOBuffer;
}
class DownloadManager;
class FilePath;
class GURL;
class MessageLoop;
class ResourceDispatcherHost;
class URLRequestContext;

// DownloadBuffer --------------------------------------------------------------

// This container is created and populated on the io_thread, and passed to the
// file_thread for writing. In order to avoid flooding the file_thread with too
// many small write messages, each write is appended to the DownloadBuffer while
// waiting for the task to run on the file_thread. Access to the write buffers
// is synchronized via the lock. Each entry in 'contents' represents one data
// buffer and its size in bytes.

struct DownloadBuffer {
  Lock lock;
  typedef std::pair<net::IOBuffer*, int> Contents;
  std::vector<Contents> contents;
};

// DownloadFile ----------------------------------------------------------------

// These objects live exclusively on the download thread and handle the writing
// operations for one download. These objects live only for the duration that
// the download is 'in progress': once the download has been completed or
// cancelled, the DownloadFile is destroyed.
class DownloadFile {
 public:
  DownloadFile(const DownloadCreateInfo* info);
  ~DownloadFile();

  bool Initialize();

  // Write a new chunk of data to the file. Returns true on success.
  bool AppendDataToFile(const char* data, int data_len);

  // Abort the download and automatically close the file.
  void Cancel();

  // Rename the download file. Returns 'true' if the rename was successful.
  bool Rename(const FilePath& full_path);

  // Accessors.
  int64 bytes_so_far() const { return bytes_so_far_; }
  int id() const { return id_; }
  FilePath full_path() const { return full_path_; }
  int render_process_id() const { return render_process_id_; }
  int render_view_id() const { return render_view_id_; }
  int request_id() const { return request_id_; }
  bool path_renamed() const { return path_renamed_; }
  bool in_progress() const { return file_ != NULL; }
  void set_in_progress(bool in_progress) { in_progress_ = in_progress; }

 private:
  // Open or Close the OS file handle. The file is opened in the constructor
  // based on creation information passed to it, and automatically closed in
  // the destructor.
  void Close();
  bool Open(const char* open_mode);

  // OS file handle for writing
  FILE* file_;

  // The unique identifier for this download, assigned at creation by
  // the DownloadFileManager for its internal record keeping.
  int id_;

  // IDs for looking up the tab we are associated with.
  int render_process_id_;
  int render_view_id_;

  // Handle for informing the ResourceDispatcherHost of a UI based cancel.
  int request_id_;

  // Amount of data received up to this point. We may not know in advance how
  // much data to expect since some servers don't provide that information.
  int64 bytes_so_far_;

  // Full path to the downloaded file.
  FilePath full_path_;

  // Whether the download is still using its initial temporary path.
  bool path_renamed_;

  // Whether the download is still receiving data.
  bool in_progress_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFile);
};


// DownloadFileManager ---------------------------------------------------------

// Manages all in progress downloads.
class DownloadFileManager
    : public base::RefCountedThreadSafe<DownloadFileManager> {
 public:
  DownloadFileManager(MessageLoop* ui_loop, ResourceDispatcherHost* rdh);
  ~DownloadFileManager();

  // Lifetime management functions, called on the UI thread.
  void Initialize();
  void Shutdown();

  // Called on the IO thread
  int GetNextId();

  // Handlers for notifications sent from the IO thread and run on the
  // download thread.
  void StartDownload(DownloadCreateInfo* info);
  void UpdateDownload(int id, DownloadBuffer* buffer);
  void CancelDownload(int id);
  void DownloadFinished(int id, DownloadBuffer* buffer);

  // Handlers for notifications sent from the download thread and run on
  // the UI thread.
  void OnStartDownload(DownloadCreateInfo* info);
  void OnDownloadFinished(int id, int64 bytes_so_far);

  // Download the URL. Called on the UI thread and forwarded to the
  // ResourceDispatcherHost on the IO thread.
  void DownloadUrl(const GURL& url,
                   const GURL& referrer,
                   int render_process_host_id,
                   int render_view_id,
                   URLRequestContext* request_context);

  // Run on the IO thread to initiate the download of a URL.
  void OnDownloadUrl(const GURL& url,
                     const GURL& referrer,
                     int render_process_host_id,
                     int render_view_id,
                     URLRequestContext* request_context);

  // Called on the UI thread to remove a download item or manager.
  void RemoveDownloadManager(DownloadManager* manager);
  void RemoveDownload(int id, DownloadManager* manager);

  // Handler for shell operations sent from the UI to the download thread.
  void OnShowDownloadInShell(const FilePath& full_path);
  // Handler to open or execute a downloaded file.
  void OnOpenDownloadInShell(const FilePath& full_path,
                             const std::wstring& url,
                             gfx::NativeView parent_window);

  // The download manager has provided a final name for a download. Sent from
  // the UI thread and run on the download thread.
  void OnFinalDownloadName(int id, const FilePath& full_path);

  // Timer notifications.
  void UpdateInProgressDownloads();

  MessageLoop* file_loop() const { return file_loop_; }

  // Called by the download manager to delete non validated dangerous downloads.
  static void DeleteFile(const FilePath& path);

 private:
  // Timer helpers for updating the UI about the current progress of a download.
  void StartUpdateTimer();
  void StopUpdateTimer();

  // Clean up helper that runs on the download thread.
  void OnShutdown();

  // Called only on UI thread to get the DownloadManager for a tab's profile.
  static DownloadManager* DownloadManagerFromRenderIds(int render_process_id,
                                                       int review_view_id);
  DownloadManager* LookupManager(int download_id);

  // Called only on the download thread.
  DownloadFile* LookupDownload(int id);

  // Called on the UI thread to remove a download from the UI progress table.
  void RemoveDownloadFromUIProgress(int id);

  // Unique ID for each DownloadFile.
  int next_id_;

  // A map of all in progress downloads.
  typedef base::hash_map<int, DownloadFile*> DownloadFileMap;
  DownloadFileMap downloads_;

  // Throttle updates to the UI thread.
  base::RepeatingTimer<DownloadFileManager> update_timer_;

  // The MessageLoop that the DownloadManagers live on.
  MessageLoop* ui_loop_;

  // The MessageLoop that the this objects primarily operates on.
  MessageLoop* file_loop_;

  // Used only for DCHECKs!
  MessageLoop* io_loop_;

  ResourceDispatcherHost* resource_dispatcher_host_;

  // Tracking which DownloadManager to send data to, called only on UI thread.
  // DownloadManagerMap maps download IDs to their DownloadManager.
  typedef base::hash_map<int, DownloadManager*> DownloadManagerMap;
  DownloadManagerMap managers_;

  // RequestMap maps a DownloadManager to all in-progress download IDs.
  // Called only on the UI thread.
  typedef base::hash_set<int> DownloadRequests;
  typedef base::hash_map<DownloadManager*, DownloadRequests> RequestMap;
  RequestMap requests_;

  // Used for progress updates on the UI thread, mapping download->id() to bytes
  // received so far. Written to by the file thread and read by the UI thread.
  typedef base::hash_map<int, int64> ProgressMap;
  ProgressMap ui_progress_;
  Lock progress_lock_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileManager);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
