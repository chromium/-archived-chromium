// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lock.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/visitedlink_master.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/cookie_monster_sqlite.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/resource_bundle.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_policy.h"
#include "net/http/http_cache.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_POSIX)
// TODO(port): get rid of this include. It's used just to provide declarations
// and stub definitions for classes we encouter during the porting effort.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

// TODO(port): Get rid of this section and finish porting.
#if defined(OS_WIN)
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#endif

using base::Time;
using base::TimeDelta;

// Delay, in milliseconds, before we explicitly create the SessionService.
static const int kCreateSessionServiceDelayMS = 500;

// A pointer to the request context for the default profile.  See comments on
// Profile::GetDefaultRequestContext.
URLRequestContext* Profile::default_request_context_;

//static
void Profile::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kSearchSuggestEnabled, true);
  prefs->RegisterBooleanPref(prefs::kSessionExitedCleanly, true);
  prefs->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, true);
#if defined(OS_MACOSX)
  // MASSIVE HACK!!! We don't have localization working yet. Undo this once we
  // do. TODO(port): take this out
  prefs->RegisterStringPref(prefs::kSpellCheckDictionary,
      L"IDS_SPELLCHECK_DICTIONARY");
#elif defined(OS_WIN) || defined(OS_LINUX)
  prefs->RegisterLocalizedStringPref(prefs::kSpellCheckDictionary,
      IDS_SPELLCHECK_DICTIONARY);
#endif
  prefs->RegisterBooleanPref(prefs::kEnableSpellCheck, true);
}

//static
Profile* Profile::CreateProfile(const FilePath& path) {
  return new ProfileImpl(path);
}

//static
URLRequestContext* Profile::GetDefaultRequestContext() {
  return default_request_context_;
}


////////////////////////////////////////////////////////////////////////////////
//
// OffTheRecordProfileImpl is a profile subclass that wraps an existing profile
// to  make it suitable for the off the record mode.
//
////////////////////////////////////////////////////////////////////////////////
class OffTheRecordProfileImpl : public Profile,
                                public NotificationObserver {
 public:
  explicit OffTheRecordProfileImpl(Profile* real_profile)
      : profile_(real_profile),
        start_time_(Time::Now()) {
    request_context_ = ChromeURLRequestContext::CreateOffTheRecord(this);
    request_context_->AddRef();
    // Register for browser close notifications so we can detect when the last
    // off-the-record window is closed, in which case we can clean our states
    // (cookies, downloads...).
    NotificationService::current()->AddObserver(
        this, NOTIFY_BROWSER_CLOSED, NotificationService::AllSources());
  }

  virtual ~OffTheRecordProfileImpl() {
    if (request_context_) {
      request_context_->CleanupOnUIThread();

      // Clean up request context on IO thread.
      g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(request_context_,
              &base::RefCountedThreadSafe<URLRequestContext>::Release));
      request_context_ = NULL;
    }
    NotificationService::current()->RemoveObserver(
        this, NOTIFY_BROWSER_CLOSED, NotificationService::AllSources());
  }

  virtual FilePath GetPath() { return profile_->GetPath(); }

  virtual bool IsOffTheRecord() {
    return true;
  }

  virtual Profile* GetOffTheRecordProfile() {
    return this;
  }

  virtual Profile* GetOriginalProfile() {
    return profile_;
  }

  virtual VisitedLinkMaster* GetVisitedLinkMaster() {
    return profile_->GetVisitedLinkMaster();
  }

  virtual ExtensionsService* GetExtensionsService() {
    return profile_->GetExtensionsService();
  }

  virtual UserScriptMaster* GetUserScriptMaster() {
    return profile_->GetUserScriptMaster();
  }

  virtual HistoryService* GetHistoryService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS) {
      return profile_->GetHistoryService(sat);
    } else {
      NOTREACHED() << "This profile is OffTheRecord";
      return NULL;
    }
  }

  virtual WebDataService* GetWebDataService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS) {
      return profile_->GetWebDataService(sat);
    } else {
      NOTREACHED() << "This profile is OffTheRecord";
      return NULL;
    }
  }

  virtual PrefService* GetPrefs() {
    return profile_->GetPrefs();
  }

  virtual TemplateURLModel* GetTemplateURLModel() {
    return profile_->GetTemplateURLModel();
  }

  virtual TemplateURLFetcher* GetTemplateURLFetcher() {
    return profile_->GetTemplateURLFetcher();
  }

  virtual DownloadManager* GetDownloadManager() {
    if (!download_manager_.get()) {
      scoped_refptr<DownloadManager> dlm(new DownloadManager);
      dlm->Init(this);
      download_manager_.swap(dlm);
    }
    return download_manager_.get();
  }

  virtual bool HasCreatedDownloadManager() const {
    return (download_manager_.get() != NULL);
  }

  virtual URLRequestContext* GetRequestContext() {
    return request_context_;
  }

  virtual SessionService* GetSessionService() {
    // Don't save any sessions when off the record.
    return NULL;
  }

  virtual void ShutdownSessionService() {
    // We don't allow a session service, nothing to do.
  }

  virtual bool HasSessionService() const {
    // We never have a session service.
    return false;
  }

  virtual std::wstring GetName() {
    return profile_->GetName();
  }

  virtual void SetName(const std::wstring& name) {
    profile_->SetName(name);
  }

  virtual std::wstring GetID() {
    return profile_->GetID();
  }

  virtual void SetID(const std::wstring& id) {
    profile_->SetID(id);
  }

  virtual bool DidLastSessionExitCleanly() {
    return profile_->DidLastSessionExitCleanly();
  }

  virtual BookmarkModel* GetBookmarkModel() {
    return profile_->GetBookmarkModel();
  }

