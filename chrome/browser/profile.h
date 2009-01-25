// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILE_H__
#define CHROME_BROWSER_PROFILE_H__

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/timer.h"
#ifdef CHROME_PERSONALIZATION
#include "chrome/personalization/personalization.h"
#endif
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

class BookmarkModel;
class ChromeURLRequestContext;
class DownloadManager;
class ExtensionsService;
class HistoryService;
class NavigationController;
class PrefService;
class SessionService;
class SpellChecker;
class TabRestoreService;
class TemplateURLFetcher;
class TemplateURLModel;
class URLRequestContext;
class UserScriptMaster;
class VisitedLinkMaster;
class WebDataService;

class Profile {
 public:

  // Profile services are accessed with the following parameter. This parameter
  // defines what the caller plans to do with the service.
  // The caller is  responsible for not performing any operation that would
  // result in persistent implicit records while using an OffTheRecord profile.
  // This flag allows the profile to perform an additional check.
  //
  // It also gives us an opportunity to perform further checks in the future. We
  // could, for example, return an history service that only allow some specific
  // methods.
  enum ServiceAccessType {
    // The caller plans to perform a read or write that takes place as a result
    // of the user input. Use this flag when the operation you are doing can be
    // performed while off the record. (ex: creating a bookmark)
    //
    // Since EXPLICIT_ACCESS means "as a result of a user action", this request
    // always succeeds.
    EXPLICIT_ACCESS,

    // The caller plans to call a method that will permanently change some data
    // in the profile, as part of Chrome's implicit data logging. Use this flag
    // when you are about to perform an operation which is incompatible with the
    // off the record mode.
    IMPLICIT_ACCESS
  };
  Profile() : restored_last_session_(false) {}
  virtual ~Profile() {}

  // Profile prefs are registered as soon as the prefs are loaded for the first
  // time.
  static void RegisterUserPrefs(PrefService* prefs);

  // Create a new profile given a path.
  static Profile* CreateProfile(const std::wstring& path);

  // Returns the request context for the "default" profile.  This may be called
  // from any thread.  This CAN return NULL if a first request context has not
  // yet been created.  If necessary, listen on the UI thread for
  // NOTIFY_DEFAULT_REQUEST_CONTEXT_AVAILABLE.
  //
  // The returned object is ref'd by the profile.  Callers who AddRef() it (to
  // keep it alive longer than the profile) must Release() it on the I/O thread.
  static URLRequestContext* GetDefaultRequestContext();

  // Returns the path of the directory where this profile's data is stored.
  virtual std::wstring GetPath() = 0;

  // Return whether this profile is off the record. Default is false.
  virtual bool IsOffTheRecord() = 0;

  // Return the off the record version of this profile. The returned pointer
  // is owned by the receiving profile. If the receiving profile is off the
  // record, the same profile is returned.
  virtual Profile* GetOffTheRecordProfile() = 0;

  // Return the original "recording" profile. This method returns this if the
  // profile is not off the record.
  virtual Profile* GetOriginalProfile() = 0;

  // Retrieves a pointer to the VisitedLinkMaster associated with this
  // profile.  The VisitedLinkMaster is lazily created the first time
  // that this method is called.
  virtual VisitedLinkMaster* GetVisitedLinkMaster() = 0;

  // Retrieves a pointer to the ExtensionsService associated with this
  // profile. The ExtensionsService is created at startup.
  virtual ExtensionsService* GetExtensionsService() = 0;

  // Retrieves a pointer to the UserScriptMaster associated with this
  // profile.  The UserScriptMaster is lazily created the first time
  // that this method is called.
  virtual UserScriptMaster* GetUserScriptMaster() = 0;

  // Retrieves a pointer to the HistoryService associated with this
  // profile.  The HistoryService is lazily created the first time
  // that this method is called.
  // Although HistoryService is refcounted, this will not addref, and callers
  // do not need to do any reference counting as long as they keep the pointer
  // only for the local scope (which they should do anyway since the browser
  // process may decide to shut down).
  //
  // |access| defines what the caller plans to do with the service. See
  // the ServiceAccessType definition above.
  virtual HistoryService* GetHistoryService(ServiceAccessType access) = 0;

  // Returns the WebDataService for this profile. This is owned by
  // the Profile. Callers that outlive the life of this profile need to be
  // sure they refcount the returned value.
  //
  // |access| defines what the caller plans to do with the service. See
  // the ServiceAccessType definition above.
  virtual WebDataService* GetWebDataService(ServiceAccessType access) = 0;

