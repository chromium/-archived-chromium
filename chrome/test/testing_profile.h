// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_TEST_TESTING_PROFILE_H__
#define CHROME_TEST_TESTING_PROFILE_H__

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/file_util.h"
#include "chrome/browser/browser_prefs.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_service.h"

class TestingProfile : public Profile {
 public:
  TestingProfile() : start_time_(Time::Now()) {}
  virtual ~TestingProfile();

  // Creates the HistoryService. Normally there is no HistoryService.
  void CreateHistoryService();

  virtual std::wstring GetPath() {
    return std::wstring();
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
  virtual bool HasHistoryService() const {
    return (history_service_.get() != NULL);
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
    return NULL;
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
    return false;
  }
  virtual BookmarkBarModel* GetBookmarkBarModel() {
    return NULL;
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

 protected:
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
};

#endif  // CHROME_TEST_TESTING_PROFILE_H__