#ifdef CHROME_PERSONALIZATION
  virtual ProfilePersonalization* GetProfilePersonalization() {
    return profile_->GetProfilePersonalization();
  }
#endif

  virtual bool IsSameProfile(Profile* profile) {
    if (profile == static_cast<Profile*>(this))
      return true;
    return profile == profile_;
  }

  virtual Time GetStartTime() const {
    return start_time_;
  }

  virtual TabRestoreService* GetTabRestoreService() {
    return NULL;
  }

  virtual void ResetTabRestoreService() {
  }

  virtual void ReinitializeSpellChecker() {
    profile_->ReinitializeSpellChecker();
  }

  virtual SpellChecker* GetSpellChecker() {
    return profile_->GetSpellChecker();
  }

  virtual void MarkAsCleanShutdown() {
  }

  virtual void InitExtensions() {
    NOTREACHED();
  }

  virtual void ExitedOffTheRecordMode() {
    // Drop our download manager so we forget about all the downloads made
    // in off-the-record mode.
    download_manager_ = NULL;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK_EQ(NOTIFY_BROWSER_CLOSED, type);
    // We are only interested in OTR browser closing.
    if (Source<Browser>(source)->profile() != this)
      return;

    // Let's check if we still have an Off The Record window opened.
    // Note that we check against 1 as this notification is sent before the
    // browser window is actually removed from the list.
    if (BrowserList::GetBrowserCount(this) <= 1)
      ExitedOffTheRecordMode();
  }

 private:
  // The real underlying profile.
  Profile* profile_;

  // The context to use for requests made from this OTR session.
  ChromeURLRequestContext* request_context_;

  // The download manager that only stores downloaded items in memory.
  scoped_refptr<DownloadManager> download_manager_;

  // Time we were started.
  Time start_time_;

  DISALLOW_EVIL_CONSTRUCTORS(OffTheRecordProfileImpl);
};

ProfileImpl::ProfileImpl(const FilePath& path)
    : path_(path),
      off_the_record_(false),
#ifdef CHROME_PERSONALIZATION
      personalization_(NULL),
