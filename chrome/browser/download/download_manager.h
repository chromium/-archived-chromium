// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadManager object manages the process of downloading, including
// updates to the history system and providing the information for displaying
// the downloads view in the Destinations tab. There is one DownloadManager per
// active profile in Chrome.
//
// Each download is represented by a DownloadItem, and all DownloadItems
// are owned by the DownloadManager which maintains a global list of all
// downloads. DownloadItems are created when a user initiates a download,
// and exist for the duration of the browser life time.
//
// Download observers:
// Objects that are interested in notifications about new downloads, or progress
// updates for a given download must implement one of the download observer
// interfaces:
//   DownloadItem::Observer:
//     - allows observers to receive notifications about one download from start
//       to completion
//   DownloadManager::Observer:
//     - allows observers, primarily views, to be notified when changes to the
//       set of all downloads (such as new downloads, or deletes) occur
// Use AddObserver() / RemoveObserver() on the appropriate download object to
// receive state updates.
//
// Download state persistence:
// The DownloadManager uses the history service for storing persistent
// information about the state of all downloads. The history system maintains a
// separate table for this called 'downloads'. At the point that the
// DownloadManager is constructed, we query the history service for the state of
// all persisted downloads.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H__
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H__

#include "build/build_config.h"

#include <string>
#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/download_types.h"
#include "chrome/browser/history/history.h"
#if defined(OS_WIN)
// TODO(port): Port this header and remove #ifdef.
#include "chrome/browser/shell_dialogs.h"
#endif
#include "chrome/common/pref_member.h"

class DownloadFileManager;
class DownloadItemView;
class DownloadManager;
class GURL;
class MessageLoop;
class PrefService;
class Profile;
class ResourceDispatcherHost;
class URLRequestContext;
class WebContents;

namespace base {
class Thread;
}

// DownloadItem ----------------------------------------------------------------

// One DownloadItem per download. This is the model class that stores all the
// state for a download. Multiple views, such as a tab's download shelf and the
// Destination tab's download view, may refer to a given DownloadItem.
class DownloadItem {
 public:
  enum DownloadState {
    IN_PROGRESS,
    COMPLETE,
    CANCELLED,
    REMOVING
  };

  enum SafetyState {
    SAFE = 0,
    DANGEROUS,
    DANGEROUS_BUT_VALIDATED  // Dangerous but the user confirmed the download.
  };

  // Interface that observers of a particular download must implement in order
  // to receive updates to the download's status.
  class Observer {
   public:
    virtual void OnDownloadUpdated(DownloadItem* download) = 0;
  };

  // Constructing from persistent store:
  DownloadItem(const DownloadCreateInfo& info);

  // Constructing from user action:
  DownloadItem(int32 download_id,
               const std::wstring& path,
               int path_uniquifier,
               const std::wstring& url,
               const std::wstring& original_name,
               const base::Time start_time,
               int64 download_size,
               int render_process_id,
               int request_id,
               bool is_dangerous);

  ~DownloadItem();

  void Init(bool start_timer);

  // Public API

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notify our observers periodically
  void UpdateObservers();

  // Received a new chunk of data
  void Update(int64 bytes_so_far);

  // Cancel the download operation. We need to distinguish between cancels at
  // exit (DownloadManager destructor) from user interface initiated cancels
  // because at exit, the history system may not exist, and any updates to it
  // require AddRef'ing the DownloadManager in the destructor which results in
  // a DCHECK failure. Set 'update_history' to false when canceling from at
  // exit to prevent this crash. This may result in a difference between the
  // downloaded file's size on disk, and what the history system's last record
  // of it is. At worst, we'll end up re-downloading a small portion of the file
  // when resuming a download (assuming the server supports byte ranges).
  void Cancel(bool update_history);

  // Download operation completed.
  void Finished(int64 size);

  // The user wants to remove the download from the views and history. If
  // |delete_file| is true, the file is deleted on the disk.
  void Remove(bool delete_file);

  // Start/stop sending periodic updates to our observers
  void StartProgressTimer();
  void StopProgressTimer();

  // Simple calculation of the amount of time remaining to completion. Fills
  // |*remaining| with the amount of time remaining if successful. Fails and
  // returns false if we do not have the number of bytes or the speed so can
  // not estimate.
  bool TimeRemaining(base::TimeDelta* remaining) const;

  // Simple speed estimate in bytes/s
  int64 CurrentSpeed() const;

  // Rough percent complete, -1 means we don't know (since we didn't receive a
  // total size).
  int PercentComplete() const;

  // Update the download's path, the actual file is renamed on the download
  // thread.
  void Rename(const std::wstring& full_path);

  // Allow the user to temporarily pause a download or resume a paused download.
  void TogglePause();

