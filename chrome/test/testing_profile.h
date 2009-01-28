// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TESTING_PROFILE_H_
#define CHROME_TEST_TESTING_PROFILE_H_

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/file_util.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_prefs.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/common/pref_service.h"

class TestingProfile : public Profile {
 public:
  TestingProfile();
  // Creates a new profile by adding |count| to the end of the path. Use this
  // when you need to have more than one TestingProfile running at the same
  // time.
  explicit TestingProfile(int count);
  virtual ~TestingProfile();

  // Creates the history service. If |delete_file| is true, the history file is
  // deleted first, then the HistoryService is created. As TestingProfile
  // deletes the directory containing the files used by HistoryService, the
  // boolean only matters if you're recreating the HistoryService.
  void CreateHistoryService(bool delete_file);

  // Creates the BookmkarBarModel. If not invoked the bookmark bar model is
  // NULL. If |delete_file| is true, the bookmarks file is deleted first, then
  // the model is created. As TestingProfile deletes the directory containing
  // the files used by HistoryService, the boolean only matters if you're
  // recreating the BookmarkModel.
  //
  // NOTE: this does not block until the bookmarks are loaded. For that use
  // BlockUntilBookmarkModelLoaded.
  void CreateBookmarkModel(bool delete_file);

  // Blocks until the BookmarkModel finishes loaded. This is NOT invoked from
  // CreateBookmarkModel.
  void BlockUntilBookmarkModelLoaded();

  // Creates a TemplateURLModel. If not invoked the TemplateURLModel is NULL.
  void CreateTemplateURLModel();

  virtual FilePath GetPath() {
    return path_;
  }
  // Sets whether we're off the record. Default is false.
  void set_off_the_record(bool off_the_record) {
    off_the_record_ = off_the_record;
  }
  virtual bool IsOffTheRecord() {
    return off_the_record_;
  }
  virtual Profile* GetOffTheRecordProfile() {
    return NULL;
  }
  virtual Profile* GetOriginalProfile() {
    return this;
  }
  virtual VisitedLinkMaster* GetVisitedLinkMaster() {
    return NULL;
  }
  virtual ExtensionsService* GetExtensionsService() {
    return NULL;
  }
  virtual UserScriptMaster* GetUserScriptMaster() {
    return NULL;
  }
  virtual HistoryService* GetHistoryService(ServiceAccessType access) {
    return history_service_.get();
  }
  void set_has_history_service(bool has_history_service) {
    has_history_service_ = has_history_service;
  }
  virtual WebDataService* GetWebDataService(ServiceAccessType access) {
    return NULL;
  }
  virtual PrefService* GetPrefs() {
    std::wstring prefs_filename;
    PathService::Get(base::DIR_TEMP, &prefs_filename);
    file_util::AppendToPath(&prefs_filename, L"TestPreferences");
    if (!prefs_.get()) {
      prefs_.reset(new PrefService(prefs_filename));
      Profile::RegisterUserPrefs(prefs_.get());
      browser::RegisterAllPrefs(prefs_.get(), prefs_.get());
    }
    return prefs_.get();
  }
  virtual TemplateURLModel* GetTemplateURLModel() {
    return template_url_model_.get();
  }
  virtual TemplateURLFetcher* GetTemplateURLFetcher() {
    return NULL;
  }
  virtual DownloadManager* GetDownloadManager() {
    return NULL;
  }
  virtual bool HasCreatedDownloadManager() const {
    return false;
  }
  virtual URLRequestContext* GetRequestContext() {
    return NULL;
  }
  void set_session_service(SessionService* session_service) {
    session_service_ = session_service;
  }
  virtual SessionService* GetSessionService() {
    return session_service_.get();
  }
  virtual void ShutdownSessionService() {
  }
  virtual bool HasSessionService() const {
    return (session_service_.get() != NULL);
  }
  virtual std::wstring GetName() {
    return std::wstring();
  }
  virtual void SetName(const std::wstring& name) {
  }
  virtual std::wstring GetID() {
    return id_;
  }
  virtual void SetID(const std::wstring& id) {
    id_ = id;
  }
  void set_last_session_exited_cleanly(bool value) {
    last_session_exited_cleanly_ = value;
  }
  virtual bool DidLastSessionExitCleanly() {
    return last_session_exited_cleanly_;
  }
  virtual void MergeResourceString(int message_id,
                                   std::wstring* output_string) {
  }
  virtual void MergeResourceInteger(int message_id, int* output_value) {
  }
  virtual void MergeResourceBoolean(int message_id, bool* output_value) {
  }
  virtual BookmarkModel* GetBookmarkModel() {
    return bookmark_bar_model_.get();
  }
  virtual bool Profile::IsSameProfile(Profile *p) {
    return this == p;
  }
  virtual base::Time GetStartTime() const {
    return start_time_;
  }
  virtual TabRestoreService* GetTabRestoreService() {
    return NULL;
  }
  virtual void ResetTabRestoreService() {
  }
  virtual void ReinitializeSpellChecker() {
  }
  virtual SpellChecker* GetSpellChecker() {
    return NULL;
  }
  virtual void MarkAsCleanShutdown() {
  }
  virtual void InitExtensions() {
  }

#ifdef CHROME_PERSONALIZATION
  virtual ProfilePersonalization* GetProfilePersonalization() {
    return NULL;
  }
#endif

 protected:
  // The path of the profile; the various database and other files are relative
  // to this.
  FilePath path_;
  base::Time start_time_;
  scoped_ptr<PrefService> prefs_;

 private:
  // If the history service has been created, it is destroyed. This is invoked
  // from the destructor.
  void DestroyHistoryService();

  // The history service. Only created if CreateHistoryService is invoked.
  scoped_refptr<HistoryService> history_service_;

  // The BookmarkModel. Only created if CreateBookmarkModel is invoked.
  scoped_ptr<BookmarkModel> bookmark_bar_model_;

  // The TemplateURLFetcher. Only created if CreateTemplateURLModel is invoked.
  scoped_ptr<TemplateURLModel> template_url_model_;

  // The SessionService. Defaults to NULL, but can be set using the setter.
  scoped_refptr<SessionService> session_service_;

  // Do we have a history service? This defaults to the value of
  // history_service, but can be explicitly set.
  bool has_history_service_;

  std::wstring id_;

  bool off_the_record_;

  // Did the last session exit cleanly? Default is true.
  bool last_session_exited_cleanly_;
};

#endif  // CHROME_TEST_TESTING_PROFILE_H_