#endif
      request_context_(NULL),
      history_service_created_(false),
      created_web_data_service_(false),
      created_download_manager_(false),
      start_time_(Time::Now()),
      spellchecker_(NULL),
      shutdown_session_service_(false) {
  DCHECK(!path.empty()) << "Using an empty path will attempt to write " <<
                            "profile files to the root directory!";
  create_session_service_timer_.Start(
      TimeDelta::FromMilliseconds(kCreateSessionServiceDelayMS), this,
      &ProfileImpl::EnsureSessionServiceCreated);

  PrefService* prefs = GetPrefs();
  prefs->AddPrefObserver(prefs::kSpellCheckDictionary, this);
  prefs->AddPrefObserver(prefs::kEnableSpellCheck, this);
}

void ProfileImpl::InitExtensions() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool user_scripts_enabled =
      command_line->HasSwitch(switches::kEnableUserScripts);
  bool extensions_enabled = 
      command_line->HasSwitch(switches::kEnableExtensions);

  FilePath script_dir;
  if (user_scripts_enabled) {
    script_dir = GetPath();
    script_dir = script_dir.Append(chrome::kUserScriptsDirname);
  }

  user_script_master_ = new UserScriptMaster(
      g_browser_process->file_thread()->message_loop(), script_dir);
  extensions_service_ = new ExtensionsService(
      FilePath(GetPath()), user_script_master_.get());

  // If we have extensions, the extension service will kick off the first scan
  // after extensions are loaded. Otherwise, we need to do that now.
  if (extensions_enabled)
    extensions_service_->Init();
  else if (user_scripts_enabled)
    user_script_master_->StartScan();
}

ProfileImpl::~ProfileImpl() {
  tab_restore_service_ = NULL;

  StopCreateSessionServiceTimer();
  // TemplateURLModel schedules a task on the WebDataService from its
  // destructor. Delete it first to ensure the task gets scheduled before we
  // shut down the database.
  template_url_model_.reset();

  // The download manager queries the history system and should be deleted
  // before the history is shutdown so it can properly cancel all requests.
  download_manager_ = NULL;

  // Remove pref observers.
  PrefService* prefs = GetPrefs();
  prefs->RemovePrefObserver(prefs::kSpellCheckDictionary, this);
  prefs->RemovePrefObserver(prefs::kEnableSpellCheck, this);

#ifdef CHROME_PERSONALIZATION
  personalization_.reset();
#endif

  // Both HistoryService and WebDataService maintain threads for background
  // processing. Its possible each thread still has tasks on it that have
  // increased the ref count of the service. In such a situation, when we
  // decrement the refcount, it won't be 0, and the threads/databases aren't
  // properly shut down. By explicitly calling Cleanup/Shutdown we ensure the
  // databases are properly closed.
  if (web_data_service_.get())
    web_data_service_->Shutdown();

  if (history_service_.get())
    history_service_->Cleanup();

  // The I/O thread may be NULL during testing.
  base::Thread* io_thread = g_browser_process->io_thread();

  if (spellchecker_) {
    // The spellchecker must be deleted on the I/O thread. During testing, we
    // don't have an I/O thread.
    if (io_thread)
      io_thread->message_loop()->ReleaseSoon(FROM_HERE, spellchecker_);
    else
      spellchecker_->Release();
  }

  if (request_context_) {
    request_context_->CleanupOnUIThread();

    if (default_request_context_ == request_context_)
      default_request_context_ = NULL;

    // Clean up request context on IO thread.
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(request_context_,
            &base::RefCountedThreadSafe<URLRequestContext>::Release));
    request_context_ = NULL;
  }

  // HistoryService may call into the BookmarkModel, as such we need to
  // delete HistoryService before the BookmarkModel. The destructor for
  // HistoryService will join with HistoryService's backend thread so that
  // by the time the destructor has finished we're sure it will no longer call
  // into the BookmarkModel.
  history_service_ = NULL;
  bookmark_bar_model_.reset();

  MarkAsCleanShutdown();
}

FilePath ProfileImpl::GetPath() {
  return path_;
}

bool ProfileImpl::IsOffTheRecord() {
  return false;
}

Profile* ProfileImpl::GetOffTheRecordProfile() {
  if (!off_the_record_profile_.get()) {
    scoped_ptr<OffTheRecordProfileImpl> p(new OffTheRecordProfileImpl(this));
    off_the_record_profile_.swap(p);
  }
  return off_the_record_profile_.get();
}

