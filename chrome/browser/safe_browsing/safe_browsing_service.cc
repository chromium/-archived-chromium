// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include "base/histogram.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/base/registry_controlled_domain.h"

SafeBrowsingService::SafeBrowsingService()
    : io_loop_(NULL),
      database_(NULL),
      protocol_manager_(NULL),
      enabled_(false),
      resetting_(false) {
}

SafeBrowsingService::~SafeBrowsingService() {
}

// Only called on the UI thread.
void SafeBrowsingService::Initialize(MessageLoop* io_loop) {
  io_loop_ = io_loop;

  // Get the profile's preference for SafeBrowsing.
  std::wstring user_data_dir;
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
  DCHECK(!db_thread_.get());
  db_thread_.reset(new base::Thread("Chrome_SafeBrowsingThread"));
  if (!db_thread_->Start())
    return;

  db_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnDBInitialize));

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
  protocol_manager_->Initialize();
}

void SafeBrowsingService::OnDBInitialize() {
  DCHECK(MessageLoop::current() == db_thread_->message_loop());
  GetDatabase();
}

void SafeBrowsingService::OnIOShutdown() {
  DCHECK(MessageLoop::current() == io_loop_);
  if (!enabled_)
    return;

  enabled_ = false;

  // This cancels all in-flight GetHash requests.
  delete protocol_manager_;

  if (db_thread_.get())
    db_thread_->message_loop()->DeleteSoon(FROM_HERE, database_);

  // Flush the database thread. Any in-progress database check results will be
  // ignored and cleaned up below.
  db_thread_.reset(NULL);

  database_ = NULL;

  // Delete checks once the database thread is done, calling back any clients
  // with 'URL_SAFE'.
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
  return url.SchemeIs("http") || url.SchemeIs("https");
}

bool SafeBrowsingService::CheckUrl(const GURL& url, Client* client) {
  DCHECK(MessageLoop::current() == io_loop_);

  if (!enabled_ || !database_)
    return true;

  if (!resetting_) {
    Time start_time = Time::Now();
    bool need_check = database_->NeedToCheckUrl(url);
    UMA_HISTOGRAM_TIMES(L"SB.BloomFilter", Time::Now() - start_time);
    if (!need_check)
      return true;  // The url is definitely safe.
  }

  // The url may or may not be safe, need to go to the database to be sure.
  SafeBrowsingCheck* check = new SafeBrowsingCheck();
  check->url = url;
  check->client = client;
  check->result = URL_SAFE;
  check->need_get_hash = false;
  check->start = Time::Now();
  checks_.insert(check);

  db_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::CheckDatabase,
      check, protocol_manager_->last_update()));
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

  SafeBrowsingBlockingPage* blocking_page = new SafeBrowsingBlockingPage(
      this, client, render_process_host_id, render_view_id, url, resource_type,
      result);
  blocking_page->AddRef();
  ui_loop->PostTask(FROM_HERE, NewRunnableMethod(
      blocking_page, &SafeBrowsingBlockingPage::DisplayBlockingPage));
}

void SafeBrowsingService::CancelCheck(Client* client) {
  DCHECK(MessageLoop::current() == io_loop_);

  for (CurrentChecks::iterator i = checks_.begin(); i != checks_.end(); ++i) {
    if ((*i)->client == client)
      (*i)->client = NULL;
  }
}

void SafeBrowsingService::CheckDatabase(SafeBrowsingCheck* info,
                                        Time last_update) {
  DCHECK(MessageLoop::current() == db_thread_->message_loop());
  // If client == NULL it means it was cancelled, no need for db lookup.
  if (info->client && GetDatabase()) {
    Time now = Time::Now();
    std::string list;
    if (GetDatabase()->ContainsUrl(info->url,
                                   &list,
                                   &info->prefix_hits,
                                   &info->full_hits,
                                   last_update)) {
      if (info->prefix_hits.empty()) {
        info->result = GetResultFromListname(list);
      } else {
        if (info->full_hits.empty())
          info->need_get_hash = true;
      }
    }
    info->db_time = Time::Now() - now;
  }

  if (io_loop_)
    io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &SafeBrowsingService::OnCheckDone, info));
}

