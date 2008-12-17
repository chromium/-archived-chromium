// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Windows.h>
#include <objbase.h>

#include "chrome/browser/download/save_file_manager.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/save_file.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_util.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/common/win_util.h"
#include "chrome/common/win_safe_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context.h"

SaveFileManager::SaveFileManager(MessageLoop* ui_loop,
                                 MessageLoop* io_loop,
                                 ResourceDispatcherHost* rdh)
    : next_id_(0),
      ui_loop_(ui_loop),
      io_loop_(io_loop),
      resource_dispatcher_host_(rdh) {
  DCHECK(ui_loop_);
  // Need to make sure that we are in UI thread because using g_browser_process
  // on a non-UI thread can cause crashes during shutdown.
  DCHECK(ui_loop_ == MessageLoop::current());
  // Cache the message loop of file thread.
  base::Thread* thread = g_browser_process->file_thread();
  if (thread)
    file_loop_ = thread->message_loop();
  else
    // It could be NULL when it is created in unit test of
    // ResourceDispatcherHost.
    file_loop_ = NULL;
  DCHECK(resource_dispatcher_host_);
}

SaveFileManager::~SaveFileManager() {
  // Check for clean shutdown.
  DCHECK(save_file_map_.empty());
}

// Called during the browser shutdown process to clean up any state (open files,
// timers) that live on the saving thread (file thread).
void SaveFileManager::Shutdown() {
  MessageLoop* loop = GetSaveLoop();
  if (loop) {
    loop->PostTask(FROM_HERE,
        NewRunnableMethod(this, &SaveFileManager::OnShutdown));
  }
}

// Stop file thread operations.
void SaveFileManager::OnShutdown() {
  DCHECK(MessageLoop::current() == GetSaveLoop());
  STLDeleteValues(&save_file_map_);
}

SaveFile* SaveFileManager::LookupSaveFile(int save_id) {
  SaveFileMap::iterator it = save_file_map_.find(save_id);
  return it == save_file_map_.end() ? NULL : it->second;
}

// Called on the IO thread when
// a) The ResourceDispatcherHost has decided that a request is savable.
// b) The resource does not come from the network, but we still need a
// save ID for for managing the status of the saving operation. So we
// file a request from the file thread to the IO thread to generate a
// unique save ID.
int SaveFileManager::GetNextId() {
  DCHECK(MessageLoop::current() == io_loop_);
  return next_id_++;
}

void SaveFileManager::RegisterStartingRequest(const std::wstring& save_url,
                                              SavePackage* save_package) {
  // Make sure it runs in the UI thread.
  DCHECK(MessageLoop::current() == ui_loop_);
  int tab_id = save_package->tab_id();

  // Register this starting request.
  StartingRequestsMap& starting_requests = tab_starting_requests_[tab_id];
  bool never_present = starting_requests.insert(
      StartingRequestsMap::value_type(save_url, save_package)).second;
  DCHECK(never_present);
}

SavePackage* SaveFileManager::UnregisterStartingRequest(
    const std::wstring& save_url, int tab_id) {
  // Make sure it runs in UI thread.
  DCHECK(MessageLoop::current() == ui_loop_);

  TabToStartingRequestsMap::iterator it = tab_starting_requests_.find(tab_id);
  if (it != tab_starting_requests_.end()) {
    StartingRequestsMap& requests = it->second;
    StartingRequestsMap::iterator sit = requests.find(save_url);
    if (sit == requests.end())
      return NULL;

    // Found, erase it from starting list and return SavePackage.
    SavePackage* save_package = sit->second;
    requests.erase(sit);
    // If there is no element in requests, remove it
    if (requests.empty())
      tab_starting_requests_.erase(it);
    return save_package;
  }

  return NULL;
}

