// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/save_package.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/save_file.h"
#include "chrome/browser/download/save_file_manager.h"
#include "chrome/browser/download/save_page_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/common/win_util.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/dom_serializer_delegate.h"

#include "generated_resources.h"

using base::Time;

namespace {

// Default name which will be used when we can not get proper name from
// resource URL.
const wchar_t kDefaultSaveName[] = L"saved_resource";

// Maximum number of file ordinal number. I think it's big enough for resolving
// name-conflict files which has same base file name.
const int32 kMaxFileOrdinalNumber = 9999;

// Maximum length for file path. Since Windows have MAX_PATH limitation for
// file path, we need to make sure length of file path of every saved file
// is less than MAX_PATH
const uint32 kMaxFilePathLength = MAX_PATH - 1;

// Maximum length for file ordinal number part. Since we only support the
// maximum 9999 for ordinal number, which means maximum file ordinal number part
// should be "(9998)", so the value is 6.
const uint32 kMaxFileOrdinalNumberPartLength = 6;

// If false, we don't prompt the user as to where to save the file.  This
// exists only for testing.
bool g_should_prompt_for_filename = true;

}  // namespace

SavePackage::SavePackage(WebContents* web_content,
                         SavePackageType save_type,
                         const std::wstring& file_full_path,
                         const std::wstring& directory_full_path)
    : web_contents_(web_content),
      save_type_(save_type),
      saved_main_file_path_(file_full_path),
      saved_main_directory_path_(directory_full_path),
      all_save_items_count_(0),
      disk_error_occurred_(false),
      user_canceled_(false),
      download_(NULL),
      finished_(false),
      wait_state_(INITIALIZE),
      tab_id_(web_content->process()->host_id()) {
  DCHECK(web_content);
  const GURL& current_page_url = web_contents_->GetURL();
  DCHECK(current_page_url.is_valid());
  page_url_ = UTF8ToWide(current_page_url.spec());
  DCHECK(save_type_ == SAVE_AS_ONLY_HTML ||
         save_type_ == SAVE_AS_COMPLETE_HTML);
  DCHECK(!saved_main_file_path_.empty() &&
         saved_main_file_path_.length() <= kMaxFilePathLength);
  DCHECK(!saved_main_directory_path_.empty() &&
         saved_main_directory_path_.length() < kMaxFilePathLength);
}

// This is for testing use. Set |finished_| as true because we don't want
// method Cancel to be be called in destructor in test mode.
SavePackage::SavePackage(const wchar_t* file_full_path,
                         const wchar_t* directory_full_path)
    : all_save_items_count_(0),
      saved_main_file_path_(file_full_path),
      saved_main_directory_path_(directory_full_path),
      finished_(true),
      download_(NULL),
      user_canceled_(false),
      disk_error_occurred_(false),
      tab_id_(0) {
  DCHECK(!saved_main_file_path_.empty() &&
         saved_main_file_path_.length() <= kMaxFilePathLength);
  DCHECK(!saved_main_directory_path_.empty() &&
         saved_main_directory_path_.length() < kMaxFilePathLength);
}

SavePackage::~SavePackage() {
  // Stop receiving saving job's updates
  if (!finished_ && !canceled()) {
    // Unexpected quit.
    Cancel(true);
  }

  DCHECK(all_save_items_count_ == (waiting_item_queue_.size() +
                                   completed_count() +
                                   in_process_count()));
  // Free all SaveItems.
  while (!waiting_item_queue_.empty()) {
    // We still have some items which are waiting for start to save.
    SaveItem* save_item = waiting_item_queue_.front();
    waiting_item_queue_.pop();
    delete save_item;
  }

  STLDeleteValues(&saved_success_items_);
  STLDeleteValues(&in_progress_items_);
  STLDeleteValues(&saved_failed_items_);

  if (download_) {
    // We call this to remove the view from the shelf. It will invoke
    // DownloadManager::RemoveDownload, but since the fake DownloadItem is not
    // owned by DownloadManager, it will do nothing to our fake item.
    download_->Remove(false);
    delete download_;
    download_ = NULL;
  }
  file_manager_ = NULL;
}

// Cancel all in progress request, might be called by user or internal error.
void SavePackage::Cancel(bool user_action) {
  if (!canceled()) {
    if (user_action)
      user_canceled_ = true;
    else
      disk_error_occurred_ = true;
    Stop();
  }
}

