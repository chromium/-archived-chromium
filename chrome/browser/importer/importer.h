// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_H_

#include <set>
#include <vector>

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/history/history_types.h"
#if defined(OS_WIN)
#include "chrome/browser/password_manager/ie7_password.h"
#endif
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/password_form.h"

// An enumeration of the type of browsers that we support to import
// settings and data from them.
enum ProfileType {
  MS_IE = 0,
  FIREFOX2,
  FIREFOX3,
  // Identifies a 'bookmarks.html' file.
  BOOKMARKS_HTML
};

// An enumeration of the type of data we want to import.
enum ImportItem {
  NONE           = 0x0000,
  HISTORY        = 0x0001,
  FAVORITES      = 0x0002,
  COOKIES        = 0x0004,  // not supported yet.
  PASSWORDS      = 0x0008,
  SEARCH_ENGINES = 0x0010,
  HOME_PAGE      = 0x0020,
  ALL            = 0x003f
};

typedef struct {
  std::wstring description;
  ProfileType browser_type;
  std::wstring source_path;
  std::wstring app_path;
  uint16 services_supported;  // bitmap of ImportItem
} ProfileInfo;

class FirefoxProfileLock;
class Importer;

// ProfileWriter encapsulates profile for writing entries into it.
// This object must be invoked on UI thread.
class ProfileWriter : public base::RefCounted<ProfileWriter> {
 public:
  // Used to identify how the bookmarks are added.
  enum BookmarkOptions {
    // Indicates the bookmark should only be added if unique. Uniqueness
    // is done by title, url and path. That is, if this is passed to
    // AddBookmarkEntry the bookmark is added only if there is no other
    // URL with the same url, path and title.
    ADD_IF_UNIQUE = 1 << 0,

    // Indicates the bookmarks are being added during first run.
    FIRST_RUN     = 1 << 1
  };

  explicit ProfileWriter(Profile* profile) : profile_(profile) { }
  virtual ~ProfileWriter() { }

  // Methods for monitoring BookmarkModel status.
  virtual bool BookmarkModelIsLoaded() const;
  virtual void AddBookmarkModelObserver(
      BookmarkModelObserver* observer);

  // Methods for monitoring TemplateURLModel status.
  virtual bool TemplateURLModelIsLoaded() const;
  virtual void AddTemplateURLModelObserver(
      NotificationObserver* observer);

  // A bookmark entry.
  struct BookmarkEntry {
    bool in_toolbar;
    GURL url;
    std::vector<std::wstring> path;
    std::wstring title;
    base::Time creation_time;

    BookmarkEntry() : in_toolbar(false) {}
  };

  // Helper methods for adding data to local stores.
  virtual void AddPasswordForm(const PasswordForm& form);
#if defined(OS_WIN)
  virtual void AddIE7PasswordInfo(const IE7PasswordInfo& info);
#endif
  virtual void AddHistoryPage(const std::vector<history::URLRow>& page);
  virtual void AddHomepage(const GURL& homepage);
  // Adds the bookmarks to the BookmarkModel.  
  // |options| is a bitmask of BookmarkOptions and dictates how and
  // which bookmarks are added. If the bitmask contains FIRST_RUN,
  // then any entries with a value of true for in_toolbar are added to
  // the bookmark bar. If the bitmask does not contain FIRST_RUN then
  // the folder name the bookmarks are added to is uniqued based on
  // |first_folder_name|. For example, if |first_folder_name| is 'foo'
  // and a folder with the name 'foo' already exists in the other
  // bookmarks folder, then the folder name 'foo 2' is used.
  // If |options| contains ADD_IF_UNIQUE, then the bookmark is added only
  // if another bookmarks does not exist with the same title, path and
  // url.
  virtual void AddBookmarkEntry(const std::vector<BookmarkEntry>& bookmark,
                                const std::wstring& first_folder_name,
                                int options);
  virtual void AddFavicons(
      const std::vector<history::ImportedFavIconUsage>& favicons);
  // Add the TemplateURLs in |template_urls| to the local store and make the
  // TemplateURL at |default_keyword_index| the default keyword (does not set
  // a default keyword if it is -1).  The local store becomes the owner of the
  // TemplateURLs.  Some TemplateURLs in |template_urls| may conflict (same
  // keyword or same host name in the URL) with existing TemplateURLs in the
  // local store, in which case the existing ones takes precedence and the
  // duplicate in |template_urls| are deleted.
  // If unique_on_host_and_path a TemplateURL is only added if there is not an
  // existing TemplateURL that has a replaceable search url with the same
  // host+path combination.
  virtual void AddKeywords(const std::vector<TemplateURL*>& template_urls,
                           int default_keyword_index,
                           bool unique_on_host_and_path);