void SaveFileManager::RequireSaveJobFromOtherSource(SaveFileCreateInfo* info) {
  // This function must be called on the UI thread, because the io_loop_
  // pointer may be junk when we use it on file thread. We can only rely on the
  // io_loop_ pointer being valid when we run code on the UI thread (or on
  // the IO thread.
  DCHECK(MessageLoop::current() == ui_loop_);
  DCHECK(info->save_id == -1);
  // Since the data will come from render process, so we need to start
  // this kind of save job by ourself.
  io_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &SaveFileManager::OnRequireSaveJobFromOtherSource,
                        info));
}

// Look up a SavePackage according to a save id.
SavePackage* SaveFileManager::LookupPackage(int save_id) {
  DCHECK(MessageLoop::current() == ui_loop_);
  SavePackageMap::iterator it = packages_.find(save_id);
  if (it != packages_.end())
    return it->second;
  return NULL;
}

// Call from SavePackage for starting a saving job
void SaveFileManager::SaveURL(const std::wstring& url,
                              const std::wstring& referrer,
                              int render_process_host_id,
                              int render_view_id,
                              SaveFileCreateInfo::SaveFileSource save_source,
                              const std::wstring& file_full_path,
                              URLRequestContext* request_context,
                              SavePackage* save_package) {
  DCHECK(MessageLoop::current() == ui_loop_);
  if (!io_loop_) {
    NOTREACHED();  // Net IO thread must exist.
    return;
  }

  // Register a saving job.
  RegisterStartingRequest(url, save_package);
  if (save_source == SaveFileCreateInfo::SAVE_FILE_FROM_NET) {
    GURL save_url(url);
    DCHECK(save_url.is_valid());

    io_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &SaveFileManager::OnSaveURL,
                          save_url,
                          GURL(referrer),
                          render_process_host_id,
                          render_view_id,
                          request_context));
  } else {
    // We manually start the save job.
    SaveFileCreateInfo* info = new SaveFileCreateInfo(file_full_path,
         url,
         save_source,
         -1);
    info->render_process_id = render_process_host_id;
    info->render_view_id = render_view_id;
    RequireSaveJobFromOtherSource(info);
  }
}

// Utility function for look up table maintenance, called on the UI thread.
// A manager may have multiple save page job (SavePackage) in progress,
// so we just look up the save id and remove it from the tracking table.
// If the save id is -1, it means we just send a request to save, but the
// saving action has still not happened, need to call UnregisterStartingRequest
// to remove it from the tracking map.
void SaveFileManager::RemoveSaveFile(int save_id, const std::wstring& save_url,
                                     SavePackage* package) {
  DCHECK(MessageLoop::current() == ui_loop_ && package);
  // A save page job(SavePackage) can only have one manager,
  // so remove it if it exists.
  if (save_id == -1) {
    SavePackage* old_package = UnregisterStartingRequest(save_url,
                                                         package->tab_id());
    DCHECK(old_package == package);
  } else {
    SavePackageMap::iterator it = packages_.find(save_id);
    if (it != packages_.end())
      packages_.erase(it);
  }
}

// Static
// Utility function for converting request IDs to a TabContents. Must be called
// only on the UI thread.
SavePackage* SaveFileManager::GetSavePackageFromRenderIds(
    int render_process_id, int render_view_id) {
  TabContents* contents = tab_util::GetTabContentsByID(render_process_id,
                                                       render_view_id);
  if (contents && contents->type() == TAB_CONTENTS_WEB) {
    // Convert const pointer of WebContents to pointer of WebContents.
    const WebContents* web_contents = contents->AsWebContents();
    if (web_contents)
      return web_contents->save_package();
  }

  return NULL;
}

// Utility function for deleting specified file.
void SaveFileManager::DeleteDirectoryOrFile(const std::wstring& full_path,
                                            bool is_dir) {
  DCHECK(MessageLoop::current() == ui_loop_);
  MessageLoop* loop = GetSaveLoop();
  DCHECK(loop);
  loop->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &SaveFileManager::OnDeleteDirectoryOrFile,
                        full_path,
                        is_dir));
}

