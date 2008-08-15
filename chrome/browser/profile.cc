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

#include "chrome/browser/profile.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/lock.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/bookmark_bar_model.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download_manager.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/browser/tab_restore_service.h"
#include "chrome/browser/template_url_fetcher.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/browser/visitedlink_master.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/cookie_monster_sqlite.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_policy.h"
#include "net/http/http_cache.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/webkit_glue.h"

// Delay, in milliseconds, before we explicitly create the SessionService.
static const int kCreateSessionServiceDelayMS = 500;

// A pointer to the request context for the default profile.  See comments on
// Profile::GetDefaultRequestContext.
URLRequestContext* Profile::default_request_context_;

//static
void Profile::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kSessionExitedCleanly, true);
  prefs->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, true);
}

//static
Profile* Profile::CreateProfile(const std::wstring& path) {
  return new ProfileImpl(path);
}

//static
URLRequestContext* Profile::GetDefaultRequestContext() {
  return default_request_context_;
}


// Sets up proxy info if it was specified, otherwise returns NULL. The
// returned pointer MUST be deleted by the caller if non-NULL.
static net::ProxyInfo* CreateProxyInfo(const CommandLine& command_line) {
  net::ProxyInfo* proxy_info = NULL;

  if (command_line.HasSwitch(switches::kProxyServer)) {
    proxy_info = new net::ProxyInfo();
    const std::wstring& proxy_server =
        command_line.GetSwitchValue(switches::kProxyServer);
    proxy_info->UseNamedProxy(WideToASCII(proxy_server));
  }

  return proxy_info;
}

// Releases the URLRequestContext and sends out a notification about it.
// Note: runs on IO thread.
static void ReleaseURLRequestContext(URLRequestContext* context) {
  NotificationService::current()->Notify(NOTIFY_URL_REQUEST_CONTEXT_RELEASED,
                                         Source<URLRequestContext>(context),
                                         NotificationService::NoDetails());
  context->Release();
}

