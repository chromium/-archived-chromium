// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include "base/command_line.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "net/base/registry_controlled_domain.h"

using base::Time;
using base::TimeDelta;

SafeBrowsingService::SafeBrowsingService()
    : io_loop_(NULL),
      database_(NULL),
      protocol_manager_(NULL),
      enabled_(false),
      resetting_(false),
      database_loaded_(false),
      update_in_progress_(false) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  DCHECK(monitor);
  if (monitor)
    monitor->AddObserver(this);
}

SafeBrowsingService::~SafeBrowsingService() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->RemoveObserver(this);
}

// Only called on the UI thread.
void SafeBrowsingService::Initialize(MessageLoop* io_loop) {
  io_loop_ = io_loop;

  // Get the profile's preference for SafeBrowsing.
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  PrefService* pref_service = profile->GetPrefs();
  if (pref_service->GetBoolean(prefs::kSafeBrowsingEnabled))
    Start();
}

// Start up SafeBrowsing objects. This can be called at browser start, or when
// the user checks the "Enable SafeBrowsing" option in the Advanced options UI.
void SafeBrowsingService::Start() {
  DCHECK(!safe_browsing_thread_.get());
  safe_browsing_thread_.reset(new base::Thread("Chrome_SafeBrowsingThread"));
  if (!safe_browsing_thread_->Start())
    return;

  // Retrieve client MAC keys.
  PrefService* local_state = g_browser_process->local_state();
  std::string client_key, wrapped_key;
  if (local_state) {
    client_key =
      WideToASCII(local_state->GetString(prefs::kSafeBrowsingClientKey));
    wrapped_key =
      WideToASCII(local_state->GetString(prefs::kSafeBrowsingWrappedKey));
  }

  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnIOInitialize, MessageLoop::current(),
      client_key, wrapped_key));

  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnDBInitialize));
}

void SafeBrowsingService::ShutDown() {
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnIOShutdown));
}

void SafeBrowsingService::OnIOInitialize(MessageLoop* notify_loop,
                                         const std::string& client_key,
                                         const std::string& wrapped_key) {
  DCHECK(MessageLoop::current() == io_loop_);
  enabled_ = true;
  protocol_manager_ = new SafeBrowsingProtocolManager(this,
                                                      notify_loop,
                                                      client_key,
                                                      wrapped_key);
  // We want to initialize the protocol manager only after the database has
  // loaded, which we'll receive asynchronously (DatabaseLoadComplete). If
  // database_loaded_ isn't true, we'll wait for that notification to do the
  // init.
  if (database_loaded_)
    protocol_manager_->Initialize();
}

void SafeBrowsingService::OnDBInitialize() {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  GetDatabase();
}

void SafeBrowsingService::OnIOShutdown() {
  DCHECK(MessageLoop::current() == io_loop_);
  if (!enabled_)
    return;

  enabled_ = false;
  resetting_ = false;

  // This cancels all in-flight GetHash requests.
  delete protocol_manager_;
  protocol_manager_ = NULL;

  if (safe_browsing_thread_.get())
    safe_browsing_thread_->message_loop()->DeleteSoon(FROM_HERE, database_);

  // Flush the database thread. Any in-progress database check results will be
  // ignored and cleaned up below.
  safe_browsing_thread_.reset(NULL);

  database_ = NULL;
  database_loaded_ = false;

  // Delete queued and pending checks once the database thread is done, calling
  // back any clients with 'URL_SAFE'.
  while (!queued_checks_.empty()) {
    QueuedCheck check = queued_checks_.front();
    if (check.client)
      check.client->OnUrlCheckResult(check.url, URL_SAFE);
    queued_checks_.pop_front();
  }

  for (CurrentChecks::iterator it = checks_.begin();
       it != checks_.end(); ++it) {
    if ((*it)->client)
      (*it)->client->OnUrlCheckResult((*it)->url, URL_SAFE);
    delete *it;
  }
  checks_.clear();

  gethash_requests_.clear();
}

// Runs on the UI thread.
void SafeBrowsingService::OnEnable(bool enabled) {
  if (enabled)
    Start();
  else
    ShutDown();
}

bool SafeBrowsingService::CanCheckUrl(const GURL& url) const {
  return url.SchemeIs(chrome::kHttpScheme) ||
         url.SchemeIs(chrome::kHttpsScheme);
}