  // Shows the bookmarks toolbar.
  void ShowBookmarkBar();

  Profile* GetProfile() const { return profile_; }

 private:
  // Generates a unique folder name. If folder_name is not unique, then this
  // repeatedly tests for '|folder_name| + (i)' until a unique name is found.
  std::wstring GenerateUniqueFolderName(BookmarkModel* model,
                                        const std::wstring& folder_name);

  // Returns true if a bookmark exists with the same url, title and path
  // as |entry|. |first_folder_name| is the name to use for the first
  // path entry if |first_run| is true.
  bool DoesBookmarkExist(BookmarkModel* model,
                         const BookmarkEntry& entry,
                         const std::wstring& first_folder_name,
                         bool first_run);

  Profile* profile_;

  DISALLOW_EVIL_CONSTRUCTORS(ProfileWriter);
};

// This class hosts the importers. It enumerates profiles from other
// browsers dynamically, and controls the process of importing. When
// the import process is done, ImporterHost deletes itself.
class ImporterHost : public base::RefCounted<ImporterHost>,
                     public BookmarkModelObserver,
                     public NotificationObserver {
 public:
  ImporterHost();
  ~ImporterHost();

  // This constructor only be used by unit-tests, where file thread does not
  // exist.
  explicit ImporterHost(MessageLoop* file_loop);

  // BookmarkModelObserver methods.
  virtual void Loaded(BookmarkModel* model);
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index) {}
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index) {}
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node) {}

  // NotificationObserver method. Called when TemplateURLModel has been loaded.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // ShowWarningDialog() asks user to close the application that is owning the
  // lock. They can retry or skip the importing process.
  void ShowWarningDialog();

  // OnLockViewEnd() is called when user end the dialog by clicking a push
  // button. |is_continue| is true when user clicked the "Continue" button.
  void OnLockViewEnd(bool is_continue);

  // Starts the process of importing the settings and data depending
  // on what the user selected.
  void StartImportSettings(const ProfileInfo& profile_info,
                           uint16 items,
                           ProfileWriter* writer,
                           bool first_run);

  // Cancel
  void Cancel();

  // When in headless mode, the importer will not show the warning dialog and
  // the outcome is as if the user had canceled the import operation.
  void set_headless() {
    headless_ = true;
  }

  bool is_headless() const {
    return headless_;
  }

  // An interface which an object can implement to be notified of events during
  // the import process.
  class Observer {
   public:
     virtual ~Observer() {}
    // Invoked when data for the specified item is about to be collected.
    virtual void ImportItemStarted(ImportItem item) = 0;

    // Invoked when data for the specified item has been collected from the
    // source profile and is now ready for further processing.
    virtual void ImportItemEnded(ImportItem item) = 0;

    // Invoked when the import begins.
    virtual void ImportStarted() = 0;

    // Invoked when the source profile has been imported.
    virtual void ImportEnded() = 0;
  };
  void SetObserver(Observer* observer);

  // A series of functions invoked at the start, during and end of the end
  // of the import process. The middle functions are notifications that the
  // harvesting of a particular source of data (specified by |item|) is under
  // way.
  void ImportStarted();
  void ImportItemStarted(ImportItem item);
  void ImportItemEnded(ImportItem item);
  void ImportEnded();

  Importer* CreateImporterByType(ProfileType type);

  // Returns the number of different browser profiles you can import from.
  int GetAvailableProfileCount();

  // Returns the name of the profile at the 'index' slot. The profiles are
  // ordered such that the profile at index 0 is the likely default browser.
  std::wstring GetSourceProfileNameAt(int index) const;

  // Returns the ProfileInfo at the specified index.  The ProfileInfo should be
  // passed to StartImportSettings().
  const ProfileInfo& GetSourceProfileInfoAt(int index) const;

 private:
  // If we're not waiting on any model to finish loading, invokes the task_.
  void InvokeTaskIfDone();

  // Detects the installed browsers and their associated profiles, then
  // stores their information in a list. It returns the list of description
  // of all profiles.
  void DetectSourceProfiles();

  // Helper methods for detecting available profiles.
  void DetectIEProfiles();
  void DetectFirefoxProfiles();

  // The list of profiles with the default one first.
  std::vector<ProfileInfo*> source_profiles_;

  Observer* observer_;
  scoped_refptr<ProfileWriter> writer_;

  // The task is the process of importing settings from other browsers.
  Task* task_;

  // The importer used in the task;
  Importer* importer_;

  // The message loop for reading the source profiles.
  MessageLoop* file_loop_;

  // True if we're waiting for the model to finish loading.
  bool waiting_for_bookmarkbar_model_;
  bool waiting_for_template_url_model_;

  // True if source profile is readable.
  bool is_source_readable_;

  // True if UI is not to be shown.
  bool headless_;

  // Firefox profile lock.
  scoped_ptr<FirefoxProfileLock> firefox_lock_;

  DISALLOW_EVIL_CONSTRUCTORS(ImporterHost);
};