// Initialize the SavePackage.
bool SavePackage::Init() {
  // Set proper running state.
  if (wait_state_ != INITIALIZE)
    return false;

  wait_state_ = START_PROCESS;

  // Initialize the request context and resource dispatcher.
  Profile* profile = web_contents_->profile();
  if (!profile) {
    NOTREACHED();
    return false;
  }

  request_context_ = profile->GetRequestContext();

  ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
  if (!rdh) {
    NOTREACHED();
    return false;
  }

  file_manager_ = rdh->save_file_manager();
  if (!file_manager_) {
    NOTREACHED();
    return false;
  }

  // Create the fake DownloadItem and display the view.
  download_ = new DownloadItem(1,
      FilePath::FromWStringHack(saved_main_file_path_), 0, page_url_,
      FilePath(), Time::Now(), 0, -1, -1, false);
  download_->set_manager(web_contents_->profile()->GetDownloadManager());
  DownloadShelfView* shelf = web_contents_->GetDownloadShelfView();
  shelf->AddDownloadView(new DownloadItemView(
      download_, shelf, new SavePageModel(this, download_)));
  web_contents_->SetDownloadShelfVisible(true);

  // Check save type and process the save page job.
  if (save_type_ == SAVE_AS_COMPLETE_HTML) {
    // Get directory
    DCHECK(!saved_main_directory_path_.empty());
    GetAllSavableResourceLinksForCurrentPage();
  } else {
    wait_state_ = NET_FILES;
    GURL u(page_url_);
    SaveFileCreateInfo::SaveFileSource save_source = u.SchemeIsFile() ?
        SaveFileCreateInfo::SAVE_FILE_FROM_FILE :
        SaveFileCreateInfo::SAVE_FILE_FROM_NET;
    SaveItem* save_item = new SaveItem(page_url_,
                                       L"",
                                       this,
                                       save_source);
    // Add this item to waiting list.
    waiting_item_queue_.push(save_item);
    all_save_items_count_ = 1;
    download_->set_total_bytes(1);

    DoSavingProcess();
  }

  return true;
}

// Generate name for saving resource.
bool SavePackage::GenerateFilename(const std::string& disposition,
                                   const std::wstring& url,
                                   bool need_html_ext,
                                   std::wstring* generated_name) {
  std::wstring file_name =
      net::GetSuggestedFilename(GURL(url), disposition, kDefaultSaveName);

  DCHECK(!file_name.empty());
  // Check whether we have same name before.
  std::wstring::size_type last_dot = file_name.rfind(L'.');
  std::wstring pure_file_name, file_name_ext;
  if (last_dot == std::wstring::npos) {
    pure_file_name = file_name;
  } else {
    pure_file_name = std::wstring(file_name, 0, last_dot);
    file_name_ext = std::wstring(file_name, last_dot);
  }
  // If it is HTML resource, use ".htm" as its extension name.
  if (need_html_ext)
    file_name_ext = L".htm";
  if (file_name_ext == L".")
    file_name_ext.clear();

  // Get safe pure file name.
  if (!GetSafePureFileName(saved_main_directory_path_, file_name_ext,
                           kMaxFilePathLength, &pure_file_name))
    return false;

  file_name = pure_file_name + file_name_ext;

  // Check whether we already have same name.
  if (file_name_set_.find(file_name) == file_name_set_.end()) {
    file_name_set_.insert(file_name);
  } else {
    // Found same name, increase the ordinal number for the file name.
    std::wstring base_file_name, file_ordinal_number;

    if (!GetBaseFileNameAndFileOrdinalNumber(pure_file_name, &base_file_name,
                                             &file_ordinal_number))
      base_file_name = pure_file_name;

    // We need to make sure the length of base file name plus maximum ordinal
    // number path will be less than or equal to kMaxFilePathLength.
    if (!GetSafePureFileName(saved_main_directory_path_, file_name_ext,
        kMaxFilePathLength - kMaxFileOrdinalNumberPartLength, &base_file_name))
      return false;

    // Prepare the new ordinal number.
    uint32 ordinal_number;
    FileNameCountMap::iterator it = file_name_count_map_.find(base_file_name);
    if (it == file_name_count_map_.end()) {
      // First base-name-conflict resolving, use 1 as initial ordinal number.
      file_name_count_map_[base_file_name] = 1;
      ordinal_number = 1;
    } else {
      // We have met same base-name conflict, use latest ordinal number.
      ordinal_number = it->second;
    }

    if (ordinal_number > (kMaxFileOrdinalNumber - 1)) {
      // Use a random file from temporary file.
      file_util::CreateTemporaryFileName(&file_name);
      file_name = file_util::GetFilenameFromPath(file_name);
      // Get safe pure file name.
      if (!GetSafePureFileName(saved_main_directory_path_, std::wstring(),
                               kMaxFilePathLength, &file_name))
        return false;
    } else {
      uint32 i;
      for (i = ordinal_number; i < kMaxFileOrdinalNumber; ++i) {
        std::wstring new_name =
            StringPrintf(L"%ls(%d)", base_file_name.c_str(), i) + file_name_ext;
        if (file_name_set_.find(new_name) == file_name_set_.end()) {
          // Resolved name conflict.
          file_name = new_name;
          file_name_count_map_[base_file_name] = ++i;
          break;
        }
      }
    }

    file_name_set_.insert(file_name);
  }

  DCHECK(!file_name.empty());
  generated_name->assign(file_name);

  return true;
}