  // Retrieves a pointer to the PrefService that manages the preferences
  // for this user profile.  The PrefService is lazily created the first
  // time that this method is called.
  virtual PrefService* GetPrefs() = 0;

  // Returns the TemplateURLModel for this profile. This is owned by the
  // the Profile.
  virtual TemplateURLModel* GetTemplateURLModel() = 0;

  // Returns the TemplateURLFetcher for this profile. This is owned by the
  // profile.
  virtual TemplateURLFetcher* GetTemplateURLFetcher() = 0;

  // Returns the DownloadManager associated with this profile
  virtual DownloadManager* GetDownloadManager() = 0;
  virtual bool HasCreatedDownloadManager() const = 0;

  // Returns the request context information associated with this profile.  Call
  // this only on the UI thread, since it can send notifications that should
  // happen on the UI thread.
  //
  // The returned object is ref'd by the profile.  Callers who AddRef() it (to
  // keep it alive longer than the profile) must Release() it on the I/O thread.
  virtual URLRequestContext* GetRequestContext() = 0;

  // Returns the session service for this profile. This may return NULL. If
  // this profile supports a session service (it isn't off the record), and
  // the session service hasn't yet been created, this forces creation of
  // the session service.
  //
  // This returns NULL in two situations: the profile is off the record, or the
  // session service has been explicitly shutdown (browser is exiting). Callers
  // should always check the return value for NULL.
  virtual SessionService* GetSessionService() = 0;

  // If this profile has a session service, it is shut down. To properly record
  // the current state this forces creation of the session service, then shuts
  // it down.
  virtual void ShutdownSessionService() = 0;

  // Returns true if this profile has a session service.
  virtual bool HasSessionService() const = 0;

  // Convenience functions to get & set the name and ID of the profile.
  virtual std::wstring GetName() = 0;
  virtual void SetName(const std::wstring& name) = 0;
  virtual std::wstring GetID() = 0;
  virtual void SetID(const std::wstring& id) = 0;

  // Returns true if the last time this profile was open it was exited cleanly.
  virtual bool DidLastSessionExitCleanly() = 0;

  // Returns the BookmarkModel, creating if not yet created.
  virtual BookmarkModel* GetBookmarkModel() = 0;

#ifdef CHROME_PERSONALIZATION
  virtual ProfilePersonalization* GetProfilePersonalization() = 0;
#endif

  // Return whether 2 profiles are the same. 2 profiles are the same if they
  // represent the same profile. This can happen if there is pointer equality
  // or if one profile is the off the record version of another profile (or vice
  // versa).
  virtual bool IsSameProfile(Profile* profile) = 0;

  // Returns the time the profile was started. This is not the time the profile
  // was created, rather it is the time the user started chrome and logged into
  // this profile. For the single profile case, this corresponds to the time
  // the user started chrome.
  virtual base::Time GetStartTime() const = 0;

  // Returns the TabRestoreService. This returns NULL when off the record.
  virtual TabRestoreService* GetTabRestoreService() = 0;

  virtual void ResetTabRestoreService() = 0;

  // This reinitializes the spellchecker according to the current dictionary 
  // language, and enable spell check option, in the prefs.
  virtual void ReinitializeSpellChecker() = 0;

  // Returns the spell checker object for this profile. THIS OBJECT MUST ONLY
  // BE USED ON THE I/O THREAD! This pointer is retrieved from the profile and
  // sent to the I/O thread where it is actually used.
  virtual SpellChecker* GetSpellChecker() = 0;

  // Marks the profile as cleanly shutdown.
  //
  // NOTE: this is invoked internally on a normal shutdown, but is public so
  // that it can be invoked when the user logs out/powers down (WM_ENDSESSION).
  virtual void MarkAsCleanShutdown() = 0;  

  virtual void InitExtensions() = 0;

#ifdef UNIT_TEST
  // Use with caution.  GetDefaultRequestContext may be called on any thread!
  static void set_default_request_context(URLRequestContext* c) {
    default_request_context_ = c;
  }
#endif

  // Did the user restore the last session? This is set by SessionRestore.
  void set_restored_last_session(bool restored_last_session) {
    restored_last_session_ = restored_last_session;
  }
  bool restored_last_session() const {
    return restored_last_session_;
  }

 protected:
  static URLRequestContext* default_request_context_;

 private:
  bool restored_last_session_;
};

class OffTheRecordProfileImpl;