bool SafeBrowsingService::CheckUrl(const GURL& url, Client* client) {
  DCHECK(MessageLoop::current() == io_loop_);
  if (!enabled_ || !database_)
    return true;

  if (resetting_ || !database_loaded_) {
    QueuedCheck check;
    check.client = client;
    check.url = url;
    queued_checks_.push_back(check);
    return false;
  }

  std::string list;
  std::vector<SBPrefix> prefix_hits;
  std::vector<SBFullHashResult> full_hits;
  base::Time check_start = base::Time::Now();
  bool prefix_match = database_->ContainsUrl(url, &list, &prefix_hits,
                                             &full_hits,
                                             protocol_manager_->last_update());

  UMA_HISTOGRAM_TIMES("SB2.FilterCheck", base::Time::Now() - check_start);

  if (!prefix_match)
    return true;  // URL is okay.

  // Needs to be asynchronous, since we could be in the constructor of a
  // ResourceDispatcherHost event handler which can't pause there.
  SafeBrowsingCheck* check = new SafeBrowsingCheck();
  check->url = url;
  check->client = client;
  check->result = URL_SAFE;
  check->need_get_hash = full_hits.empty();
  check->prefix_hits.swap(prefix_hits);
  check->full_hits.swap(full_hits);
  checks_.insert(check);

  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnCheckDone, check));

  return false;
}

void SafeBrowsingService::DisplayBlockingPage(const GURL& url,
                                              ResourceType::Type resource_type,
                                              UrlCheckResult result,
                                              Client* client,
                                              MessageLoop* ui_loop,
                                              int render_process_host_id,
                                              int render_view_id) {
  // Check if the user has already ignored our warning for this render_view
  // and domain.
  for (size_t i = 0; i < white_listed_entries_.size(); ++i) {
    const WhiteListedEntry& entry = white_listed_entries_[i];
    if (entry.render_process_host_id == render_process_host_id &&
        entry.render_view_id == render_view_id &&
        entry.result == result &&
        entry.domain ==
        net::RegistryControlledDomainService::GetDomainAndRegistry(url)) {
      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &SafeBrowsingService::NotifyClientBlockingComplete,
          client, true));
      return;
    }
  }

  UnsafeResource resource;
  resource.url = url;
  resource.resource_type = resource_type;
  resource.threat_type= result;
  resource.client = client;
  resource.render_process_host_id = render_process_host_id;
  resource.render_view_id = render_view_id;

  // The blocking page must be created from the UI thread.
  ui_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
      &SafeBrowsingService::DoDisplayBlockingPage,
      resource));
}

// Invoked on the UI thread.
void SafeBrowsingService::DoDisplayBlockingPage(
    const UnsafeResource& resource) {
  // The tab might have been closed.
  TabContents* wc =
      tab_util::GetTabContentsByID(resource.render_process_host_id,
                                   resource.render_view_id);

  if (!wc) {
    // The tab is gone and we did not have a chance at showing the interstitial.
    // Just act as "Don't Proceed" was chosen.
    std::vector<UnsafeResource> resources;
    resources.push_back(resource);
    MessageLoop* message_loop;
    if (g_browser_process->io_thread())
      message_loop = g_browser_process->io_thread()->message_loop();
    else  // For unit-tests, just post on the current thread.
      message_loop = MessageLoop::current();
    message_loop->PostTask(FROM_HERE, NewRunnableMethod(
        this, &SafeBrowsingService::OnBlockingPageDone, resources, false));
    return;
  }

  // Report the malware sub-resource to the SafeBrowsing servers if we have a
  // malware sub-resource on a safe page and only if the user has opted in to
  // reporting statistics.
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);
  if (prefs && prefs->GetBoolean(prefs::kMetricsReportingEnabled) &&
      resource.resource_type != ResourceType::MAIN_FRAME &&
      resource.threat_type == SafeBrowsingService::URL_MALWARE) {
    GURL page_url = wc->GetURL();
    GURL referrer_url;
    NavigationEntry* entry = wc->controller().GetActiveEntry();
    if (entry)
      referrer_url = entry->referrer();
    io_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &SafeBrowsingService::ReportMalware,
                          resource.url,
                          page_url,
                          referrer_url));
  }

  SafeBrowsingBlockingPage::ShowBlockingPage(this, resource);
}