// We have received a message from SaveFileManager about a new saving job. We
// create a SaveItem and store it in our in_progress list.
void SavePackage::StartSave(const SaveFileCreateInfo* info) {
  DCHECK(info && !info->url.empty());

  SaveUrlItemMap::iterator it = in_progress_items_.find(info->url);
  if (it == in_progress_items_.end()) {
    // If not found, we must have cancel action.
    DCHECK(canceled());
    return;
  }
  SaveItem* save_item = it->second;

  DCHECK(!saved_main_file_path_.empty());

  save_item->SetSaveId(info->save_id);
  save_item->SetTotalBytes(info->total_bytes);

  // Determine the proper path for a saving job, by choosing either the default
  // save directory, or prompting the user.
  DCHECK(!save_item->has_final_name());
  if (info->url != page_url_) {
    std::wstring generated_name;
    // For HTML resource file, make sure it will have .htm as extension name,
    // otherwise, when you open the saved page in Chrome again, download
    // file manager will treat it as downloadable resource, and download it
    // instead of opening it as HTML.
    bool need_html_ext =
        info->save_source == SaveFileCreateInfo::SAVE_FILE_FROM_DOM;
    if (!GenerateFilename(info->content_disposition,
                          info->url,
                          need_html_ext,
                          &generated_name)) {
      // We can not generate file name for this SaveItem, so we cancel the
      // saving page job if the save source is from serialized DOM data.
      // Otherwise, it means this SaveItem is sub-resource type, we treat it
      // as an error happened on saving. We can ignore this type error for
      // sub-resource links which will be resolved as absolute links instead
      // of local links in final saved contents.
      if (info->save_source == SaveFileCreateInfo::SAVE_FILE_FROM_DOM)
        Cancel(true);
      else
        SaveFinished(save_item->save_id(), 0, false);
      return;
    }

    // When saving page as only-HTML, we only have a SaveItem whose url
    // must be page_url_.
    DCHECK(save_type_ == SAVE_AS_COMPLETE_HTML);
    DCHECK(!saved_main_directory_path_.empty());

    // Now we get final name retrieved from GenerateFilename, we will use it
    // rename the SaveItem.
    std::wstring final_name = saved_main_directory_path_;
    file_util::AppendToPath(&final_name, generated_name);
    save_item->Rename(final_name);
  } else {
    // It is the main HTML file, use the name chosen by the user.
    save_item->Rename(saved_main_file_path_);
  }

  // If the save source is from file system, inform SaveFileManager to copy
  // corresponding file to the file path which this SaveItem specifies.
  if (info->save_source == SaveFileCreateInfo::SAVE_FILE_FROM_FILE) {
    file_manager_->GetSaveLoop()->PostTask(FROM_HERE,
        NewRunnableMethod(file_manager_,
                          &SaveFileManager::SaveLocalFile,
                          save_item->url(),
                          save_item->save_id(),
                          tab_id()));
    return;
  }

  // Check whether we begin to require serialized HTML data.
  if (save_type_ == SAVE_AS_COMPLETE_HTML && wait_state_ == HTML_DATA) {
    // Inform backend to serialize the all frames' DOM and send serialized
    // HTML data back.
    GetSerializedHtmlDataForCurrentPageWithLocalLinks();
  }
}

// Look up SaveItem by save id from in progress map.
SaveItem* SavePackage::LookupItemInProcessBySaveId(int32 save_id) {
  if (in_process_count()) {
    SaveUrlItemMap::iterator it = in_progress_items_.begin();
    for (; it != in_progress_items_.end(); ++it) {
      SaveItem* save_item = it->second;
      DCHECK(save_item->state() == SaveItem::IN_PROGRESS);
      if (save_item->save_id() == save_id)
        return save_item;
    }
  }
  return NULL;
}

// Remove SaveItem from in progress map and put it to saved map.
void SavePackage::PutInProgressItemToSavedMap(SaveItem* save_item) {
  SaveUrlItemMap::iterator it = in_progress_items_.find(
      save_item->url());
  DCHECK(it != in_progress_items_.end());
  DCHECK(save_item == it->second);
  in_progress_items_.erase(it);

  if (save_item->success()) {
    // Add it to saved_success_items_.
    DCHECK(saved_success_items_.find(save_item->save_id()) ==
           saved_success_items_.end());
    saved_success_items_[save_item->save_id()] = save_item;
  } else {
    // Add it to saved_failed_items_.
    DCHECK(saved_failed_items_.find(save_item->url()) ==
           saved_failed_items_.end());
    saved_failed_items_[save_item->url()] = save_item;
  }
}

