// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Objects that handle file operations for saving files, on the file thread.
//
// The SaveFileManager owns a set of SaveFile objects, each of which connects
// with a SaveItem object which belongs to one SavePackage and runs on the file
// thread for saving data in order to avoid disk activity on either network IO
// thread or the UI thread. It coordinates the notifications from the network
// and UI.
//
// The SaveFileManager itself is a singleton object owned by the
// ResourceDispatcherHost.
//
// The data sent to SaveFileManager have 2 sources, one is from
// ResourceDispatcherHost, run in network IO thread, the all sub-resources
// and save-only-HTML pages will be got from network IO. The second is from
// render process, those html pages which are serialized from DOM will be
// composed in render process and encoded to its original encoding, then sent
// to UI loop in browser process, then UI loop will dispatch the data to
// SaveFileManager on the file thread. SaveFileManager will directly
// call SaveFile's method to persist data.
//
// A typical saving job operation involves multiple threads:
//
// Updating an in progress save file
// io_thread
//      |----> data from net   ---->|
//                                  |
//                                  |
//      |----> data from    ---->|  |
//      |      render process    |  |
// ui_thread                     |  |
//                      file_thread (writes to disk)
//                              |----> stats ---->|
//                                              ui_thread (feedback for user)
//
//
// Cancel operations perform the inverse order when triggered by a user action:
// ui_thread (user click)
//    |----> cancel command ---->|
//    |           |      file_thread (close file)
//    |           |---------------------> cancel command ---->|
//    |                                               io_thread (stops net IO
// ui_thread (user close tab)                                    for saving)
//    |----> cancel command ---->|
//                            Render process(stop serializing DOM and sending
//                                           data)
//
//
// The SaveFileManager tracks saving requests, mapping from a save ID
// (unique integer created in the IO thread) to the SavePackage for the
// tab where the saving job was initiated. In the event of a tab closure
// during saving, the SavePackage will notice the SaveFileManage to
// cancel all SaveFile job.

#ifndef CHROME_BROWSER_DOWNLOAD_SAVE_FILE_MANAGER_H__
#define CHROME_BROWSER_DOWNLOAD_SAVE_FILE_MANAGER_H__

#include <utility>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "chrome/browser/download/save_types.h"

class GURL;
class SaveFile;
class SavePackage;
class MessageLoop;
class ResourceDispatcherHost;
class Task;
class URLRequestContext;