void SafeBrowsingService::CancelCheck(Client* client) {
  DCHECK(MessageLoop::current() == io_loop_);

  for (CurrentChecks::iterator i = checks_.begin(); i != checks_.end(); ++i) {
    if ((*i)->client == client)
      (*i)->client = NULL;
  }

  // Scan the queued clients store. Clients may be here if they requested a URL
  // check before the database has finished loading or resetting.
  if (!database_loaded_ || resetting_) {
    std::deque<QueuedCheck>::iterator it = queued_checks_.begin();
    for (; it != queued_checks_.end(); ++it) {
      if (it->client == client)
        it->client = NULL;
    }
  }
}

void SafeBrowsingService::OnCheckDone(SafeBrowsingCheck* check) {
  DCHECK(MessageLoop::current() == io_loop_);

  // If we've been shutdown during the database lookup, this check will already
  // have been deleted (in OnIOShutdown).
  if (!enabled_ || checks_.find(check) == checks_.end())
    return;

  if (check->client && check->need_get_hash) {
    // We have a partial match so we need to query Google for the full hash.
    // Clean up will happen in HandleGetHashResults.

    // See if we have a GetHash request already in progress for this particular
    // prefix. If so, we just append ourselves to the list of interested parties
    // when the results arrive. We only do this for checks involving one prefix,
    // since that is the common case (multiple prefixes will issue the request
    // as normal).
    if (check->prefix_hits.size() == 1) {
      SBPrefix prefix = check->prefix_hits[0];
      GetHashRequests::iterator it = gethash_requests_.find(prefix);
      if (it != gethash_requests_.end()) {
        // There's already a request in progress.
        it->second.push_back(check);
        return;
      }

      // No request in progress, so we're the first for this prefix.
      GetHashRequestors requestors;
      requestors.push_back(check);
      gethash_requests_[prefix] = requestors;
    }

    // Reset the start time so that we can measure the network time without the
    // database time.
    check->start = Time::Now();
    protocol_manager_->GetFullHash(check, check->prefix_hits);
  } else {
    // We may have cached results for previous GetHash queries.
    HandleOneCheck(check, check->full_hits);
  }
}

SafeBrowsingDatabase* SafeBrowsingService::GetDatabase() {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  if (database_)
    return database_;

  FilePath path;
  bool result = PathService::Get(chrome::DIR_USER_DATA, &path);
  DCHECK(result);
  path = path.Append(chrome::kSafeBrowsingFilename);

  Time before = Time::Now();
  SafeBrowsingDatabase* database = SafeBrowsingDatabase::Create();
  Callback0::Type* chunk_callback =
      NewCallback(this, &SafeBrowsingService::ChunkInserted);
  bool init_success = database->Init(path, chunk_callback);

  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::DatabaseLoadComplete, !init_success));

  if (!init_success) {
    NOTREACHED();
    return NULL;
  }

  database_ = database;

  TimeDelta open_time = Time::Now() - before;
  SB_DLOG(INFO) << "SafeBrowsing database open took " <<
      open_time.InMilliseconds() << " ms.";

  return database_;
}

// Public API called only on the IO thread.
// The SafeBrowsingProtocolManager has received the full hash results for
// prefix hits detected in the database.
void SafeBrowsingService::HandleGetHashResults(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes,
    bool can_cache) {
  if (checks_.find(check) == checks_.end())
    return;

  DCHECK(enabled_);

  UMA_HISTOGRAM_LONG_TIMES("SB2.Network", Time::Now() - check->start);

  std::vector<SBPrefix> prefixes = check->prefix_hits;
  OnHandleGetHashResults(check, full_hashes);  // 'check' is deleted here.

  if (can_cache && database_) {
    // Cache the GetHash results in memory:
    database_->CacheHashResults(prefixes, full_hashes);
  }
}

void SafeBrowsingService::OnHandleGetHashResults(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes) {
  SBPrefix prefix = check->prefix_hits[0];
  GetHashRequests::iterator it = gethash_requests_.find(prefix);
  if (check->prefix_hits.size() > 1 || it == gethash_requests_.end()) {
    HandleOneCheck(check, full_hashes);
    return;
  }

  // Call back all interested parties.
  GetHashRequestors& requestors = it->second;
  for (GetHashRequestors::iterator r = requestors.begin();
       r != requestors.end(); ++r) {
    HandleOneCheck(*r, full_hashes);
  }

  gethash_requests_.erase(it);
}