// Called for updating saving state.
bool SavePackage::UpdateSaveProgress(int32 save_id,
                                     int64 size,
                                     bool write_success) {
  // Because we might have canceled this saving job before,
  // so we might not find corresponding SaveItem.
  SaveItem* save_item = LookupItemInProcessBySaveId(save_id);
  if (!save_item)
    return false;

  save_item->Update(size);

  // If we got disk error, cancel whole save page job.
  if (!write_success) {
    // Cancel job with reason of disk error.
    Cancel(false);
  }
  return true;
}

// Stop all page saving jobs that are in progress and instruct the file thread
// to delete all saved  files.
void SavePackage::Stop() {
  // When stopping, if it still has some items in in_progress, cancel them.
  DCHECK(canceled());
  if (in_process_count()) {
    SaveUrlItemMap::iterator it = in_progress_items_.begin();
    for (; it != in_progress_items_.end(); ++it) {
      SaveItem* save_item = it->second;
      DCHECK(save_item->state() == SaveItem::IN_PROGRESS);
      save_item->Cancel();
    }
    // Remove all in progress item to saved map. For failed items, they will
    // be put into saved_failed_items_, for successful item, they will be put
    // into saved_success_items_.
    while (in_process_count())
      PutInProgressItemToSavedMap(in_progress_items_.begin()->second);
  }

  // This vector contains the save ids of the save files which SaveFileManager
  // needs to remove from its save_file_map_.
  SaveIDList save_ids;
  for (SavedItemMap::iterator it = saved_success_items_.begin();
      it != saved_success_items_.end(); ++it)
    save_ids.push_back(it->first);
  for (SaveUrlItemMap::iterator it = saved_failed_items_.begin();
      it != saved_failed_items_.end(); ++it)
    save_ids.push_back(it->second->save_id());

  file_manager_->GetSaveLoop()->PostTask(FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &SaveFileManager::RemoveSavedFileFromFileMap,
                        save_ids));

  finished_ = true;
  wait_state_ = FAILED;

  // Inform the DownloadItem we have canceled whole save page job.
  DCHECK(download_);
  download_->Cancel(false);
}

void SavePackage::CheckFinish() {
  if (in_process_count() || finished_)
    return;

  std::wstring dir = save_type_ == SAVE_AS_COMPLETE_HTML ?
                     saved_main_directory_path_ :
                     L"";

  // This vector contains the final names of all the successfully saved files
  // along with their save ids. It will be passed to SaveFileManager to do the
  // renaming job.
  FinalNameList final_names;
  for (SavedItemMap::iterator it = saved_success_items_.begin();
      it != saved_success_items_.end(); ++it)
    final_names.push_back(std::make_pair(it->first,
                                         it->second->full_path()));

  file_manager_->GetSaveLoop()->PostTask(FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &SaveFileManager::RenameAllFiles,
                        final_names,
                        dir,
                        web_contents_->process()->host_id(),
                        web_contents_->render_view_host()->routing_id()));
}

// Successfully finished all items of this SavePackage.
void SavePackage::Finish() {
  // User may cancel the job when we're moving files to the final directory.
  if (canceled())
    return;

  wait_state_ = SUCCESSFUL;
  finished_ = true;

  // This vector contains the save ids of the save files which SaveFileManager
  // needs to remove from its save_file_map_.
  SaveIDList save_ids;
  for (SaveUrlItemMap::iterator it = saved_failed_items_.begin();
       it != saved_failed_items_.end(); ++it)
    save_ids.push_back(it->second->save_id());

  file_manager_->GetSaveLoop()->PostTask(FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &SaveFileManager::RemoveSavedFileFromFileMap,
                        save_ids));

  DCHECK(download_);
  download_->Finished(all_save_items_count_);
}

// Called for updating end state.
void SavePackage::SaveFinished(int32 save_id, int64 size, bool is_success) {
  // Because we might have canceled this saving job before,
  // so we might not find corresponding SaveItem. Just ignore it.
  SaveItem* save_item = LookupItemInProcessBySaveId(save_id);
  if (!save_item)
    return;

  // Let SaveItem set end state.
  save_item->Finish(size, is_success);
  // Remove the associated save id and SavePackage.
  file_manager_->RemoveSaveFile(save_id, save_item->url(), this);

  PutInProgressItemToSavedMap(save_item);

  // Inform the DownloadItem to update UI.
  DCHECK(download_);
  // We use the received bytes as number of saved files.
  download_->Update(completed_count());

  if (save_item->save_source() == SaveFileCreateInfo::SAVE_FILE_FROM_DOM &&
      save_item->url() == page_url_ && !save_item->received_bytes()) {
    // If size of main HTML page is 0, treat it as disk error.
    Cancel(false);
    return;
  }

  if (canceled()) {
    DCHECK(finished_);
    return;
  }

  // Continue processing the save page job.
  DoSavingProcess();

  // Check whether we can successfully finish whole job.
  CheckFinish();
}