void SaveFileManager::SendCancelRequest(int save_id) {
  // Cancel the request which has specific save id.
  DCHECK(save_id > -1);
  MessageLoop* loop = GetSaveLoop();
  DCHECK(loop);
  loop->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &SaveFileManager::CancelSave,
                        save_id));
}

// Notifications sent from the IO thread and run on the file thread:

// The IO thread created |info|, but the file thread (this method) uses it
// to create a SaveFile which will hold and finally destroy |info|. It will
// then passes |info| to the UI thread for reporting saving status.
void SaveFileManager::StartSave(SaveFileCreateInfo* info) {
  DCHECK(MessageLoop::current() == GetSaveLoop());
  DCHECK(info);
  SaveFile* save_file = new SaveFile(info);
  DCHECK(LookupSaveFile(info->save_id) == NULL);
  save_file_map_[info->save_id] = save_file;
  info->path = save_file->full_path();

  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &SaveFileManager::OnStartSave,
                        info));
}

// We do forward an update to the UI thread here, since we do not use timer to
// update the UI. If the user has canceled the saving action (in the UI
// thread). We may receive a few more updates before the IO thread gets the
// cancel message. We just delete the data since the SaveFile has been deleted.
void SaveFileManager::UpdateSaveProgress(int save_id,
                                         char* data,
                                         int data_len) {
  DCHECK(MessageLoop::current() == GetSaveLoop());
  SaveFile* save_file = LookupSaveFile(save_id);
  if (save_file) {
    bool write_success = save_file->AppendDataToFile(data, data_len);
    ui_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &SaveFileManager::OnUpdateSaveProgress,
                          save_file->save_id(),
                          save_file->bytes_so_far(),
                          write_success));
  }
  delete [] data;
}

// The IO thread will call this when saving is completed or it got error when
// fetching data. In the former case, we forward the message to OnSaveFinished
// in UI thread. In the latter case, the save ID will be -1, which means the
// saving action did not even start, so we need to call OnErrorFinished in UI
// thread, which will use the save URL to find corresponding request record and
// delete it.
void SaveFileManager::SaveFinished(int save_id,
                                   std::wstring save_url,
                                   int render_process_id,
                                   bool is_success) {
  DCHECK(MessageLoop::current() == GetSaveLoop());
  SaveFileMap::iterator it = save_file_map_.find(save_id);
  if (it != save_file_map_.end()) {
    SaveFile* save_file = it->second;
    ui_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &SaveFileManager::OnSaveFinished,
                          save_id,
                          save_file->bytes_so_far(),
                          is_success));

    save_file->Finish();
  } else if (save_id == -1) {
    // Before saving started, we got error. We still call finish process.
    DCHECK(!save_url.empty());
    ui_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &SaveFileManager::OnErrorFinished,
                          save_url,
                          render_process_id));
  }
}

// Notifications sent from the file thread and run on the UI thread.

void SaveFileManager::OnStartSave(const SaveFileCreateInfo* info) {
  DCHECK(MessageLoop::current() == ui_loop_);
  SavePackage* save_package =
      GetSavePackageFromRenderIds(info->render_process_id,
                                  info->render_view_id);
  if (!save_package) {
    // Cancel this request.
    SendCancelRequest(info->save_id);
    return;
  }

  // Insert started saving job to tracking list.
  SavePackageMap::iterator sit = packages_.find(info->save_id);
  if (sit == packages_.end()) {
    // Find the registered request. If we can not find, it means we have
    // canceled the job before.
    SavePackage* old_save_package = UnregisterStartingRequest(info->url,
        info->render_process_id);
    if (!old_save_package) {
      // Cancel this request.
      SendCancelRequest(info->save_id);
      return;
    }
    DCHECK(old_save_package == save_package);
    packages_[info->save_id] = save_package;
  } else {
    NOTREACHED();
  }

  // Forward this message to SavePackage.
  save_package->StartSave(info);
}