class SaveFileManager
    : public base::RefCountedThreadSafe<SaveFileManager> {
 public:
  SaveFileManager(MessageLoop* ui_loop,
                  MessageLoop* io_loop,
                  ResourceDispatcherHost* rdh);
  ~SaveFileManager();

  // Lifetime management.
  void Shutdown();

  // Called on the IO thread
  int GetNextId();

  // Save the specified URL. Called on the UI thread and forwarded to the
  // ResourceDispatcherHost on the IO thread.
  void SaveURL(const std::wstring& url,
               const std::wstring& referrer,
               int render_process_host_id,
               int render_view_id,
               SaveFileCreateInfo::SaveFileSource save_source,
               const std::wstring& file_full_path,
               URLRequestContext* request_context,
               SavePackage* save_package);

  // Notifications sent from the IO thread and run on the file thread:
  void StartSave(SaveFileCreateInfo* info);
  void UpdateSaveProgress(int save_id, char* data, int size);
  void SaveFinished(int save_id, std::wstring save_url,
                    int render_process_id, bool is_success);

  // Notifications sent from the UI thread and run on the file thread.
  // Cancel a SaveFile instance which has specified save id.
  void CancelSave(int save_id);

  // Called on the UI thread to remove a save package from SaveFileManager's
  // tracking map.
  void RemoveSaveFile(int save_id, const std::wstring& save_url,
                      SavePackage* package);

  // Handler for shell operations sent from the UI to the file thread.
  void OnShowSavedFileInShell(const std::wstring full_path);

  // Helper function for deleting specified file.
  void DeleteDirectoryOrFile(const std::wstring& full_path, bool is_dir);

  // For posting notifications from the UI and IO threads.
  MessageLoop* GetSaveLoop() const { return file_loop_; }

  // Runs on file thread to save a file by copying from file system when
  // original url is using file scheme.
  void SaveLocalFile(const std::wstring& original_file_url,
                     int save_id,
                     int render_process_id);

  // Renames all the successfully saved files.
  // |final_names| points to a vector which contains pairs of save ids and
  // final names of successfully saved files.
  void RenameAllFiles(
      const FinalNameList& final_names,
      const std::wstring& resource_dir,
      int render_process_id,
      int render_view_id);

  // When the user cancels the saving, we need to remove all remaining saved
  // files of this page saving job from save_file_map_.
  void RemoveSavedFileFromFileMap(const SaveIDList & save_ids);

 private:
  // A cleanup helper that runs on the file thread.
  void OnShutdown();

  // The resource does not come from the network, but we still needs to call
  // this function for getting unique save ID by calling
  // OnRequireSaveJobFromOtherSource in the net IO thread and start saving
  // operation. This function is called on the UI thread.
  void RequireSaveJobFromOtherSource(SaveFileCreateInfo* info);

  // Called only on UI thread to get the SavePackage for a tab's profile.
  static SavePackage* GetSavePackageFromRenderIds(int render_process_id,
                                                  int review_view_id);

  // Register a starting request. Associate the save URL with a
  // SavePackage for further matching.
  void RegisterStartingRequest(const std::wstring& save_url,
                               SavePackage* save_package);
  // Unregister a start request according save URL, disassociate
  // the save URL and SavePackage.
  SavePackage* UnregisterStartingRequest(const std::wstring& save_url,
                                         int tab_id);

  // Look up the SavePackage according to save id.
  SavePackage* LookupPackage(int save_id);

  // Called only on the file thread.
  // Look up one in-progress saving item according to save id.
  SaveFile* LookupSaveFile(int save_id);

  // Help function for sending notification of canceling specific request.
  void SendCancelRequest(int save_id);

  // Notifications sent from the file thread and run on the UI thread.

  // Lookup the SaveManager for this WebContents' saving profile and inform it
  // the saving job has been started.
  void OnStartSave(const SaveFileCreateInfo* info);
  // Update the SavePackage with the current state of a started saving job.
  // If the SavePackage for this saving job is gone, cancel the request.
  void OnUpdateSaveProgress(int save_id,
                            int64 bytes_so_far,
                            bool write_success);
  // Update the SavePackage with the finish state, and remove the request
  // tracking entries.
  void OnSaveFinished(int save_id, int64 bytes_so_far, bool is_success);
  // For those requests that do not have valid save id, use
  // map:(url, SavePackage) to find the request and remove it.
  void OnErrorFinished(std::wstring save_url, int tab_id);
  // Handler for a notification sent to the UI thread.
  // The user has requested a cancel in the UI thread, so send a cancel request
  // to stop the network requests in net IO thread.
  void OnCancelSaveRequest(int render_process_id, int request_id);
  // Notifies SavePackage that the whole page saving job is finished.
  void OnFinishSavePageJob(int render_process_id, int render_view_id);

  // Notifications sent from the UI thread and run on the file thread.

  // Deletes a specified file on the file thread.
  void OnDeleteDirectoryOrFile(const std::wstring& full_path, bool is_dir);

  // Notifications sent from the UI thread and run on the IO thread

  // Initiates a request for URL to be saved.
  void OnSaveURL(const GURL& url,
                 const GURL& referrer,
                 int render_process_host_id,
                 int render_view_id,
                 URLRequestContext* request_context);
  // Handler for a notification sent to the IO thread for generating save id.
  void OnRequireSaveJobFromOtherSource(SaveFileCreateInfo* info);
  // Call ResourceDispatcherHost's CancelRequest method to execute cancel
  // action in the IO thread.
  void ExecuteCancelSaveRequest(int render_process_id, int request_id);

  // Unique ID for the next SaveFile object.
  int next_id_;

  // A map of all saving jobs by using save id.
  typedef base::hash_map<int, SaveFile*> SaveFileMap;
  SaveFileMap save_file_map_;

  // Message loop that the SavePackages live on.
  MessageLoop* ui_loop_;

  // We cache the IO loop, we will use it to request resources from network.
  MessageLoop* io_loop_;

  // We cache the file loop, we will use it to do real file operation.
  // We guarantee that we won't access them incorrectly during the shutdown
  // process
  MessageLoop* file_loop_;

  ResourceDispatcherHost* resource_dispatcher_host_;

  // Tracks which SavePackage to send data to, called only on UI thread.
  // SavePackageMap maps save IDs to their SavePackage.
  typedef base::hash_map<int, SavePackage*> SavePackageMap;
  SavePackageMap packages_;

  // There is a gap between after calling SaveURL() and before calling
  // StartSave(). In this gap, each request does not have save id for tracking.
  // But sometimes users might want to stop saving job or ResourceDispatcherHost
  // calls SaveFinished with save id -1 for network error. We name the requests
  // as starting requests. For tracking those starting requests, we need to
  // have some data structure.
  // First we use a hashmap to map the request URL to SavePackage, then we
  // use a hashmap to map the tab id (we actually use render_process_id) to the
  // hashmap since it is possible to save same URL in different tab at
  // same time.
  typedef base::hash_map<std::wstring, SavePackage*> StartingRequestsMap;
  typedef base::hash_map<int, StartingRequestsMap> TabToStartingRequestsMap;
  TabToStartingRequestsMap tab_starting_requests_;

  DISALLOW_EVIL_CONSTRUCTORS(SaveFileManager);
};

#endif  // CHROME_BROWSER_DOWNLOAD_SAVE_FILE_MANAGER_H__