// Sometimes, the net io will only call SaveFileManager::SaveFinished with
// save id -1 when it encounters error. Since in this case, save id will be
// -1, so we can only use URL to find which SaveItem is associated with
// this error.
// Saving an item failed. If it's a sub-resource, ignore it. If the error comes
// from serializing HTML data, then cancel saving page.
void SavePackage::SaveFailed(const std::wstring& save_url) {
  SaveUrlItemMap::iterator it = in_progress_items_.find(save_url);
  if (it == in_progress_items_.end()) {
    NOTREACHED();  // Should not exist!
    return;
  }
  SaveItem* save_item = it->second;

  save_item->Finish(0, false);

  PutInProgressItemToSavedMap(save_item);

  // Inform the DownloadItem to update UI.
  DCHECK(download_);
  // We use the received bytes as number of saved files.
  download_->Update(completed_count());

  if (save_type_ == SAVE_AS_ONLY_HTML ||
      save_item->save_source() == SaveFileCreateInfo::SAVE_FILE_FROM_DOM) {
    // We got error when saving page. Treat it as disk error.
    Cancel(true);
  }

  if (canceled()) {
    DCHECK(finished_);
    return;
  }

  // Continue processing the save page job.
  DoSavingProcess();

  CheckFinish();
}

void SavePackage::SaveCanceled(SaveItem* save_item) {
  // Call the RemoveSaveFile in UI thread.
  file_manager_->RemoveSaveFile(save_item->save_id(),
                                save_item->url(),
                                this);
  if (save_item->save_id() != -1)
    file_manager_->GetSaveLoop()->PostTask(FROM_HERE,
        NewRunnableMethod(file_manager_,
                          &SaveFileManager::CancelSave,
                          save_item->save_id()));
}

// Initiate a saving job of a specific URL. We send the request to
// SaveFileManager, which will dispatch it to different approach according to
// the save source. Parameter process_all_remaining_items indicates whether
// we need to save all remaining items.
void SavePackage::SaveNextFile(bool process_all_remaining_items) {
  DCHECK(web_contents_);
  DCHECK(waiting_item_queue_.size());

  do {
    // Pop SaveItem from waiting list.
    SaveItem* save_item = waiting_item_queue_.front();
    waiting_item_queue_.pop();

    // Add the item to in_progress_items_.
    SaveUrlItemMap::iterator it = in_progress_items_.find(
        save_item->url());
    DCHECK(it == in_progress_items_.end());
    in_progress_items_[save_item->url()] = save_item;
    save_item->Start();
    file_manager_->SaveURL(save_item->url(),
                           save_item->referrer(),
                           web_contents_->process()->host_id(),
                           web_contents_->render_view_host()->routing_id(),
                           save_item->save_source(),
                           save_item->full_path(),
                           request_context_.get(),
                           this);
  } while (process_all_remaining_items && waiting_item_queue_.size());
}


// Open download page in windows explorer on file thread, to avoid blocking the
// user interface.
void SavePackage::ShowDownloadInShell() {
  DCHECK(file_manager_);
  DCHECK(finished_ && !canceled() && !saved_main_file_path_.empty());
  file_manager_->GetSaveLoop()->PostTask(FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &SaveFileManager::OnShowSavedFileInShell,
                        saved_main_file_path_));
}

// Calculate the percentage of whole save page job.
int SavePackage::PercentComplete() {
  if (!all_save_items_count_)
    return 0;
  else if (!in_process_count())
    return 100;
  else
    return completed_count() / all_save_items_count_;
}

// Continue processing the save page job after one SaveItem has been
// finished.
void SavePackage::DoSavingProcess() {
  if (save_type_ == SAVE_AS_COMPLETE_HTML) {
    // We guarantee that images and JavaScripts must be downloaded first.
    // So when finishing all those sub-resources, we will know which
    // sub-resource's link can be replaced with local file path, which
    // sub-resource's link need to be replaced with absolute URL which
    // point to its internet address because it got error when saving its data.
    SaveItem* save_item = NULL;
    // Start a new SaveItem job if we still have job in waiting queue.
    if (waiting_item_queue_.size()) {
      DCHECK(wait_state_ == NET_FILES);
      save_item = waiting_item_queue_.front();
      if (save_item->save_source() != SaveFileCreateInfo::SAVE_FILE_FROM_DOM) {
        SaveNextFile(false);
      } else if (!in_process_count()) {
        // If there is no in-process SaveItem, it means all sub-resources
        // have been processed. Now we need to start serializing HTML DOM
        // for the current page to get the generated HTML data.
        wait_state_ = HTML_DATA;
        // All non-HTML resources have been finished, start all remaining
        // HTML files.
        SaveNextFile(true);
      }
    } else if (in_process_count()) {
      // Continue asking for HTML data.
      DCHECK(wait_state_ == HTML_DATA);
    }
  } else {
    // Save as HTML only.
    DCHECK(wait_state_ == NET_FILES);
    DCHECK(save_type_ == SAVE_AS_ONLY_HTML);
    if (waiting_item_queue_.size()) {
      DCHECK(all_save_items_count_ == waiting_item_queue_.size());
      SaveNextFile(false);
    }
  }
}