// A context for URLRequests issued relative to this profile.
class ProfileImpl::RequestContext : public URLRequestContext,
                                    public NotificationObserver {
 public:
  // |cookie_store_path| is the local disk path at which the cookie store
  // is persisted.
  RequestContext(const std::wstring& cookie_store_path,
                 const std::wstring& disk_cache_path,
                 PrefService* prefs)
      : prefs_(prefs) {
    cookie_store_ = NULL;

    // setup user agent
    user_agent_ = webkit_glue::GetDefaultUserAgent();
    // set up Accept-Language and Accept-Charset header values
    // TODO(jungshik) : This may slow down http requests. Perhaps,
    // we have to come up with a better way to set up these values.
    accept_language_ = WideToASCII(prefs_->GetString(prefs::kAcceptLanguages));
    accept_charset_ = WideToASCII(prefs_->GetString(prefs::kDefaultCharset));
    accept_charset_ += ",*,utf-8";

    CommandLine command_line;

    scoped_ptr<net::ProxyInfo> proxy_info(CreateProxyInfo(command_line));
    net::HttpCache* cache =
        new net::HttpCache(proxy_info.get(), disk_cache_path, 0);

    bool record_mode = chrome::kRecordModeEnabled &&
                       CommandLine().HasSwitch(switches::kRecordMode);
    bool playback_mode = CommandLine().HasSwitch(switches::kPlaybackMode);

    if (record_mode || playback_mode) {
      // Don't use existing cookies and use an in-memory store.
      cookie_store_ = new net::CookieMonster();
      cache->set_mode(
          record_mode ? net::HttpCache::RECORD : net::HttpCache::PLAYBACK);
    }
    http_transaction_factory_ = cache;

    // setup cookie store
    if (!cookie_store_) {
      DCHECK(!cookie_store_path.empty());
      cookie_db_.reset(new SQLitePersistentCookieStore(
          cookie_store_path, g_browser_process->db_thread()->message_loop()));
      cookie_store_ = new net::CookieMonster(cookie_db_.get());
    }

    cookie_policy_.SetType(net::CookiePolicy::FromInt(
        prefs_->GetInteger(prefs::kCookieBehavior)));

    // The first request context to be created is the one for the default
    // profile - at least until we support multiple profiles.
    if (!default_request_context_)
      default_request_context_ = this;

    // Register for notifications about prefs.
    prefs_->AddPrefObserver(prefs::kAcceptLanguages, this);
    prefs_->AddPrefObserver(prefs::kCookieBehavior, this);
  }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (NOTIFY_PREF_CHANGED == type) {
      std::wstring* pref_name_in = Details<std::wstring>(details).ptr();
      PrefService* prefs = Source<PrefService>(source).ptr();
      DCHECK(pref_name_in && prefs);
      if (*pref_name_in == prefs::kAcceptLanguages) {
        std::string accept_language =
            WideToASCII(prefs->GetString(prefs::kAcceptLanguages));
        g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
            NewRunnableMethod(this,
                              &RequestContext::OnAcceptLanguageChange,
                              accept_language));
      } else if (*pref_name_in == prefs::kCookieBehavior) {
        net::CookiePolicy::Type type = net::CookiePolicy::FromInt(
            prefs_->GetInteger(prefs::kCookieBehavior));
        g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
            NewRunnableMethod(this,
                              &RequestContext::OnCookiePolicyChange,
                              type));
      }
    }
  }

  // Since ProfileImpl::RequestContext will be destroyed on IO thread, but all
  // PrefService observers are needed to clear in before destroying ProfileImpl.
  // So we use to CleanupBeforeDestroy to do this thing. This function need to
  // be called on destructor of ProfileImpl.
  void CleanupBeforeDestroy() {
    // Unregister for pref notifications.
    prefs_->RemovePrefObserver(prefs::kAcceptLanguages, this);
    prefs_->RemovePrefObserver(prefs::kCookieBehavior, this);
    prefs_ = NULL;
  }

  void OnAcceptLanguageChange(std::string accept_language) {
    DCHECK(MessageLoop::current() ==
           ChromeThread::GetMessageLoop(ChromeThread::IO));
    accept_language_ = accept_language;
  }

  void OnCookiePolicyChange(net::CookiePolicy::Type type) {
    DCHECK(MessageLoop::current() ==
           ChromeThread::GetMessageLoop(ChromeThread::IO));
    cookie_policy_.SetType(type);
  }

  virtual ~RequestContext() {
    DCHECK(NULL == prefs_);
    delete cookie_store_;
    delete http_transaction_factory_;

    if (default_request_context_ == this)
      default_request_context_ = NULL;
  }

 private:
  scoped_ptr<SQLitePersistentCookieStore> cookie_db_;
  PrefService* prefs_;
};

