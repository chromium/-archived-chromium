// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_
#define CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_

#include <queue>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/shell_dialogs.h"
#include "googleurl/src/gurl.h"

class SaveFileManager;
class SaveItem;
class SavePackage;
class DownloadItem;
class DownloadManager;
class GURL;
class MessageLoop;
class PrefService;
class Profile;
class TabContents;
class URLRequestContext;
class TabContents;

namespace base {
class Thread;
class Time;
}

struct SaveFileCreateInfo;
struct SavePackageParam;

// The SavePackage object manages the process of saving a page as only-html or
// complete-html and providing the information for displaying saving status.
// Saving page as only-html means means that we save web page to a single HTML
// file regardless internal sub resources and sub frames.
// Saving page as complete-html page means we save not only the main html file
// the user told it to save but also a directory for the auxiliary files such
// as all sub-frame html files, image files, css files and js files.
//
// Each page saving job may include one or multiple files which need to be
// saved. Each file is represented by a SaveItem, and all SaveItems are owned
// by the SavePackage. SaveItems are created when a user initiates a page
// saving job, and exist for the duration of one tab's life time.
class SavePackage : public base::RefCountedThreadSafe<SavePackage>,
                    public RenderViewHostDelegate::Save,
                    public SelectFileDialog::Listener {
 public:
  enum SavePackageType {
    // User chose to save only the HTML of the page.
    SAVE_AS_ONLY_HTML = 0,
    // User chose to save complete-html page.
    SAVE_AS_COMPLETE_HTML = 1
  };

  enum WaitState {
    // State when created but not initialized.
    INITIALIZE = 0,
    // State when after initializing, but not yet saving.
    START_PROCESS,
    // Waiting on a list of savable resources from the backend.
    RESOURCES_LIST,
    // Waiting for data sent from net IO or from file system.
    NET_FILES,
    // Waiting for html DOM data sent from render process.
    HTML_DATA,
    // Saving page finished successfully.
    SUCCESSFUL,
    // Failed to save page.
    FAILED
  };

  // Constructor for user initiated page saving. This constructor results in a
  // SavePackage that will generate and sanitize a suggested name for the user
  // in the "Save As" dialog box.
  SavePackage(TabContents* web_content);

  // This contructor is used only for testing. We can bypass the file and
  // directory name generation / sanitization by providing well known paths
  // better suited for tests.
  SavePackage(TabContents* web_content,
              SavePackageType save_type,
              const FilePath& file_full_path,
              const FilePath& directory_full_path);

  ~SavePackage();

  // Initialize the SavePackage. Returns true if it initializes properly.
  // Need to make sure that this method must be called in the UI thread because
  // using g_browser_process on a non-UI thread can cause crashes during
  // shutdown.
  bool Init();

  void Cancel(bool user_action);

  void Finish();

  // Notifications sent from the file thread to the UI thread.
  void StartSave(const SaveFileCreateInfo* info);
  bool UpdateSaveProgress(int32 save_id, int64 size, bool write_success);
  void SaveFinished(int32 save_id, int64 size, bool is_success);
  void SaveFailed(const GURL& save_url);
  void SaveCanceled(SaveItem* save_item);

  // Rough percent complete, -1 means we don't know (since we didn't receive a
  // total size).
  int PercentComplete();

  // Show or Open a saved page via the Windows shell.
  void ShowDownloadInShell();

  bool canceled() { return user_canceled_ || disk_error_occurred_; }

  // Accessor
  bool finished() { return finished_; }
  SavePackageType save_type() { return save_type_; }

  // Since for one tab, it can only have one SavePackage in same time.
  // Now we actually use render_process_id as tab's unique id.
  int tab_id() const { return tab_id_; }

  void GetSaveInfo();
  void ContinueSave(SavePackageParam* param,
                    const FilePath& final_name,
                    int index);

  // RenderViewHostDelegate::Save ----------------------------------------------

  // Process all of the current page's savable links of subresources, resources
  // referrers and frames (including the main frame and subframes) from the
  // render view host.
  virtual void OnReceivedSavableResourceLinksForCurrentPage(
      const std::vector<GURL>& resources_list,
      const std::vector<GURL>& referrers_list,
      const std::vector<GURL>& frames_list);

  // Process the serialized html content data of a specified web page
  // gotten from render process.
  virtual void OnReceivedSerializedHtmlData(const GURL& frame_url,
                                            const std::string& data,
                                            int32 status);

  // Statics -------------------------------------------------------------------

  // Used to disable prompting the user for a directory/filename of the saved
  // web page.  This is available for testing.
  static void SetShouldPromptUser(bool should_prompt);

  // Check whether we can do the saving page operation for the specified URL.
  static bool IsSavableURL(const GURL& url);

  // Check whether we can do the saving page operation for the contents which
  // have the specified MIME type.
  static bool IsSavableContents(const std::string& contents_mime_type);

  // Check whether we can save page as complete-HTML for the contents which
  // have specified a MIME type. Now only contents which have the MIME type
  // "text/html" can be saved as complete-HTML.
  static bool CanSaveAsComplete(const std::string& contents_mime_type);

  // File name is considered being consist of pure file name, dot and file
  // extension name. File name might has no dot and file extension, or has
  // multiple dot inside file name. The dot, which separates the pure file
  // name and file extension name, is last dot in the whole file name.
  // This function is for making sure the length of specified file path is not
  // great than the specified maximum length of file path and getting safe pure
  // file name part if the input pure file name is too long.
  // The parameter |dir_path| specifies directory part of the specified
  // file path. The parameter |file_name_ext| specifies file extension
  // name part of the specified file path (including start dot). The parameter
  // |max_file_path_len| specifies maximum length of the specified file path.
  // The parameter |pure_file_name| input pure file name part of the specified
  // file path. If the length of specified file path is great than
  // |max_file_path_len|, the |pure_file_name| will output new pure file name
  // part for making sure the length of specified file path is less than
  // specified maximum length of file path. Return false if the function can
  // not get a safe pure file name, otherwise it returns true.
  static bool GetSafePureFileName(const FilePath& dir_path,
                                  const FilePath::StringType& file_name_ext,
                                  uint32 max_file_path_len,
                                  FilePath::StringType* pure_file_name);

  // SelectFileDialog::Listener interface.
  virtual void FileSelected(const FilePath& path, int index, void* params);
  virtual void FileSelectionCanceled(void* params);

 private:
  // For testing only.
  SavePackage(const FilePath& file_full_path,
              const FilePath& directory_full_path);

  void Stop();
  void CheckFinish();
  void SaveNextFile(bool process_all_remainder_items);
  void DoSavingProcess();

  // Create a file name based on the response from the server.
  bool GenerateFilename(const std::string& disposition,
                        const GURL& url,
                        bool need_html_ext,
                        FilePath::StringType* generated_name);

  // Get all savable resource links from current web page, include main
  // frame and sub-frame.
  void GetAllSavableResourceLinksForCurrentPage();
  // Get html data by serializing all frames of current page with lists
  // which contain all resource links that have local copy.
  void GetSerializedHtmlDataForCurrentPageWithLocalLinks();

  SaveItem* LookupItemInProcessBySaveId(int32 save_id);
  void PutInProgressItemToSavedMap(SaveItem* save_item);

  typedef base::hash_map<std::string, SaveItem*> SaveUrlItemMap;
  // in_progress_items_ is map of all saving job in in-progress state.
  SaveUrlItemMap in_progress_items_;
  // saved_failed_items_ is map of all saving job which are failed.
  SaveUrlItemMap saved_failed_items_;

  // The number of in process SaveItems.
  int in_process_count() const {
    return static_cast<int>(in_progress_items_.size());
  }

  // The number of all SaveItems which have completed, including success items
  // and failed items.
  int completed_count() const {
    return static_cast<int>(saved_success_items_.size() +
                            saved_failed_items_.size());
  }

  // Helper function for preparing suggested name for the SaveAs Dialog. The
  // suggested name is composed of the default save path and the web document's
  // title.
  static FilePath GetSuggestNameForSaveAs(
      PrefService* prefs, const FilePath& name, bool can_save_as_complete);

  // Ensure that the file name has a proper extension for HTML by adding ".htm"
  // if necessary.
  static FilePath EnsureHtmlExtension(const FilePath& name);

  typedef std::queue<SaveItem*> SaveItemQueue;
  // A queue for items we are about to start saving.
  SaveItemQueue waiting_item_queue_;

  typedef base::hash_map<int32, SaveItem*> SavedItemMap;
  // saved_success_items_ is map of all saving job which are successfully saved.
  SavedItemMap saved_success_items_;

  // The request context which provides application-specific context for
  // URLRequest instances.
  scoped_refptr<URLRequestContext> request_context_;

  // Non-owning pointer for handling file writing on the file thread.
  SaveFileManager* file_manager_;

  TabContents* tab_contents_;

  // We use a fake DownloadItem here in order to reuse the DownloadItemView.
  // This class owns the pointer.
  DownloadItem* download_;

  // The URL of the page the user wants to save.
  GURL page_url_;
  FilePath saved_main_file_path_;
  FilePath saved_main_directory_path_;

  // Indicates whether the actual saving job is finishing or not.
  bool finished_;

  // Indicates whether user canceled the saving job.
  bool user_canceled_;

  // Indicates whether user get disk error.
  bool disk_error_occurred_;

  // Type about saving page as only-html or complete-html.
  SavePackageType save_type_;

  // Number of all need to be saved resources.
  size_t all_save_items_count_;

  typedef base::hash_set<FilePath::StringType> FileNameSet;
  // This set is used to eliminate duplicated file names in saving directory.
  FileNameSet file_name_set_;

  typedef base::hash_map<FilePath::StringType, uint32> FileNameCountMap;
  // This map is used to track serial number for specified filename.
  FileNameCountMap file_name_count_map_;

  // Indicates current waiting state when SavePackage try to get something
  // from outside.
  WaitState wait_state_;

  // Unique id for this SavePackage.
  const int tab_id_;

  // For managing select file dialogs.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  friend class SavePackageTest;
  DISALLOW_COPY_AND_ASSIGN(SavePackage);
};

#endif  // CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_