// After finishing all SaveItems which need to get data from net.
// We collect all URLs which have local storage and send the
// map:(originalURL:currentLocalPath) to render process (backend).
// Then render process will serialize DOM and send data to us.
void SavePackage::GetSerializedHtmlDataForCurrentPageWithLocalLinks() {
  if (wait_state_ != HTML_DATA)
    return;
  std::vector<std::wstring> saved_links;
  std::vector<std::wstring> saved_file_paths;
  int successful_started_items_count = 0;

  // Collect all saved items which have local storage.
  // First collect the status of all the resource files and check whether they
  // have created local files although they have not been completely saved.
  // If yes, the file can be saved. Otherwise, there is a disk error, so we
  // need to cancel the page saving job.
  for (SaveUrlItemMap::iterator it = in_progress_items_.begin();
       it != in_progress_items_.end(); ++it) {
    DCHECK(it->second->save_source() ==
           SaveFileCreateInfo::SAVE_FILE_FROM_DOM);
    if (it->second->has_final_name())
      successful_started_items_count++;
    saved_links.push_back(it->second->url());
    saved_file_paths.push_back(it->second->file_name());
  }

  // If not all file of HTML resource have been started, then wait.
  if (successful_started_items_count != in_process_count())
    return;

  // Collect all saved success items.
  for (SavedItemMap::iterator it = saved_success_items_.begin();
       it != saved_success_items_.end(); ++it) {
    DCHECK(it->second->has_final_name());
    saved_links.push_back(it->second->url());
    saved_file_paths.push_back(it->second->file_name());
  }

  // Get the relative directory name.
  std::wstring::size_type last_slash = saved_main_directory_path_.rfind(L'\\');
  DCHECK(last_slash != std::wstring::npos);
  std::wstring relative_dir_name = std::wstring(saved_main_directory_path_,
                                                last_slash + 1);

  relative_dir_name = std::wstring(L"./") + relative_dir_name + L"/";

  web_contents_->render_view_host()->
      GetSerializedHtmlDataForCurrentPageWithLocalLinks(
          saved_links, saved_file_paths, relative_dir_name);
}

// Process the serialized HTML content data of a specified web page
// retrieved from render process.
void SavePackage::OnReceivedSerializedHtmlData(const GURL& frame_url,
                                               const std::string& data,
                                               int32 status) {
  webkit_glue::DomSerializerDelegate::PageSavingSerializationStatus flag =
      static_cast<webkit_glue::DomSerializerDelegate::PageSavingSerializationStatus>
          (status);
  // Check current state.
  if (wait_state_ != HTML_DATA)
    return;

  int id = tab_id();
  // If the all frames are finished saving, we need to close the
  // remaining SaveItems.
  if (flag == webkit_glue::DomSerializerDelegate::ALL_FRAMES_ARE_FINISHED) {
    for (SaveUrlItemMap::iterator it = in_progress_items_.begin();
         it != in_progress_items_.end(); ++it) {
      file_manager_->GetSaveLoop()->PostTask(FROM_HERE,
          NewRunnableMethod(file_manager_,
                            &SaveFileManager::SaveFinished,
                            it->second->save_id(),
                            it->second->url(),
                            id,
                            true));
    }
    return;
  }

  std::wstring current_frame_url = UTF8ToWide(frame_url.spec());
  SaveUrlItemMap::iterator it = in_progress_items_.find(current_frame_url);
  if (it == in_progress_items_.end())
    return;
  SaveItem* save_item = it->second;
  DCHECK(save_item->save_source() == SaveFileCreateInfo::SAVE_FILE_FROM_DOM);

  if (!data.empty()) {
    // Prepare buffer for saving HTML data.
    char* new_data = static_cast<char*>(new char[data.size()]);
    memcpy(new_data, data.data(), data.size());

    // Call write file functionality in file thread.
    file_manager_->GetSaveLoop()->PostTask(FROM_HERE,
        NewRunnableMethod(file_manager_,
                          &SaveFileManager::UpdateSaveProgress,
                          save_item->save_id(),
                          new_data,
                          static_cast<int>(data.size())));
  }

  // Current frame is completed saving, call finish in file thread.
  if (flag == webkit_glue::DomSerializerDelegate::CURRENT_FRAME_IS_FINISHED) {
    file_manager_->GetSaveLoop()->PostTask(FROM_HERE,
        NewRunnableMethod(file_manager_,
                          &SaveFileManager::SaveFinished,
                          save_item->save_id(),
                          save_item->url(),
                          id,
                          true));
  }
}

// Ask for all savable resource links from backend, include main frame and
// sub-frame.
void SavePackage::GetAllSavableResourceLinksForCurrentPage() {
  if (wait_state_ != START_PROCESS)
    return;

  wait_state_ = RESOURCES_LIST;
  GURL main_page_url(page_url_);
  web_contents_->render_view_host()->
      GetAllSavableResourceLinksForCurrentPage(main_page_url);
}