void SaveFileManager::OnUpdateSaveProgress(int save_id, int64 bytes_so_far,
                                           bool write_success) {
  DCHECK(MessageLoop::current() == ui_loop_);
  SavePackage* package = LookupPackage(save_id);
  if (package)
    package->UpdateSaveProgress(save_id, bytes_so_far, write_success);
  else
    SendCancelRequest(save_id);
}

void SaveFileManager::OnSaveFinished(int save_id,
                                     int64 bytes_so_far,
                                     bool is_success) {
  DCHECK(MessageLoop::current() == ui_loop_);
  SavePackage* package = LookupPackage(save_id);
  if (package)
    package->SaveFinished(save_id, bytes_so_far, is_success);
}

void SaveFileManager::OnErrorFinished(std::wstring save_url, int tab_id) {
  DCHECK(MessageLoop::current() == ui_loop_);
  SavePackage* save_package = UnregisterStartingRequest(save_url, tab_id);
  if (save_package)
    save_package->SaveFailed(save_url);
}

void SaveFileManager::OnCancelSaveRequest(int render_process_id,
                                          int request_id) {
  DCHECK(MessageLoop::current() == ui_loop_);
  DCHECK(io_loop_);
  io_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &SaveFileManager::ExecuteCancelSaveRequest,
                        render_process_id,
                        request_id));
}

// Notifications sent from the UI thread and run on the IO thread.

void SaveFileManager::OnSaveURL(const GURL& url,
                                const GURL& referrer,
                                int render_process_host_id,
                                int render_view_id,
                                URLRequestContext* request_context) {
  DCHECK(MessageLoop::current() == io_loop_);
  resource_dispatcher_host_->BeginSaveFile(url,
                                           referrer,
                                           render_process_host_id,
                                           render_view_id,
                                           request_context);
}

void SaveFileManager::OnRequireSaveJobFromOtherSource(
    SaveFileCreateInfo* info) {
  DCHECK(MessageLoop::current() == io_loop_);
  DCHECK(info->save_id == -1);
  // Generate a unique save id.
  info->save_id = GetNextId();
  // Start real saving action.
  MessageLoop* loop = GetSaveLoop();
  DCHECK(loop);
  loop->PostTask(FROM_HERE,
                 NewRunnableMethod(this,
                                   &SaveFileManager::StartSave,
                                   info));
}

void SaveFileManager::ExecuteCancelSaveRequest(int render_process_id,
                                               int request_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  resource_dispatcher_host_->CancelRequest(render_process_id,
                                           request_id,
                                           false);
}

// Notifications sent from the UI thread and run on the file thread.

// This method will be sent via a user action, or shutdown on the UI thread,
// and run on the file thread. We don't post a message back for cancels,
// but we do forward the cancel to the IO thread. Since this message has been
// sent from the UI thread, the saving job may have already completed and
// won't exist in our map.
void SaveFileManager::CancelSave(int save_id) {
  DCHECK(MessageLoop::current() == GetSaveLoop());
  SaveFileMap::iterator it = save_file_map_.find(save_id);
  if (it != save_file_map_.end()) {
    SaveFile* save_file = it->second;

    // If the data comes from the net IO thread, then forward the cancel
    // message to IO thread. If the data comes from other sources, just
    // ignore the cancel message.
    if (save_file->save_source() == SaveFileCreateInfo::SAVE_FILE_FROM_NET) {
      ui_loop_->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &SaveFileManager::OnCancelSaveRequest,
                            save_file->render_process_id(),
                            save_file->request_id()));

      // UI thread will notify the render process to stop sending data,
      // so in here, we need not to do anything, just close the save file.
      save_file->Cancel();
    } else {
      // If we did not find SaveFile in map, the saving job should either get
      // data from other sources or have finished.
      DCHECK(save_file->save_source() !=
             SaveFileCreateInfo::SAVE_FILE_FROM_NET ||
             !save_file->in_progress());
    }
    // Whatever the save file is renamed or not, just delete it.
    save_file_map_.erase(it);
    delete save_file;
  }
}