// The default profile implementation.
class ProfileImpl : public Profile,
                    public NotificationObserver {
 public:
  virtual ~ProfileImpl();

  // Profile implementation.
  virtual std::wstring GetPath();
  virtual bool IsOffTheRecord();
  virtual Profile* GetOffTheRecordProfile();
  virtual Profile* GetOriginalProfile();
  virtual VisitedLinkMaster* GetVisitedLinkMaster();
  virtual UserScriptMaster* GetUserScriptMaster();
  virtual ExtensionsService* GetExtensionsService();
  virtual HistoryService* GetHistoryService(ServiceAccessType sat);
  virtual WebDataService* GetWebDataService(ServiceAccessType sat);
  virtual PrefService* GetPrefs();
  virtual TemplateURLModel* GetTemplateURLModel();
  virtual TemplateURLFetcher* GetTemplateURLFetcher();
  virtual DownloadManager* GetDownloadManager();
  virtual bool HasCreatedDownloadManager() const;
  virtual URLRequestContext* GetRequestContext();
  virtual SessionService* GetSessionService();
  virtual void ShutdownSessionService();
  virtual bool HasSessionService() const;
  virtual std::wstring GetName();
  virtual void SetName(const std::wstring& name);
  virtual std::wstring GetID();
  virtual void SetID(const std::wstring& id);
  virtual bool DidLastSessionExitCleanly();
  virtual BookmarkModel* GetBookmarkModel();
  virtual bool IsSameProfile(Profile* profile);
  virtual base::Time GetStartTime() const;
  virtual TabRestoreService* GetTabRestoreService();
  virtual void ResetTabRestoreService();
  virtual void ReinitializeSpellChecker();
  virtual SpellChecker* GetSpellChecker();
  virtual void MarkAsCleanShutdown();
  virtual void InitExtensions();
#ifdef CHROME_PERSONALIZATION
  virtual ProfilePersonalization* GetProfilePersonalization();
#endif
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);  

 private:
  friend class Profile;

  explicit ProfileImpl(const std::wstring& path);

  void CreateWebDataService();
  std::wstring GetPrefFilePath();

  void StopCreateSessionServiceTimer();
  
  void EnsureSessionServiceCreated() {
    GetSessionService();
  }

  // Initializes the spellchecker. If the spellchecker already exsts, then
  // it is released, and initialized again. This model makes sure that 
  // spellchecker language can be changed without restarting the browser.
  // NOTE: This is being currently called in the UI thread, which is OK as long
  // as the spellchecker object is USED in the IO thread.
  // The |need_to_broadcast| parameter tells it whether to broadcast the new
  // spellchecker to the resource message filters.
  void InitializeSpellChecker(bool need_to_broadcast);
  
  std::wstring path_;
  bool off_the_record_;
  scoped_ptr<VisitedLinkMaster> visited_link_master_;
  scoped_refptr<ExtensionsService> extensions_service_;
  scoped_refptr<UserScriptMaster> user_script_master_;
  scoped_ptr<PrefService> prefs_;
  scoped_ptr<TemplateURLFetcher> template_url_fetcher_;
  scoped_ptr<TemplateURLModel> template_url_model_;
  scoped_ptr<BookmarkModel> bookmark_bar_model_;

#ifdef CHROME_PERSONALIZATION
  scoped_ptr<ProfilePersonalization> personalization_;
#endif

  ChromeURLRequestContext* request_context_;

  scoped_refptr<DownloadManager> download_manager_;
  scoped_refptr<HistoryService> history_service_;
  scoped_refptr<WebDataService> web_data_service_;
  scoped_refptr<SessionService> session_service_;
  bool history_service_created_;
  bool created_web_data_service_;
  bool created_download_manager_;
  // Whether or not the last session exited cleanly. This is set only once.
  bool last_session_exited_cleanly_;

  base::OneShotTimer<ProfileImpl> create_session_service_timer_;

  scoped_ptr<OffTheRecordProfileImpl> off_the_record_profile_;

  // See GetStartTime for details.
  base::Time start_time_;

  scoped_refptr<TabRestoreService> tab_restore_service_;

  // This can not be a scoped_refptr because we must release it on the I/O
  // thread.
  SpellChecker* spellchecker_;

  // Set to true when ShutdownSessionService is invoked. If true
  // GetSessionService won't recreate the SessionService.
  bool shutdown_session_service_;

  DISALLOW_EVIL_CONSTRUCTORS(ProfileImpl);
};

// This struct is used to pass the spellchecker object through the notification
// NOTIFY_SPELLCHECKER_REINITIALIZED. This is used as the details for the
// notification service.
struct SpellcheckerReinitializedDetails {
  scoped_refptr<SpellChecker> spellchecker;
};

#endif  // CHROME_BROWSER_PROFILE_H__