// Give backend the lists which contain all resource links that have local
// storage, after which, render process will serialize DOM for generating
// HTML data.
void SavePackage::OnReceivedSavableResourceLinksForCurrentPage(
    const std::vector<GURL>& resources_list,
    const std::vector<GURL>& referrers_list,
    const std::vector<GURL>& frames_list) {
  if (wait_state_ != RESOURCES_LIST)
    return;

  DCHECK(resources_list.size() == referrers_list.size());
  all_save_items_count_ = static_cast<int>(resources_list.size()) +
                           static_cast<int>(frames_list.size());

  // We use total bytes as the total number of files we want to save.
  download_->set_total_bytes(all_save_items_count_);

  if (all_save_items_count_) {
    // Put all sub-resources to wait list.
    for (int i = 0; i < static_cast<int>(resources_list.size()); ++i) {
      const GURL& u = resources_list[i];
      DCHECK(u.is_valid());
      SaveFileCreateInfo::SaveFileSource save_source = u.SchemeIsFile() ?
          SaveFileCreateInfo::SAVE_FILE_FROM_FILE :
          SaveFileCreateInfo::SAVE_FILE_FROM_NET;
      SaveItem* save_item = new SaveItem(UTF8ToWide(u.spec()),
          UTF8ToWide(referrers_list[i].spec()), this, save_source);
      waiting_item_queue_.push(save_item);
    }
    // Put all HTML resources to wait list.
    for (int i = 0; i < static_cast<int>(frames_list.size()); ++i) {
      const GURL& u = frames_list[i];
      DCHECK(u.is_valid());
      SaveItem* save_item = new SaveItem(UTF8ToWide(u.spec()), L"",
          this, SaveFileCreateInfo::SAVE_FILE_FROM_DOM);
      waiting_item_queue_.push(save_item);
    }
    wait_state_ = NET_FILES;
    DoSavingProcess();
  } else {
    // No resource files need to be saved, treat it as user cancel.
    Cancel(true);
  }
}

void SavePackage::SetShouldPromptUser(bool should_prompt) {
  g_should_prompt_for_filename = should_prompt;
}

std::wstring SavePackage::GetSuggestNameForSaveAs(PrefService* prefs,
                                                  const std::wstring& name) {
  // Check whether the preference has the preferred directory for saving file.
  // If not, initialize it with default directory.
  if (!prefs->IsPrefRegistered(prefs::kSaveFileDefaultDirectory)) {
    std::wstring default_save_path;
    if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                          &default_save_path))
      NOTREACHED();
    prefs->RegisterStringPref(prefs::kSaveFileDefaultDirectory,
                              default_save_path);
  }

  // Get the directory from preference.
  StringPrefMember save_file_path;
  save_file_path.Init(prefs::kSaveFileDefaultDirectory, prefs, NULL);
  DCHECK(!(*save_file_path).empty());

  // Ask user for getting final saving name.
  std::wstring suggest_name, file_name;

  file_name = name;
  file_util::ReplaceIllegalCharacters(&file_name, L' ');
  suggest_name = *save_file_path;
  file_util::AppendToPath(&suggest_name, file_name);

  return suggest_name;
}

// Static.
bool SavePackage::GetSaveInfo(const std::wstring& suggest_name,
                              HWND container_hwnd,
                              SavePackageParam* param,
                              DownloadManager* download_manager) {
  // TODO(tc): It might be nice to move this code into the download
  // manager.  http://crbug.com/6025

  // Use "Web Page, Complete" option as default choice of saving page.
  unsigned index = 2;

  // If the contents can not be saved as complete-HTML, do not show the
  // file filters.
  if (CanSaveAsComplete(param->current_tab_mime_type)) {
    // Create filter string.
    std::wstring filter = l10n_util::GetString(IDS_SAVE_PAGE_FILTER);
    filter.resize(filter.size() + 2);
    filter[filter.size() - 1] = L'\0';
    filter[filter.size() - 2] = L'\0';

    if (g_should_prompt_for_filename) {
      if (!win_util::SaveFileAsWithFilter(container_hwnd,
                                          suggest_name,
                                          filter,
                                          L"htm",
                                          &index,
                                          &param->saved_main_file_path))
        return false;
    } else {
      param->saved_main_file_path = suggest_name;
    }
  } else {
    if (g_should_prompt_for_filename) {
      if (!win_util::SaveFileAs(container_hwnd, suggest_name,
                                &param->saved_main_file_path))
        return false;
    } else {
      param->saved_main_file_path = suggest_name;
    }
    // Set save-as type to only-HTML if the contents of current tab can not be
    // saved as complete-HTML.
    index = 1;
  }

  DCHECK(download_manager);
  // Ensure the filename is safe.
  FilePath path(param->saved_main_file_path);
  download_manager->GenerateSafeFilename(param->current_tab_mime_type, &path);
  param->saved_main_file_path = path.ToWStringHack();

  // The option index is not zero-based.
  DCHECK(index > 0 && index < 3);
  param->dir = file_util::GetDirectoryFromPath(param->saved_main_file_path);

  StringPrefMember save_file_path;
  save_file_path.Init(prefs::kSaveFileDefaultDirectory, param->prefs, NULL);
  // If user change the default saving directory, we will remember it just
  // like IE and FireFox.
  if (save_file_path.GetValue() != param->dir)
    save_file_path.SetValue(param->dir);

  param->save_type = (index == 1) ? SavePackage::SAVE_AS_ONLY_HTML :
                                    SavePackage::SAVE_AS_COMPLETE_HTML;

  if (param->save_type == SavePackage::SAVE_AS_COMPLETE_HTML) {
    // Make new directory for saving complete file.
    std::wstring file_name =
        file_util::GetFilenameFromPath(param->saved_main_file_path);
    std::wstring::size_type last_dot = file_name.rfind(L'.');
    std::wstring pure_file_name;
    if (last_dot == std::wstring::npos)
      pure_file_name = file_name;
    else
      pure_file_name = std::wstring(file_name, 0, last_dot);
    pure_file_name += L"_files";
    file_util::AppendToPath(&param->dir, pure_file_name);
  }

  return true;
}