void SafeBrowsingService::OnCheckDone(SafeBrowsingCheck* info) {
  DCHECK(MessageLoop::current() == io_loop_);

  // If we've been shutdown during the database lookup, this check will already
  // have been deleted (in OnIOShutdown).
  if (!enabled_ || checks_.find(info) == checks_.end())
    return;

  UMA_HISTOGRAM_TIMES(L"SB.Database", Time::Now() - info->start);
  if (info->client && info->need_get_hash) {
    // We have a partial match so we need to query Google for the full hash.
    // Clean up will happen in HandleGetHashResults.

    // See if we have a GetHash request already in progress for this particular
    // prefix. If so, we just append ourselves to the list of interested parties
    // when the results arrive. We only do this for checks involving one prefix,
    // since that is the common case (multiple prefixes will issue the request
    // as normal).
    if (info->prefix_hits.size() == 1) {
      SBPrefix prefix = info->prefix_hits[0];
      GetHashRequests::iterator it = gethash_requests_.find(prefix);
      if (it != gethash_requests_.end()) {
        // There's already a request in progress.
        it->second.push_back(info);
        return;
      }

      // No request in progress, so we're the first for this prefix.
      GetHashRequestors requestors;
      requestors.push_back(info);
      gethash_requests_[prefix] = requestors;
    }

    // Reset the start time so that we can measure the network time without the
    // database time.
    info->start = Time::Now();
    protocol_manager_->GetFullHash(info, info->prefix_hits);
  } else {
    // We may have cached results for previous GetHash queries.
    HandleOneCheck(info, info->full_hits);
  }
}

SafeBrowsingDatabase* SafeBrowsingService::GetDatabase() {
  DCHECK(MessageLoop::current() == db_thread_->message_loop());
  if (database_)
    return database_;

  std::wstring path;
  bool result = PathService::Get(chrome::DIR_USER_DATA, &path);
  DCHECK(result);

  path.append(L"\\");
  path.append(chrome::kSafeBrowsingFilename);

  Time before = Time::Now();
  SafeBrowsingDatabase* database = SafeBrowsingDatabase::Create();
  Callback0::Type* callback =
      NewCallback(this, &SafeBrowsingService::ChunkInserted);
  result = database->Init(path, callback);
  if (!result) {
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

  std::vector<SBPrefix> prefixes = check->prefix_hits;
  UMA_HISTOGRAM_LONG_TIMES(L"SB.Network", Time::Now() - check->start);
  OnHandleGetHashResults(check, full_hashes);  // 'check' is deleted here.

  if (can_cache)
    db_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &SafeBrowsingService::CacheHashResults, prefixes, full_hashes));
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
    if (index != -1)
      result = GetResultFromListname(full_hashes[index].list_name);

    // Let the client continue handling the original request.
    check->client->OnUrlCheckResult(check->url, result);
  }

  checks_.erase(check);
  delete check;
}

void SafeBrowsingService::GetAllChunks() {
  DCHECK(MessageLoop::current() == io_loop_);
  DCHECK(enabled_);
  db_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::GetAllChunksFromDatabase));
}

void SafeBrowsingService::UpdateFinished() {
  DCHECK(MessageLoop::current() == io_loop_);
  DCHECK(enabled_);
  db_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::DatabaseUpdateFinished));
}

void SafeBrowsingService::DatabaseUpdateFinished() {
  DCHECK(MessageLoop::current() == db_thread_->message_loop());
  if (GetDatabase())
    GetDatabase()->UpdateFinished();
}

void SafeBrowsingService::OnBlockingPageDone(SafeBrowsingBlockingPage* page,
                                             Client* client,
                                             bool proceed) {
  NotifyClientBlockingComplete(client, proceed);

  if (proceed) {
    // Whitelist this domain and warning type for the given tab.
    WhiteListedEntry entry;
    entry.render_process_host_id = page->render_process_host_id();
    entry.render_view_id = page->render_view_id();
    entry.domain = net::RegistryControlledDomainService::GetDomainAndRegistry(
        page->url());
    entry.result = page->result();
    white_listed_entries_.push_back(entry);
  }

  page->Release();
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
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnChunkInserted));
}