////////////////////////////////////////////////////////////////////////////////
//
// An URLRequestContext proxy for OffTheRecord. This request context is
// really a proxy to the original profile's request context but set
// is_off_the_record_ to true.
//
// TODO(ACW). Do we need to share the FtpAuthCache with the real request context
// see bug 974328
//
// TODO(jackson): http://b/issue?id=1197350 Remove duplicated code from above.
//
////////////////////////////////////////////////////////////////////////////////
class OffTheRecordRequestContext : public URLRequestContext,
                                   public NotificationObserver {
 public:
  explicit OffTheRecordRequestContext(Profile* profile) {
    DCHECK(!profile->IsOffTheRecord());
    prefs_ = profile->GetPrefs();
    DCHECK(prefs_);

    // The OffTheRecordRequestContext is owned by the OffTheRecordProfileImpl
    // which is itself owned by the original profile. We reference the original
    // context to make sure it doesn't go away when we delete the object graph.
    original_context_ = profile->GetRequestContext();

    CommandLine command_line;
    scoped_ptr<net::ProxyInfo> proxy_info(CreateProxyInfo(command_line));

    http_transaction_factory_ = new net::HttpCache(NULL, 0);
    cookie_store_ = new net::CookieMonster;
    cookie_policy_.SetType(net::CookiePolicy::FromInt(
        prefs_->GetInteger(prefs::kCookieBehavior)));
    user_agent_ = original_context_->user_agent();
    accept_language_ = original_context_->accept_language();
    accept_charset_ = original_context_->accept_charset();
    is_off_the_record_ = true;

    // Register for notifications about prefs.
    prefs_->AddPrefObserver(prefs::kAcceptLanguages, this);
    prefs_->AddPrefObserver(prefs::kCookieBehavior, this);
  }

  // Since OffTheRecordProfileImpl maybe be destroyed after destroying
  // PrefService, but all PrefService observers are needed to clear in
  // before destroying PrefService. So we use to CleanupBeforeDestroy
  // to do this thing. This function need to be called on destructor
  // of ProfileImpl.
  void CleanupBeforeDestroy() {
    // Unregister for pref notifications.
    prefs_->RemovePrefObserver(prefs::kAcceptLanguages, this);
    prefs_->RemovePrefObserver(prefs::kCookieBehavior, this);
    prefs_ = NULL;
  }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (NOTIFY_PREF_CHANGED == type) {
      std::wstring* pref_name_in = Details<std::wstring>(details).ptr();
      PrefService* prefs = Source<PrefService>(source).ptr();
      DCHECK(pref_name_in && prefs);
      if (*pref_name_in == prefs::kAcceptLanguages) {
        std::string accept_language =
            WideToASCII(prefs->GetString(prefs::kAcceptLanguages));
        g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
            NewRunnableMethod(
                this,
                &OffTheRecordRequestContext::OnAcceptLanguageChange,
                accept_language));
      } else if (*pref_name_in == prefs::kCookieBehavior) {
        net::CookiePolicy::Type type = net::CookiePolicy::FromInt(
            prefs_->GetInteger(prefs::kCookieBehavior));
        g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
            NewRunnableMethod(this,
                              &OffTheRecordRequestContext::OnCookiePolicyChange,
                              type));
      }
    }
  }

  void OnAcceptLanguageChange(std::string accept_language) {
    DCHECK(MessageLoop::current() ==
           ChromeThread::GetMessageLoop(ChromeThread::IO));
    accept_language_ = accept_language;
  }

  void OnCookiePolicyChange(net::CookiePolicy::Type type) {
    DCHECK(MessageLoop::current() ==
           ChromeThread::GetMessageLoop(ChromeThread::IO));
    cookie_policy_.SetType(type);
  }

  virtual ~OffTheRecordRequestContext() {
    DCHECK(NULL == prefs_);
    delete cookie_store_;
    delete http_transaction_factory_;
    // The OffTheRecordRequestContext simply act as a proxy to the real context.
    // There is nothing else to delete.
  }

 private:
  // The original (non off the record) URLRequestContext.
  scoped_refptr<URLRequestContext> original_context_;
  PrefService* prefs_;

  DISALLOW_EVIL_CONSTRUCTORS(OffTheRecordRequestContext);
};

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
    request_context_ = new OffTheRecordRequestContext(real_profile);
    request_context_->AddRef();
    // Register for browser close notifications so we can detect when the last
    // off-the-record window is closed, in which case we can clean our states
    // (cookies, downloads...).
    NotificationService::current()->AddObserver(
        this, NOTIFY_BROWSER_CLOSED, NotificationService::AllSources());
  }

  virtual ~OffTheRecordProfileImpl() {
    if (request_context_) {
      request_context_->CleanupBeforeDestroy();
      // Clean up request context on IO thread.
      g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableFunction(&ReleaseURLRequestContext, request_context_));
      request_context_ = NULL;
    }
    NotificationService::current()->RemoveObserver(
        this, NOTIFY_BROWSER_CLOSED, NotificationService::AllSources());
  }

  virtual std::wstring GetPath() { return profile_->GetPath(); }

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

  virtual HistoryService* GetHistoryService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS) {
      return profile_->GetHistoryService(sat);
    } else {
      NOTREACHED() << "This profile is OffTheRecord";
      return NULL;
    }
  }

  virtual bool HasHistoryService() const {
    return profile_->HasHistoryService();
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

  virtual void RegisterNavigationController(NavigationController* controller) {
    profile_->RegisterNavigationController(controller);
  }

  virtual void UnregisterNavigationController(NavigationController*
                                              controller) {
    profile_->UnregisterNavigationController(controller);
  }

  virtual const Profile::ProfileControllerSet& GetNavigationControllers() {
    return profile_->GetNavigationControllers();
  }

  virtual bool DidLastSessionExitCleanly() {
    return profile_->DidLastSessionExitCleanly();
  }

  virtual bool HasBookmarkBarModel() {
    return profile_->HasBookmarkBarModel();
  }

  virtual BookmarkBarModel* GetBookmarkBarModel() {
    return profile_->GetBookmarkBarModel();
  }

#ifdef CHROME_PERSONALIZATION
  virtual ProfilePersonalization GetProfilePersonalization() {
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

  virtual SpellChecker* GetSpellChecker() {
    return profile_->GetSpellChecker();
  }

  virtual void MarkAsCleanShutdown() {
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

  // A proxy to the real request context.
  OffTheRecordRequestContext* request_context_;

  // The download manager that only stores downloaded items in memory.
  scoped_refptr<DownloadManager> download_manager_;

  // Time we were started.
  Time start_time_;

  DISALLOW_EVIL_CONSTRUCTORS(OffTheRecordProfileImpl);
};

ProfileImpl::ProfileImpl(const std::wstring& path)
    : path_(path),
      off_the_record_(false),
      history_service_created_(false),
      created_web_data_service_(false),
      created_download_manager_(false),
      request_context_(NULL),
#pragma warning(suppress: 4355) // Okay to pass "this" here.
      create_session_service_timer_(NULL),
      create_session_service_task_(this),
      start_time_(Time::Now()),
      spellchecker_(NULL),
#ifdef CHROME_PERSONALIZATION
      personalization_(NULL),
#endif
      shutdown_session_service_(false) {
  DCHECK(!path.empty()) << "Using an empty path will attempt to write " <<
                            "profile files to the root directory!";
  create_session_service_timer_ =
        MessageLoop::current()->timer_manager()->StartTimer(
            kCreateSessionServiceDelayMS, &create_session_service_task_, false);
}

ProfileImpl::~ProfileImpl() {
  tab_restore_service_.reset();

  StopCreateSessionServiceTimer();
  // TemplateURLModel schedules a task on the WebDataService from its
  // destructor. Delete it first to ensure the task gets scheduled before we
  // shut down the database.
  template_url_model_.reset();

  // The download manager queries the history system and should be deleted
  // before the history is shutdown so it can properly cancel all requests.
  download_manager_ = NULL;

#ifdef CHROME_PERSONALIZATION
  Personalization::CleanupProfilePersonalization(personalization_);
  personalization_ = NULL;
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
  Thread* io_thread = g_browser_process->io_thread();

  if (spellchecker_) {
    // The spellchecker must be deleted on the I/O thread. During testing, we
    // don't have an I/O thread.
    if (io_thread)
      io_thread->message_loop()->ReleaseSoon(FROM_HERE, spellchecker_);
    else
      spellchecker_->Release();
  }

  if (request_context_) {
    request_context_->CleanupBeforeDestroy();
    // Clean up request context on IO thread.
    io_thread->message_loop()->PostTask(FROM_HERE,
        NewRunnableFunction(&ReleaseURLRequestContext, request_context_));
    request_context_ = NULL;
  }

  MarkAsCleanShutdown();
}

std::wstring ProfileImpl::GetPath() {
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

static void BroadcastNewHistoryTable(SharedMemory* table_memory) {
  if (!table_memory)
    return;

  // send to all RenderProcessHosts
  for (RenderProcessHost::iterator i = RenderProcessHost::begin();
       i != RenderProcessHost::end(); i++) {
    if (!i->second->channel())
      continue;

    SharedMemoryHandle new_table;
    HANDLE process = i->second->process();
    if (!process) {
      // process can be null if it's started with the --single-process flag.
      process = GetCurrentProcess();
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

PrefService* ProfileImpl::GetPrefs() {
  if (!prefs_.get()) {
    prefs_.reset(new PrefService(GetPrefFilePath()));

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

std::wstring ProfileImpl::GetPrefFilePath() {
  std::wstring pref_file_path = path_;
  file_util::AppendToPath(&pref_file_path, chrome::kPreferencesFilename);
  return pref_file_path;
}

URLRequestContext* ProfileImpl::GetRequestContext() {
  if (!request_context_) {
    std::wstring cookie_path = GetPath();
    file_util::AppendToPath(&cookie_path, chrome::kCookieFilename);
    std::wstring cache_path = GetPath();
    file_util::AppendToPath(&cache_path, chrome::kCacheDirname);
    request_context_ = new ProfileImpl::RequestContext(cookie_path, cache_path, GetPrefs());
    request_context_->AddRef();

    DCHECK(request_context_->cookie_store());
  }

  return request_context_;
}

HistoryService* ProfileImpl::GetHistoryService(ServiceAccessType sat) {
  if (!history_service_created_) {
    scoped_refptr<HistoryService> history(new HistoryService(this));
    if (!history->Init(GetPath()))
      return NULL;
    history_service_.swap(history);
    history_service_created_ = true;

    // Send out the notification that the history service was created.
    NotificationService::current()->
        Notify(NOTIFY_HISTORY_CREATED, Source<Profile>(this),
               Details<HistoryService>(history_service_.get()));
  }
  return history_service_.get();
}

bool ProfileImpl::HasHistoryService() const {
  return !!history_service_.get();
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

void ProfileImpl::RegisterNavigationController(
    NavigationController* controller) {
  controllers_.insert(controller);
}

void ProfileImpl::UnregisterNavigationController(
    NavigationController* controller) {
  controllers_.erase(controller);
}

const Profile::ProfileControllerSet& ProfileImpl::GetNavigationControllers() {
  return controllers_;
}

bool ProfileImpl::DidLastSessionExitCleanly() {
  // last_session_exited_cleanly_ is set when the preferences are loaded. Force
  // it to be set by asking for the prefs.
  GetPrefs();
  return last_session_exited_cleanly_;
}

bool ProfileImpl::HasBookmarkBarModel() {
  return bookmark_bar_model_.get() != NULL;
}

BookmarkBarModel* ProfileImpl::GetBookmarkBarModel() {
  if (!bookmark_bar_model_.get())
    bookmark_bar_model_.reset(new BookmarkBarModel(this));
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
    tab_restore_service_.reset(new TabRestoreService(this));
  return tab_restore_service_.get();
}

void ProfileImpl::ResetTabRestoreService() {
  tab_restore_service_.reset(NULL);
}

SpellChecker* ProfileImpl::GetSpellChecker() {
  if (!spellchecker_) {
    // Don't check for an error here. In the extremely unlikely case that this
    // fails, the spellchecker just won't find the file. This prevents callers
    // from having to check for NULL return values from this function.
    std::wstring dict_dir;
    PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir);

    // Retrieve the dictionary name from preferences or the language DLL.
    // When we retrieve it from the language DLL.
    PrefService* prefs = GetPrefs();
    std::wstring dictionary_name = prefs->GetString(
        prefs::kSpellCheckDictionary);

    spellchecker_ = new SpellChecker(dict_dir, dictionary_name,
                                     GetRequestContext());
    spellchecker_->AddRef();  // Manual refcounting.
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

void ProfileImpl::StopCreateSessionServiceTimer() {
  if (create_session_service_timer_) {
    MessageLoop::current()->timer_manager()->
        StopTimer(create_session_service_timer_);
    delete create_session_service_timer_;
    create_session_service_timer_ = NULL;
  }
}

#ifdef CHROME_PERSONALIZATION
ProfilePersonalization ProfileImpl::GetProfilePersonalization() {
  if (!personalization_)
    personalization_ = Personalization::CreateProfilePersonalization(this);
  return personalization_;
}
#endif