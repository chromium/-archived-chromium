// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TESTING_PROFILE_H__
#define CHROME_TEST_TESTING_PROFILE_H__

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/file_util.h"
#include "chrome/browser/bookmark_bar_model.h"
#include "chrome/browser/browser_prefs.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/common/pref_service.h"

class TestingProfile : public Profile {
 public:
  TestingProfile();
  virtual ~TestingProfile();

  // Creates the history service. If |delete_file| is true, the history file is
  // deleted first, then the HistoryService is created. As TestingProfile
  // deletes the directory containing the files used by HistoryService, the
  // boolean only matters if you're recreating the HistoryService.
  void CreateHistoryService(bool delete_file);

  // Creates the BookmkarBarModel. If not invoked the bookmark bar model is
  // NULL.
  void CreateBookmarkBarModel();

  // Creates a TemplateURLModel. If not invoked the TemplateURLModel is NULL.
  void CreateTemplateURLModel();

  virtual std::wstring GetPath() {
    return path_;
  }
  virtual bool IsOffTheRecord() {
    return false;
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
  virtual HistoryService* GetHistoryService(ServiceAccessType access) {
    return history_service_.get();
  }
  void set_has_history_service(bool has_history_service) {
    has_history_service_ = has_history_service;
  }
  virtual bool HasHistoryService() const {
    return (history_service_.get() != NULL || has_history_service_);
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
  virtual SessionService* GetSessionService() {
    return NULL;
  }
  virtual void ShutdownSessionService() {
  }
  virtual bool HasSessionService() const {
    return false;
  }
  virtual std::wstring GetName() {
    return std::wstring();
  }
  virtual void SetName(const std::wstring& name) {
  }
  virtual std::wstring GetID() {
    return std::wstring();
  }
  virtual void SetID(const std::wstring& id) {
  }
  virtual void RegisterNavigationController(
      NavigationController* controller) {
  }
  virtual void UnregisterNavigationController(
      NavigationController* controller) {
  }
  virtual const ProfileControllerSet& GetNavigationControllers() {
    return controllers_;
  }
  virtual bool DidLastSessionExitCleanly() {
    return true;
  }
  virtual void MergeResourceString(int message_id,
                                   std::wstring* output_string) {
  }
  virtual void MergeResourceInteger(int message_id, int* output_value) {
  }
  virtual void MergeResourceBoolean(int message_id, bool* output_value) {
  }
  virtual bool HasBookmarkBarModel() {
    return (bookmark_bar_model_.get() != NULL);
  }
  virtual BookmarkBarModel* GetBookmarkBarModel() {
    return bookmark_bar_model_.get();
  }
  virtual bool Profile::IsSameProfile(Profile *p) {
    return this == p;
  }
  virtual Time GetStartTime() const {
    return start_time_;
  }
  virtual TabRestoreService* GetTabRestoreService() {
    return NULL;
  }
  virtual void ResetTabRestoreService() {
  }
  virtual SpellChecker* GetSpellChecker() {
    return NULL;
  }
  virtual void MarkAsCleanShutdown() {
  }

#ifdef CHROME_PERSONALIZATION
  virtual ProfilePersonalization GetProfilePersonalization() {
  }
#endif

 protected:
  // The path of the profile; the various database and other files are relative
  // to this.
  std::wstring path_;
  Time start_time_;
  ProfileControllerSet controllers_;
  scoped_ptr<PrefService> prefs_;

 private:
  // If the history service has been created, it is destroyed. This is invoked
  // from the destructor.
  void DestroyHistoryService();

  // Directory for the history service.
  std::wstring history_dir_;

  // The history service. Only created if CreateHistoryService is invoked.
  scoped_refptr<HistoryService> history_service_;

  // The BookmarkBarModel. Only created if CreateBookmarkBarModel is invoked.
  scoped_ptr<BookmarkBarModel> bookmark_bar_model_;

  // The TemplateURLFetcher. Only created if CreateTemplateURLModel is invoked.
  scoped_ptr<TemplateURLModel> template_url_model_;

  // Do we have a history service? This defaults to the value of
  // history_service, but can be explicitly set.
  bool has_history_service_;
};

#endif  // CHROME_TEST_TESTING_PROFILE_H__