Profile* ProfileImpl::GetOriginalProfile() {
  return this;
}

static void BroadcastNewHistoryTable(base::SharedMemory* table_memory) {
  if (!table_memory)
    return;

  // send to all RenderProcessHosts
  for (RenderProcessHost::iterator i = RenderProcessHost::begin();
       i != RenderProcessHost::end(); i++) {
    if (!i->second->channel())
      continue;

    base::SharedMemoryHandle new_table;
    base::ProcessHandle process = i->second->process().handle();
    if (!process) {
      // process can be null if it's started with the --single-process flag.
      process = base::Process::Current().handle();
    }

    table_memory->ShareToProcess(process, &new_table);
    IPC::Message* msg = new ViewMsg_VisitedLink_NewTable(new_table);
    i->second->channel()->Send(msg);
  }
}

VisitedLinkMaster* ProfileImpl::GetVisitedLinkMaster() {
  if (!visited_link_master_.get()) {
    scoped_ptr<VisitedLinkMaster> visited_links(
      new VisitedLinkMaster(g_browser_process->file_thread(),
                            BroadcastNewHistoryTable, this));
    if (!visited_links->Init())
      return NULL;
    visited_link_master_.swap(visited_links);
  }

  return visited_link_master_.get();
}

ExtensionsService* ProfileImpl::GetExtensionsService() {
  return extensions_service_.get();
}

UserScriptMaster* ProfileImpl::GetUserScriptMaster() {
  return user_script_master_.get();
}

PrefService* ProfileImpl::GetPrefs() {
  if (!prefs_.get()) {
    prefs_.reset(new PrefService(GetPrefFilePath().ToWStringHack()));

    // The Profile class and ProfileManager class may read some prefs so
    // register known prefs as soon as possible.
    Profile::RegisterUserPrefs(prefs_.get());
    ProfileManager::RegisterUserPrefs(prefs_.get());

    // The last session exited cleanly if there is no pref for
    // kSessionExitedCleanly or the value for kSessionExitedCleanly is true.
    last_session_exited_cleanly_ =
        prefs_->GetBoolean(prefs::kSessionExitedCleanly);
    // Mark the session as open.
    prefs_->SetBoolean(prefs::kSessionExitedCleanly, false);
    // Make sure we save to disk that the session has opened.
    prefs_->ScheduleSavePersistentPrefs(g_browser_process->file_thread());
  }

  return prefs_.get();
}

FilePath ProfileImpl::GetPrefFilePath() {
  FilePath pref_file_path = path_;
  pref_file_path = pref_file_path.Append(chrome::kPreferencesFilename);
  return pref_file_path;
}

URLRequestContext* ProfileImpl::GetRequestContext() {
  if (!request_context_) {
    FilePath cookie_path = GetPath();
    cookie_path = cookie_path.Append(chrome::kCookieFilename);
    FilePath cache_path = GetPath();
    cache_path = cache_path.Append(chrome::kCacheDirname);
    request_context_ = ChromeURLRequestContext::CreateOriginal(
        this, cookie_path, cache_path);
    request_context_->AddRef();

    // The first request context is always a normal (non-OTR) request context.
    // Even when Chromium is started in OTR mode, a normal profile is always
    // created first.
    if (!default_request_context_) {
      default_request_context_ = request_context_;
      NotificationService::current()->Notify(
          NOTIFY_DEFAULT_REQUEST_CONTEXT_AVAILABLE,
          NotificationService::AllSources(), NotificationService::NoDetails());
    }

    DCHECK(request_context_->cookie_store());
  }

  return request_context_;
}

HistoryService* ProfileImpl::GetHistoryService(ServiceAccessType sat) {
  if (!history_service_created_) {
    history_service_created_ = true;
    scoped_refptr<HistoryService> history(new HistoryService(this));
    if (!history->Init(GetPath(), GetBookmarkModel()))
      return NULL;
    history_service_.swap(history);

    // Send out the notification that the history service was created.
    NotificationService::current()->
        Notify(NOTIFY_HISTORY_CREATED, Source<Profile>(this),
               Details<HistoryService>(history_service_.get()));
  }
  return history_service_.get();
}