void SafeBrowsingService::HandleOneCheck(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes) {
  if (check->client) {
    UrlCheckResult result = URL_SAFE;
    int index = safe_browsing_util::CompareFullHashes(check->url, full_hashes);
    if (index != -1) {
      result = GetResultFromListname(full_hashes[index].list_name);
    } else {
      // Log the case where the SafeBrowsing servers return full hashes in the
      // GetHash response that match the prefix we're looking up, but don't
      // match the full hash of the URL.
      if (!full_hashes.empty())
        UMA_HISTOGRAM_COUNTS("SB2.GetHashServerMiss", 1);
    }

    // Let the client continue handling the original request.
    check->client->OnUrlCheckResult(check->url, result);
  }

  checks_.erase(check);
  delete check;
}

void SafeBrowsingService::UpdateStarted() {
  DCHECK(MessageLoop::current() == io_loop_);
  DCHECK(enabled_);
  DCHECK(!update_in_progress_);
  update_in_progress_ = true;
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::GetAllChunksFromDatabase));
}

void SafeBrowsingService::UpdateFinished(bool update_succeeded) {
  DCHECK(MessageLoop::current() == io_loop_);
  DCHECK(enabled_);
  if (update_in_progress_) {
    update_in_progress_ = false;
    safe_browsing_thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &SafeBrowsingService::DatabaseUpdateFinished,
                          update_succeeded));
  }
}

void SafeBrowsingService::DatabaseUpdateFinished(bool update_succeeded) {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  if (GetDatabase())
    GetDatabase()->UpdateFinished(update_succeeded);
}

void SafeBrowsingService::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed) {
  for (std::vector<UnsafeResource>::const_iterator iter = resources.begin();
       iter != resources.end(); ++iter) {
    const UnsafeResource& resource = *iter;
    NotifyClientBlockingComplete(resource.client, proceed);

    if (proceed) {
      // Whitelist this domain and warning type for the given tab.
      WhiteListedEntry entry;
      entry.render_process_host_id = resource.render_process_host_id;
      entry.render_view_id = resource.render_view_id;
      entry.domain = net::RegistryControlledDomainService::GetDomainAndRegistry(
            resource.url);
      entry.result = resource.threat_type;
      white_listed_entries_.push_back(entry);
    }
  }
}

void SafeBrowsingService::NotifyClientBlockingComplete(Client* client,
                                                       bool proceed) {
  client->OnBlockingPageComplete(proceed);
}

// This method runs on the UI loop to access the prefs.
void SafeBrowsingService::OnNewMacKeys(const std::string& client_key,
                                       const std::string& wrapped_key) {
  PrefService* prefs = g_browser_process->local_state();
  if (prefs) {
    prefs->SetString(prefs::kSafeBrowsingClientKey, ASCIIToWide(client_key));
    prefs->SetString(prefs::kSafeBrowsingWrappedKey, ASCIIToWide(wrapped_key));
  }
}

void SafeBrowsingService::ChunkInserted() {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnChunkInserted));
}

void SafeBrowsingService::OnChunkInserted() {
  DCHECK(MessageLoop::current() == io_loop_);
  if (enabled_)
    protocol_manager_->OnChunkInserted();
}

void SafeBrowsingService::DatabaseLoadComplete(bool database_error) {
  DCHECK(MessageLoop::current() == io_loop_);
  if (!enabled_)
    return;

  database_loaded_ = true;

  // TODO(paulg): More robust database initialization error handling.
  if (protocol_manager_ && !database_error)
    protocol_manager_->Initialize();

  // If we have any queued requests, we can now check them.
  if (!resetting_)
    RunQueuedClients();
}

// static
void SafeBrowsingService::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kSafeBrowsingClientKey, L"");
  prefs->RegisterStringPref(prefs::kSafeBrowsingWrappedKey, L"");
}

void SafeBrowsingService::ResetDatabase() {
  DCHECK(MessageLoop::current() == io_loop_);
  resetting_ = true;
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnResetDatabase));
}

void SafeBrowsingService::OnResetDatabase() {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  GetDatabase()->ResetDatabase();
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnResetComplete));
}

void SafeBrowsingService::OnResetComplete() {
  DCHECK(MessageLoop::current() == io_loop_);
  if (enabled_) {
    resetting_ = false;
    database_loaded_ = true;
    RunQueuedClients();
  }
}