  // Accessors
  DownloadState state() const { return state_; }
  std::wstring file_name() const { return file_name_; }
  void set_file_name(const std::wstring& name) { file_name_ = name; }
  std::wstring full_path() const { return full_path_; }
  void set_full_path(const std::wstring& path) { full_path_ = path; }
  int path_uniquifier() const { return path_uniquifier_; }
  void set_path_uniquifier(int uniquifier) { path_uniquifier_ = uniquifier; }
  std::wstring url() const { return url_; }
  int64 total_bytes() const { return total_bytes_; }
  void set_total_bytes(int64 total_bytes) { total_bytes_ = total_bytes; }
  int64 received_bytes() const { return received_bytes_; }
  int32 id() const { return id_; }
  base::Time start_time() const { return start_time_; }
  void set_db_handle(int64 handle) { db_handle_ = handle; }
  int64 db_handle() const { return db_handle_; }
  DownloadManager* manager() const { return manager_; }
  void set_manager(DownloadManager* manager) { manager_ = manager; }
  bool is_paused() const { return is_paused_; }
  void set_is_paused(bool pause) { is_paused_ = pause; }
  bool open_when_complete() const { return open_when_complete_; }
  void set_open_when_complete(bool open) { open_when_complete_ = open; }
  int render_process_id() const { return render_process_id_; }
  int request_id() const { return request_id_; }
  SafetyState safety_state() const { return safety_state_; }
  void set_safety_state(SafetyState safety_state) {
    safety_state_ = safety_state;
  }
  std::wstring original_name() const { return original_name_; }
  void set_original_name(const std::wstring& name) { original_name_ = name; }

  // Returns the file-name that should be reported to the user, which is
  // file_name_ for safe downloads and original_name_ for dangerous ones with
  // the uniquifier number.
  std::wstring GetFileName() const;

 private:
  // Internal helper for maintaining consistent received and total sizes.
  void UpdateSize(int64 size);

  // Request ID assigned by the ResourceDispatcherHost.
  int32 id_;

  // Full path to the downloaded file
  std::wstring full_path_;

  // A number that should be appended to the path to make it unique, or 0 if the
  // path should be used as is.
  int path_uniquifier_;

  // Short display version of the file
  std::wstring file_name_;

  // The URL from whence we came, for display
  std::wstring url_;

  // Total bytes expected
  int64 total_bytes_;

  // Current received bytes
  int64 received_bytes_;

  // Start time for calculating remaining time
  uintptr_t start_tick_;

  // The current state of this download
  DownloadState state_;

  // The views of this item in the download shelf and download tab
  ObserverList<Observer> observers_;

  // Time the download was started
  base::Time start_time_;

  // Our persistent store handle
  int64 db_handle_;

  // Timer for regularly updating our observers
  base::RepeatingTimer<DownloadItem> update_timer_;

  // Our owning object
  DownloadManager* manager_;

  // In progress downloads may be paused by the user, we note it here
  bool is_paused_;

  // A flag for indicating if the download should be opened at completion.
  bool open_when_complete_;

  // Whether the download is considered potentially safe or dangerous
  // (executable files are typically considered dangerous).
  SafetyState safety_state_;

  // Dangerous download are given temporary names until the user approves them.
  // This stores their original name.
  std::wstring original_name_;

  // For canceling or pausing requests.
  int render_process_id_;
  int request_id_;

  DISALLOW_EVIL_CONSTRUCTORS(DownloadItem);
};


// DownloadManager -------------------------------------------------------------

#if defined(OS_WIN)
// TODO(port): Port this part of the header and remove the #ifdef.