// Static.
bool SavePackage::GetBaseFileNameAndFileOrdinalNumber(
    const std::wstring& file_name,
    std::wstring* base_file_name,
    std::wstring* file_ordinal_number) {
  if (file_name.empty() || !base_file_name || !file_ordinal_number)
    return false;

  // Find dot position.
  std::wstring::size_type dot_position = file_name.rfind(L".");
  // Find position of right parenthesis.
  std::wstring::size_type parenthesis_right;
  if (std::wstring::npos == dot_position)
    parenthesis_right = file_name.rfind(L')');
  else
    parenthesis_right = dot_position - 1;
  // The latest character of pure file name is not ")", return false.
  if (std::wstring::npos == parenthesis_right)
    return false;
  if (file_name.at(parenthesis_right) != L')')
    return false;
  // Find position of left parenthesis.
  std::wstring::size_type parenthesis_left = file_name.rfind(L'(');
  if (std::wstring::npos == parenthesis_left)
    return false;

  if (parenthesis_right <= parenthesis_left)
    return false;
  // Check whether content between left parenthesis and right parenthesis is
  // numeric or not.
  std::wstring ordinal_number(file_name, parenthesis_left + 1,
                              parenthesis_right - parenthesis_left - 1);
  for (std::wstring::const_iterator cit = ordinal_number.begin();
       cit != ordinal_number.end(); ++cit)
    if (!IsAsciiDigit(*cit))
      return false;

  *base_file_name = std::wstring(file_name, 0, parenthesis_left);
  *file_ordinal_number = ordinal_number;
  return true;
}

// Static
bool SavePackage::IsSavableURL(const GURL& url) {
  return url.SchemeIs("http") || url.SchemeIs("https") ||
         url.SchemeIs("file") || url.SchemeIs("ftp");
}

// Static
bool SavePackage::IsSavableContents(const std::string& contents_mime_type) {
  // WebKit creates Document object when MIME type is application/xhtml+xml,
  // so we also support this MIME type.
  return contents_mime_type == "text/html" ||
         contents_mime_type == "text/xml" ||
         contents_mime_type == "application/xhtml+xml" ||
         contents_mime_type == "text/plain" ||
         contents_mime_type == "text/css" ||
         net::IsSupportedJavascriptMimeType(contents_mime_type.c_str());
}

// Static
bool SavePackage::CanSaveAsComplete(const std::string& contents_mime_type) {
  return contents_mime_type == "text/html";
}

// Static
bool SavePackage::GetSafePureFileName(const std::wstring& dir_path,
                                      const std::wstring& file_name_ext,
                                      uint32 max_file_path_len,
                                      std::wstring* pure_file_name) {
  DCHECK(!pure_file_name->empty());
  std::wstring final_name = dir_path;
  file_util::AppendToPath(&final_name, *pure_file_name);
  // Get total length of dir path, including ending "\".
  const std::wstring::size_type dir_path_length =
      final_name.length() - pure_file_name->length();
  // Get available length for putting dir path and pure file name.
  const std::wstring::size_type available_length =
      static_cast<std::wstring::size_type>(max_file_path_len) -
      file_name_ext.length();

  if (final_name.length() <= available_length)
    return true;

  if (available_length > dir_path_length) {
    *pure_file_name =
        pure_file_name->substr(0, available_length - dir_path_length);
    return true;
  } else {
    pure_file_name->clear();
    return false;
  }
}