void SafeBrowsingService::HandleChunk(const std::string& list,
                                      std::deque<SBChunk>* chunks) {
  DCHECK(MessageLoop::current() == io_loop_);
  DCHECK(enabled_);
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::HandleChunkForDatabase, list, chunks));
}

void SafeBrowsingService::HandleChunkForDatabase(
    const std::string& list_name,
    std::deque<SBChunk>* chunks) {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());

  GetDatabase()->InsertChunks(list_name, chunks);
}

void SafeBrowsingService::HandleChunkDelete(
    std::vector<SBChunkDelete>* chunk_deletes) {
  DCHECK(MessageLoop::current() == io_loop_);
  DCHECK(enabled_);
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::DeleteChunks, chunk_deletes));
}

void SafeBrowsingService::DeleteChunks(
    std::vector<SBChunkDelete>* chunk_deletes) {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());

  GetDatabase()->DeleteChunks(chunk_deletes);
}

// Database worker function.
void SafeBrowsingService::GetAllChunksFromDatabase() {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  bool database_error = true;
  std::vector<SBListChunkRanges> lists;
  if (GetDatabase()) {
    if (GetDatabase()->UpdateStarted()) {
      GetDatabase()->GetListsInfo(&lists);
      database_error = false;
    } else {
      GetDatabase()->UpdateFinished(false);
    }
  }

  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnGetAllChunksFromDatabase, lists,
      database_error));
}

// Called on the io thread with the results of all chunks.
void SafeBrowsingService::OnGetAllChunksFromDatabase(
    const std::vector<SBListChunkRanges>& lists, bool database_error) {
  DCHECK(MessageLoop::current() == io_loop_);
  if (enabled_)
    protocol_manager_->OnGetChunksComplete(lists, database_error);
}

SafeBrowsingService::UrlCheckResult SafeBrowsingService::GetResultFromListname(
    const std::string& list_name) {
  if (safe_browsing_util::IsPhishingList(list_name)) {
    return URL_PHISHING;
  }

  if (safe_browsing_util::IsMalwareList(list_name)) {
    return URL_MALWARE;
  }

  SB_DLOG(INFO) << "Unknown safe browsing list " << list_name;
  return URL_SAFE;
}

void SafeBrowsingService::LogPauseDelay(TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES("SB2.Delay", time);
}

void SafeBrowsingService::CacheHashResults(
  const std::vector<SBPrefix>& prefixes,
  const std::vector<SBFullHashResult>& full_hashes) {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  GetDatabase()->CacheHashResults(prefixes, full_hashes);
}

void SafeBrowsingService::OnSuspend(base::SystemMonitor*) {
}

// Tell the SafeBrowsing database not to do expensive disk operations for a few
// minutes after waking up. It's quite likely that the act of resuming from a
// low power state will involve much disk activity, which we don't want to
// exacerbate.
void SafeBrowsingService::OnResume(base::SystemMonitor*) {
  if (enabled_) {
    safe_browsing_thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &SafeBrowsingService::HandleResume));
  }
}

void SafeBrowsingService::HandleResume() {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  // We don't call GetDatabase() here, since we want to avoid unnecessary calls
  // to Open, Reset, etc, or reload the bloom filter while we're coming out of
  // a suspended state.
  if (database_)
    database_->HandleResume();
}

void SafeBrowsingService::RunQueuedClients() {
  DCHECK(MessageLoop::current() == io_loop_);
  HISTOGRAM_COUNTS("SB.QueueDepth", queued_checks_.size());
  while (!queued_checks_.empty()) {
    QueuedCheck check = queued_checks_.front();
    HISTOGRAM_TIMES("SB.QueueDelay", Time::Now() - check.start);
    CheckUrl(check.url, check.client);
    queued_checks_.pop_front();
  }
}

void SafeBrowsingService::ReportMalware(const GURL& malware_url,
                                        const GURL& page_url,
                                        const GURL& referrer_url) {
  DCHECK(MessageLoop::current() == io_loop_);

  if (!enabled_ || !database_)
    return;

  // Check if 'page_url' is already blacklisted (exists in our cache). Only
  // report if it's not there.
  std::string list;
  std::vector<SBPrefix> prefix_hits;
  std::vector<SBFullHashResult> full_hits;
  database_->ContainsUrl(page_url, &list, &prefix_hits, &full_hits,
                         protocol_manager_->last_update());

  if (full_hits.empty())
    protocol_manager_->ReportMalware(malware_url, page_url, referrer_url);
}