// The base class of all importers.
class Importer : public base::RefCounted<Importer> {
 public:
  virtual ~Importer() { }

  // All importers should implement this method by adding their
  // import logic. And it will be run in file thread by ImporterHost.
  //
  // Since we do async import, the importer should invoke
  // ImporterHost::Finished() to notify its host that import
  // stuff have been finished.
  virtual void StartImport(ProfileInfo profile_info,
                           uint16 items,
                           ProfileWriter* writer,
                           MessageLoop* delegate_loop,
                           ImporterHost* host) = 0;

  // Cancels the import process.
  virtual void Cancel() { cancelled_ = true; }

  void set_first_run(bool first_run) { first_run_ = first_run; }

  bool cancelled() const { return cancelled_; }

 protected:
  Importer()
      : main_loop_(MessageLoop::current()),
        delagate_loop_(NULL),
        importer_host_(NULL),
        cancelled_(false),
        first_run_(false) {}

  // Notifies the coordinator that the collection of data for the specified
  // item has begun.
  void NotifyItemStarted(ImportItem item) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(importer_host_,
        &ImporterHost::ImportItemStarted, item));
  }

  // Notifies the coordinator that the collection of data for the specified
  // item has completed.
  void NotifyItemEnded(ImportItem item) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(importer_host_,
        &ImporterHost::ImportItemEnded, item));
  }

  // Notifies the coordinator that the import operation has begun.
  void NotifyStarted() {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(importer_host_,
        &ImporterHost::ImportStarted));
  }

  // Notifies the coordinator that the entire import operation has completed.
  void NotifyEnded() {
    main_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(importer_host_, &ImporterHost::ImportEnded));
  }

  // Given raw image data, decodes the icon, re-sampling to the correct size as
  // necessary, and re-encodes as PNG data in the given output vector. Returns
  // true on success.
  static bool ReencodeFavicon(const unsigned char* src_data, size_t src_len,
                              std::vector<unsigned char>* png_data);

  bool first_run() const { return first_run_; }

  // The importer should know the main thread so that ProfileWriter
  // will be invoked in thread instead.
  MessageLoop* main_loop_;
  
  // The message loop in which the importer operates.
  MessageLoop* delagate_loop_;

  // The coordinator host for this importer.
  ImporterHost* importer_host_;

 private:
  // True if the caller cancels the import process.
  bool cancelled_;

  // True if the importer is created in the first run UI.
  bool first_run_;

  DISALLOW_EVIL_CONSTRUCTORS(Importer);
};

// An interface an object that calls StartImportingWithUI can call to be
// notified about the state of the import operation.
class ImportObserver {
 public:
  virtual ~ImportObserver() {}
  // The import operation was canceled by the user.
  // TODO (4164): this is never invoked, either rip it out or invoke it.
  virtual void ImportCanceled() = 0;

  // The import operation was completed successfully.
  virtual void ImportComplete() = 0;
};


#if defined(OS_WIN)
// TODO(port): Make StartImportingWithUI portable.

// Shows a UI for importing and begins importing the specified items from
// source_profile to target_profile. observer is notified when the process is
// complete, can be NULL. parent is the window to parent the UI to, can be NULL
// if there's nothing to parent to. first_run is true if it's invoked in the
// first run UI.
void StartImportingWithUI(HWND parent_window,
                          int16 items,
                          ImporterHost* coordinator,
                          const ProfileInfo& source_profile,
                          Profile* target_profile,
                          ImportObserver* observer,
                          bool first_run);
#endif

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_H_