void SafeBrowsingService::OnChunkInserted() {
  DCHECK(MessageLoop::current() == io_loop_);
  protocol_manager_->OnChunkInserted();
}

// static
void SafeBrowsingService::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kSafeBrowsingClientKey, L"");
  prefs->RegisterStringPref(prefs::kSafeBrowsingWrappedKey, L"");
}

void SafeBrowsingService::ResetDatabase() {
  DCHECK(MessageLoop::current() == io_loop_);
  resetting_ = true;
  db_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnResetDatabase));
}

void SafeBrowsingService::OnResetDatabase() {
  DCHECK(MessageLoop::current() == db_thread_->message_loop());
  GetDatabase()->ResetDatabase();
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnResetComplete));
}

void SafeBrowsingService::OnResetComplete() {
  DCHECK(MessageLoop::current() == io_loop_);
  resetting_ = false;
}

void SafeBrowsingService::HandleChunk(const std::string& list,
                                      std::deque<SBChunk>* chunks) {
  DCHECK(MessageLoop::current() == io_loop_);
  DCHECK(enabled_);
  db_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::HandleChunkForDatabase, list, chunks));
}

void SafeBrowsingService::HandleChunkForDatabase(
    const std::string& list_name,
    std::deque<SBChunk>* chunks) {
  DCHECK(MessageLoop::current() == db_thread_->message_loop());

  GetDatabase()->InsertChunks(list_name, chunks);
}

void SafeBrowsingService::HandleChunkDelete(
    std::vector<SBChunkDelete>* chunk_deletes) {
  DCHECK(MessageLoop::current() == io_loop_);
  DCHECK(enabled_);
  db_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::DeleteChunks, chunk_deletes));
}

void SafeBrowsingService::DeleteChunks(
    std::vector<SBChunkDelete>* chunk_deletes) {
  DCHECK(MessageLoop::current() == db_thread_->message_loop());

  GetDatabase()->DeleteChunks(chunk_deletes);
}

// Database worker function.
void SafeBrowsingService::GetAllChunksFromDatabase() {
  DCHECK(MessageLoop::current() == db_thread_->message_loop());
  bool database_error = false;
  std::vector<SBListChunkRanges> lists;
  if (GetDatabase()) {
    GetDatabase()->GetListsInfo(&lists);
  } else {
    database_error = true;
  }

  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnGetAllChunksFromDatabase, lists,
      database_error));
}

// Called on the io thread with the results of all chunks.
void SafeBrowsingService::OnGetAllChunksFromDatabase(
    const std::vector<SBListChunkRanges>& lists, bool database_error) {
  DCHECK(MessageLoop::current() == io_loop_);
  if (!enabled_)
    return;

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

// static
void SafeBrowsingService::LogPauseDelay(TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES(L"SB.Delay", time);
}

void SafeBrowsingService::CacheHashResults(
  const std::vector<SBPrefix>& prefixes,
  const std::vector<SBFullHashResult>& full_hashes) {
  DCHECK(MessageLoop::current() == db_thread_->message_loop());
  GetDatabase()->CacheHashResults(prefixes, full_hashes);
}

void SafeBrowsingService::OnSuspend() {
}

// Tell the SafeBrowsing database not to do expensive disk operations for a few
// minutes after waking up. It's quite likely that the act of resuming from a
// low power state will involve much disk activity, which we don't want to
// exacerbate.
void SafeBrowsingService::OnResume() {
  DCHECK(MessageLoop::current() == io_loop_);
  if (enabled_) {
    db_thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &SafeBrowsingService::HandleResume));
  }
}

void SafeBrowsingService::HandleResume() {
  DCHECK(MessageLoop::current() == db_thread_->message_loop());
  GetDatabase()->HandleResume();
}