// It is possible that SaveItem which has specified save_id has been canceled
// before this function runs. So if we can not find corresponding SaveFile by
// using specified save_id, just return.
void SaveFileManager::SaveLocalFile(const std::wstring& original_file_url,
                                    int save_id,
                                    int render_process_id) {
  DCHECK(MessageLoop::current() == GetSaveLoop());
  SaveFile* save_file = LookupSaveFile(save_id);
  if (!save_file)
    return;
  DCHECK(!save_file->path_renamed());
  // If it has finished, just return.
  if (!save_file->in_progress())
    return;

  // Close the save file before the copy operation.
  save_file->Finish();

  GURL file_url(original_file_url);
  DCHECK(file_url.SchemeIsFile());
  std::wstring file_path;
  net::FileURLToFilePath(file_url, &file_path);
  // If we can not get valid file path from original URL, treat it as
  // disk error.
  if (file_path.empty())
    SaveFinished(save_id, original_file_url, render_process_id, false);

  // Copy the local file to the temporary file. It will be renamed to its
  // final name later.
  bool success = CopyFile(file_path.c_str(),
                          save_file->full_path().c_str(), FALSE) != 0;
  if (!success)
    file_util::Delete(save_file->full_path(), false);
  SaveFinished(save_id, original_file_url, render_process_id, success);
}

void SaveFileManager::OnDeleteDirectoryOrFile(const std::wstring& full_path,
                                              bool is_dir) {
  DCHECK(MessageLoop::current() == GetSaveLoop());
  DCHECK(!full_path.empty());

  file_util::Delete(full_path, is_dir);
}

// Open a saved page package, show it in a Windows Explorer window.
// We run on this thread to avoid blocking the UI with slow Shell operations.
void SaveFileManager::OnShowSavedFileInShell(const std::wstring full_path) {
  DCHECK(MessageLoop::current() == GetSaveLoop());
  win_util::ShowItemInFolder(full_path);
}

void SaveFileManager::RenameAllFiles(
    const FinalNameList& final_names,
    const std::wstring& resource_dir,
    int render_process_id,
    int render_view_id) {
  DCHECK(MessageLoop::current() == GetSaveLoop());

  if (!resource_dir.empty() && !file_util::PathExists(resource_dir))
    file_util::CreateDirectory(resource_dir);

  for (FinalNameList::const_iterator i = final_names.begin();
      i != final_names.end(); ++i) {
    SaveFileMap::iterator it = save_file_map_.find(i->first);
    if (it != save_file_map_.end()) {
      SaveFile* save_file = it->second;
      DCHECK(!save_file->in_progress());
      save_file->Rename(i->second);
      delete save_file;
      save_file_map_.erase(it);
    }
  }

  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &SaveFileManager::OnFinishSavePageJob,
                        render_process_id,
                        render_view_id));
}

void SaveFileManager::OnFinishSavePageJob(int render_process_id,
                                          int render_view_id) {
  DCHECK(MessageLoop::current() == ui_loop_);

  SavePackage* save_package =
      GetSavePackageFromRenderIds(render_process_id, render_view_id);

  if (save_package) {
    // save_package is null if save was canceled.
    save_package->Finish();
  }
}

void SaveFileManager::RemoveSavedFileFromFileMap(
    const SaveIDList& save_ids) {
  DCHECK(MessageLoop::current() == GetSaveLoop());

  for (SaveIDList::const_iterator i = save_ids.begin();
      i != save_ids.end(); ++i) {
    SaveFileMap::iterator it = save_file_map_.find(*i);
    if (it != save_file_map_.end()) {
      SaveFile* save_file = it->second;
      DCHECK(!save_file->in_progress());
      DeleteFile(save_file->full_path().c_str());
      delete save_file;
      save_file_map_.erase(it);
    }
  }
}