TemplateURLModel* ProfileImpl::GetTemplateURLModel() {
  if (!template_url_model_.get())
    template_url_model_.reset(new TemplateURLModel(this));
  return template_url_model_.get();
}

TemplateURLFetcher* ProfileImpl::GetTemplateURLFetcher() {
  if (!template_url_fetcher_.get())
    template_url_fetcher_.reset(new TemplateURLFetcher(this));
  return template_url_fetcher_.get();
}

WebDataService* ProfileImpl::GetWebDataService(ServiceAccessType sat) {
  if (!created_web_data_service_)
    CreateWebDataService();
  return web_data_service_.get();
}

void ProfileImpl::CreateWebDataService() {
  DCHECK(!created_web_data_service_ && web_data_service_.get() == NULL);
  created_web_data_service_ = true;
  scoped_refptr<WebDataService> wds(new WebDataService());
  if (!wds->Init(GetPath()))
    return;
  web_data_service_.swap(wds);
}

DownloadManager* ProfileImpl::GetDownloadManager() {
  if (!created_download_manager_) {
    scoped_refptr<DownloadManager> dlm(new DownloadManager);
    dlm->Init(this);
    created_download_manager_ = true;
    download_manager_.swap(dlm);
  }
  return download_manager_.get();
}

bool ProfileImpl::HasCreatedDownloadManager() const {
  return created_download_manager_;
}

SessionService* ProfileImpl::GetSessionService() {
  if (!session_service_.get() && !shutdown_session_service_) {
    session_service_ = new SessionService(this);
    session_service_->ResetFromCurrentBrowsers();
  }
  return session_service_.get();
}

void ProfileImpl::ShutdownSessionService() {
  if (shutdown_session_service_)
    return;

  // We're about to exit, force creation of the session service if it hasn't
  // been created yet. We do this to ensure session state matches the point in
  // time the user exited.
  GetSessionService();
  shutdown_session_service_ = true;
  session_service_ = NULL;
}

bool ProfileImpl::HasSessionService() const {
  return (session_service_.get() != NULL);
}

std::wstring ProfileImpl::GetName() {
  return GetPrefs()->GetString(prefs::kProfileName);
}
void ProfileImpl::SetName(const std::wstring& name) {
  GetPrefs()->SetString(prefs::kProfileName, name);
}

std::wstring ProfileImpl::GetID() {
  return GetPrefs()->GetString(prefs::kProfileID);
}
void ProfileImpl::SetID(const std::wstring& id) {
  GetPrefs()->SetString(prefs::kProfileID, id);
}

bool ProfileImpl::DidLastSessionExitCleanly() {
  // last_session_exited_cleanly_ is set when the preferences are loaded. Force
  // it to be set by asking for the prefs.
  GetPrefs();
  return last_session_exited_cleanly_;
}

BookmarkModel* ProfileImpl::GetBookmarkModel() {
  if (!bookmark_bar_model_.get()) {
    bookmark_bar_model_.reset(new BookmarkModel(this));
    bookmark_bar_model_->Load();
  }
  return bookmark_bar_model_.get();
}

bool ProfileImpl::IsSameProfile(Profile* profile) {
  if (profile == static_cast<Profile*>(this))
    return true;
  OffTheRecordProfileImpl* otr_profile = off_the_record_profile_.get();
  return otr_profile && profile == static_cast<Profile*>(otr_profile);
}

Time ProfileImpl::GetStartTime() const {
  return start_time_;
}

TabRestoreService* ProfileImpl::GetTabRestoreService() {
  if (!tab_restore_service_.get())
    tab_restore_service_ = new TabRestoreService(this);
  return tab_restore_service_.get();
}

void ProfileImpl::ResetTabRestoreService() {
  tab_restore_service_ = NULL;
}

// To be run in the IO thread to notify all resource message filters that the 
// spellchecker has changed.
class NotifySpellcheckerChangeTask : public Task {
 public:
  NotifySpellcheckerChangeTask(
      Profile* profile,
      const SpellcheckerReinitializedDetails& spellchecker)
      : profile_(profile),
        spellchecker_(spellchecker) {
  }