// Browser's download manager: manages all downloads and destination view.
class DownloadManager : public base::RefCountedThreadSafe<DownloadManager>,
                        public SelectFileDialog::Listener {
  // For testing.
  friend class DownloadManagerTest;

 public:
  DownloadManager();
  ~DownloadManager();

  static void RegisterUserPrefs(PrefService* prefs);

  // Interface to implement for observers that wish to be informed of changes
  // to the DownloadManager's collection of downloads.
  class Observer {
   public:
    // New or deleted download, observers should query us for the current set
    // of downloads.
    virtual void ModelChanged() = 0;

    // A callback once the DownloadManager has retrieved the requested set of
    // downloads. The DownloadManagerObserver must copy the vector, but does not
    // own the individual DownloadItems, when this call is made.
    virtual void SetDownloads(std::vector<DownloadItem*>& downloads) = 0;
  };

  // Public API

  // Begin a search for all downloads matching 'search_text'. If 'search_text'
  // is empty, return all known downloads. The results are returned in the
  // 'SetDownloads' observer callback.
  void GetDownloads(Observer* observer,
                    const std::wstring& search_text);

  // Returns true if initialized properly.
  bool Init(Profile* profile);

  // Schedule a query of the history service to retrieve all downloads.
  void QueryHistoryForDownloads();

  // Notifications sent from the download thread to the UI thread
  void StartDownload(DownloadCreateInfo* info);
  void UpdateDownload(int32 download_id, int64 size);
  void DownloadFinished(int32 download_id, int64 size);

  // Helper method for cancelling the network request associated with a
  // download.
  static void CancelDownloadRequest(int render_process_id, int request_id);

  // Called from a view when a user clicks a UI button or link.
  void DownloadCancelled(int32 download_id);
  void PauseDownload(int32 download_id, bool pause);
  void RemoveDownload(int64 download_handle);

  // Remove downloads after remove_begin (inclusive) and before remove_end
  // (exclusive). You may pass in null Time values to do an unbounded delete
  // in either direction.
  int RemoveDownloadsBetween(const base::Time remove_begin,
                             const base::Time remove_end);

  // Remove downloads will delete all downloads that have a timestamp that is
  // the same or more recent than |remove_begin|. The number of downloads
  // deleted is returned back to the caller.
  int RemoveDownloads(const base::Time remove_begin);

  // Download the object at the URL. Used in cases such as "Save Link As..."
  void DownloadUrl(const GURL& url,
                   const GURL& referrer,
                   WebContents* web_contents);

  // Allow objects to observe the download creation process.
  void AddObserver(Observer* observer);

  // Remove a download observer from ourself.
  void RemoveObserver(Observer* observer);

  // Methods called on completion of a query sent to the history system.
  void OnQueryDownloadEntriesComplete(
      std::vector<DownloadCreateInfo>* entries);
  void OnCreateDownloadEntryComplete(DownloadCreateInfo info, int64 db_handle);
  void OnSearchComplete(HistoryService::Handle handle,
                        std::vector<int64>* results);

  // Show or Open a download via the Windows shell.
  void ShowDownloadInShell(const DownloadItem* download);
  void OpenDownloadInShell(const DownloadItem* download, HWND parent_window);

  // The number of in progress (including paused) downloads.
  int in_progress_count() const {
    return static_cast<int>(in_progress_.size());
  }

  std::wstring download_path() { return *download_path_; }

  // Clears the last download path, used to initialize "save as" dialogs.  
  void ClearLastDownloadPath();

  // Registers this file extension for automatic opening upon download
  // completion if 'open' is true, or prevents the extension from automatic
  // opening if 'open' is false.
  void OpenFilesOfExtension(const std::wstring& extension, bool open);

  // Tests if a file type should be opened automatically.
  bool ShouldOpenFileExtension(const std::wstring& extension);

  // Tests if we think the server means for this mime_type to be executable.
  static bool IsExecutableMimeType(const std::string& mime_type);

  // Tests if a file type is considered executable.
  bool IsExecutable(const std::wstring& extension);

  // Resets the automatic open preference.
  void ResetAutoOpenFiles();

  // Returns true if there are automatic handlers registered for any file
  // types.
  bool HasAutoOpenFileTypesRegistered() const;

  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const std::wstring& path, void* params);
  virtual void FileSelectionCanceled(void* params);

  // Deletes the specified path on the file thread.
  void DeleteDownload(const std::wstring& path);

  // Called when the user has validated the donwload of a dangerous file.
  void DangerousDownloadValidated(DownloadItem* download);

  // Used to make sure we have a safe file extension and filename for a
  // download.  |file_name| can either be just the file name or it can be a
  // full path to a file.
  void GenerateSafeFilename(const std::string& mime_type,
                            std::wstring* file_name);

 private:
  // Shutdown the download manager.  This call is needed only after Init.
  void Shutdown();

  // Called on the download thread to check whether the suggested file path
  // exists.  We don't check if the file exists on the UI thread to avoid UI
  // stalls from interacting with the file system.
  void CheckIfSuggestedPathExists(DownloadCreateInfo* info);

  // Called on the UI thread once the DownloadManager has determined whether the
  // suggested file path exists.
  void OnPathExistenceAvailable(DownloadCreateInfo* info);

  // Called back after a target path for the file to be downloaded to has been
  // determined, either automatically based on the suggested file name, or by
  // the user in a Save As dialog box.
  void ContinueStartDownload(DownloadCreateInfo* info,
                             const std::wstring& target_path);

  // Update the history service for a particular download.
  void UpdateHistoryForDownload(DownloadItem* download);
  void RemoveDownloadFromHistory(DownloadItem* download);
  void RemoveDownloadsFromHistoryBetween(const base::Time remove_begin,
                                         const base::Time remove_before);

  // Inform the notification service of download starts and stops.
  void NotifyAboutDownloadStart();
  void NotifyAboutDownloadStop();

  // Create an extension based on the file name and mime type.
  void GenerateExtension(const std::wstring& file_name,
                         const std::string& mime_type,
                         std::wstring* generated_extension);

  // Create a file name based on the response from the server.
  void GenerateFilename(DownloadCreateInfo* info, std::wstring* generated_name);

  // Persist the automatic opening preference.
  void SaveAutoOpens();

  // Runs the network cancel on the IO thread.
  static void OnCancelDownloadRequest(ResourceDispatcherHost* rdh,
                                      int render_process_id,
                                      int request_id);

  // Runs the pause on the IO thread.
  static void OnPauseDownloadRequest(ResourceDispatcherHost* rdh,
                                     int render_process_id,
                                     int request_id,
                                     bool pause);

  // Performs the last steps required when a download has been completed.
  // It is necessary to break down the flow when a download is finished as
  // dangerous downloads are downloaded to temporary files that need to be
  // renamed on the file thread first.
  // Invoked on the UI thread.
  void ContinueDownloadFinished(DownloadItem* download);

  // Renames a finished dangerous download from its temporary file name to its
  // real file name.
  // Invoked on the file thread.
  void ProceedWithFinishedDangerousDownload(int64 download_handle, 
                                            const std::wstring& path,
                                            const std::wstring& original_name);

  // Invoked on the UI thread when a dangerous downloaded file has been renamed.
  void DangerousDownloadRenamed(int64 download_handle,
                                bool success,
                                const std::wstring& new_path,
                                int new_path_uniquifier);

  // Checks whether a file represents a risk if downloaded.
  bool IsDangerous(const std::wstring& file_name);

  // Changes the paths and file name of the specified |download|, propagating
  // the change to the history system.
  void RenameDownload(DownloadItem* download, const std::wstring& new_path);

  // 'downloads_' is map of all downloads in this profile. The key is the handle
  // returned by the history system, which is unique across sessions. This map
  // owns all the DownloadItems once they have been created in the history
  // system.
  //
  // 'in_progress_' is a map of all downloads that are in progress and that have
  // not yet received a valid history handle. The key is the ID assigned by the
  // ResourceDispatcherHost, which is unique for the current session. This map
  // does not own the DownloadItems.
  //
  // 'dangerous_finished_' is a map of dangerous download that have finished
  // but were not yet approved by the user.  Similarly to in_progress_, the key
  // is the ID assigned by the ResourceDispatcherHost and the map does not own
  // the DownloadItems.  It is used on shutdown to delete completed downloads
  // that have not been approved.
  // 
  // When a download is created through a user action, the corresponding
  // DownloadItem* is placed in 'in_progress_' and remains there until it has
  // received a valid handle from the history system. Once it has a valid
  // handle, the DownloadItem* is placed in the 'downloads_' map. When the
  // download is complete, it is removed from 'in_progress_'. Downloads from
  // past sessions read from a persisted state from the history system are
  // placed directly into 'downloads_' since they have valid handles in the
  // history system.
  typedef base::hash_map<int64, DownloadItem*> DownloadMap;
  DownloadMap downloads_;
  DownloadMap in_progress_;
  DownloadMap dangerous_finished_;

  // True if the download manager has been initialized and requires a shutdown.
  bool shutdown_needed_;

  // Observers that want to be notified of changes to the set of downloads.
  ObserverList<Observer> observers_;

  // The current active profile.
  Profile* profile_;
  scoped_refptr<URLRequestContext> request_context_;

  // Used for history service request management.
  CancelableRequestConsumerT<Observer*, 0> cancelable_consumer_;

  // Non-owning pointer for handling file writing on the download_thread_.
  DownloadFileManager* file_manager_;

  // A pointer to the main UI loop.
  MessageLoop* ui_loop_;

  // A pointer to the file thread's loop. The file thread lives longer than
  // the DownloadManager, so this is safe to cache.
  MessageLoop* file_loop_;

  // User preferences
  BooleanPrefMember prompt_for_download_;
  StringPrefMember download_path_;

  // The user's last choice for download directory. This is only used when the
  // user wants us to prompt for a save location for each download.
  std::wstring last_download_path_;

  // Set of file extensions to open at download completion.
  std::set<std::wstring> auto_open_;

  // Set of file extensions that are executables and shouldn't be auto opened.
  std::set<std::wstring> exe_types_;

  // Keep track of downloads that are completed before the user selects the
  // destination, so that observers are appropriately notified of completion
  // after this determination is made.
  // The map is of download_id->remaining size (bytes), both of which are
  // required when calling DownloadFinished.
  typedef std::map<int32, int64> PendingFinishedMap;
  PendingFinishedMap pending_finished_downloads_;

  // The "Save As" dialog box used to ask the user where a file should be
  // saved.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  DISALLOW_EVIL_CONSTRUCTORS(DownloadManager);
};

#endif  // defined(OS_WIN)

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H__