 private:
  void Run(void) {
    NotificationService::current()->Notify(
        NOTIFY_SPELLCHECKER_REINITIALIZED,
        Source<Profile>(profile_),
        Details<SpellcheckerReinitializedDetails>(&spellchecker_));
  }

  Profile* profile_;
  SpellcheckerReinitializedDetails spellchecker_;
};

void ProfileImpl::InitializeSpellChecker(bool need_to_broadcast) {
  // The I/O thread may be NULL during testing.
  base::Thread* io_thread = g_browser_process->io_thread();
  if (spellchecker_) {
    // The spellchecker must be deleted on the I/O thread.
    // A dummy variable to aid in logical clarity.
    SpellChecker* last_spellchecker = spellchecker_;

    if (io_thread)
      io_thread->message_loop()->ReleaseSoon(FROM_HERE, last_spellchecker);
    else  //  during testing, we don't have an I/O thread
      last_spellchecker->Release();
  }

  // Retrieve the (perhaps updated recently) dictionary name from preferences.
  PrefService* prefs = GetPrefs();
  bool enable_spellcheck = prefs->GetBoolean(prefs::kEnableSpellCheck);

  if (enable_spellcheck) {
    std::wstring dict_dir;
    PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir);
    // Note that, as the object pointed to by previously by spellchecker_ 
    // is being deleted in the io thread, the spellchecker_ can be made to point
    // to a new object (RE-initialized) in parallel in this UI thread.
    spellchecker_ = new SpellChecker(dict_dir,
        prefs->GetString(prefs::kSpellCheckDictionary), GetRequestContext(),
        std::wstring());
    spellchecker_->AddRef();  // Manual refcounting.
  } else {
    spellchecker_ = NULL;
  }

  if (need_to_broadcast && io_thread) {  // Notify resource message filters.
    SpellcheckerReinitializedDetails scoped_spellchecker;
    scoped_spellchecker.spellchecker = spellchecker_;
    if (io_thread) {
      io_thread->message_loop()->PostTask(
          FROM_HERE, 
          new NotifySpellcheckerChangeTask(this, scoped_spellchecker));
    }
  }
}

void ProfileImpl::ReinitializeSpellChecker() {
  InitializeSpellChecker(true);
}

SpellChecker* ProfileImpl::GetSpellChecker() {
  if (!spellchecker_) {
    // This is where spellchecker gets initialized. Note that this is being
    // initialized in the ui_thread. However, this is not a problem as long as
    // it is *used* in the io thread.
    // TODO (sidchat) One day, change everything so that spellchecker gets
    // initialized in the IO thread itself.
    InitializeSpellChecker(false);
  }

  return spellchecker_;
}

void ProfileImpl::MarkAsCleanShutdown() {
  if (prefs_.get()) {
    // The session cleanly exited, set kSessionExitedCleanly appropriately.
    prefs_->SetBoolean(prefs::kSessionExitedCleanly, true);

    // NOTE: If you change what thread this writes on, be sure and update
    // ChromeFrame::EndSession().
    prefs_->SavePersistentPrefs(g_browser_process->file_thread());
  }
}

void ProfileImpl::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (NOTIFY_PREF_CHANGED == type) {
    std::wstring* pref_name_in = Details<std::wstring>(details).ptr();
    PrefService* prefs = Source<PrefService>(source).ptr();
    DCHECK(pref_name_in && prefs);
    if (*pref_name_in == prefs::kSpellCheckDictionary ||
        *pref_name_in == prefs::kEnableSpellCheck) {
      InitializeSpellChecker(true);
    }
  }
}

void ProfileImpl::StopCreateSessionServiceTimer() {
  create_session_service_timer_.Stop();
}

#ifdef CHROME_PERSONALIZATION
ProfilePersonalization* ProfileImpl::GetProfilePersonalization() {
  if (!personalization_.get())
    personalization_.reset(
        Personalization::CreateProfilePersonalization(this));
  return personalization_.get();
}
#endif
