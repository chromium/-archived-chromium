// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/protocol_manager.h"

#include "base/file_version_info.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/task.h"
#include "base/timer.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/safe_browsing/protocol_parser.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/stl_util-inl.h"
#include "net/base/base64.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"

using base::Time;
using base::TimeDelta;

// Maximum time, in seconds, from start up before we must issue an update query.
static const int kSbTimerStartIntervalSec = 5 * 60;

// Update URL for querying about the latest set of chunk updates.
static const char* const kSbUpdateUrl =
    "http://safebrowsing.clients.google.com/safebrowsing/downloads?client=%s&appver=%s&pver=2.2";

// GetHash request URL for retrieving full hashes.
static const char* const kSbGetHashUrl =
    "http://safebrowsing.clients.google.com/safebrowsing/gethash?client=%s&appver=%s&pver=2.2";

// New MAC client key requests URL.
static const char* const kSbNewKeyUrl =
    "https://sb-ssl.google.com/safebrowsing/newkey?client=%s&appver=%s&pver=2.2";

// URL for reporting malware pages.
static const char* const kSbMalwareReportUrl =
    "http://safebrowsing.clients.google.com/safebrowsing/report?evts=malblhit&evtd=%s&evtr=%s&evhr=%s&client=%s&appver=%s";

#if defined(GOOGLE_CHROME_BUILD)
static const char* const kSbClientName = "googlechrome";
#else
static const char* const kSbClientName = "chromium";
#endif

// Maximum back off multiplier.
static const int kSbMaxBackOff = 8;


// SafeBrowsingProtocolManager implementation ----------------------------------

SafeBrowsingProtocolManager::SafeBrowsingProtocolManager(
    SafeBrowsingService* sb_service,
    MessageLoop* notify_loop,
    const std::string& client_key,
    const std::string& wrapped_key)
    : sb_service_(sb_service),
      request_type_(NO_REQUEST),
      update_error_count_(0),
      gethash_error_count_(0),
      update_back_off_mult_(1),
      gethash_back_off_mult_(1),
      next_update_sec_(-1),
      update_state_(FIRST_REQUEST),
      initial_request_(true),
      chunk_pending_to_write_(false),
      notify_loop_(notify_loop),
      client_key_(client_key),
      wrapped_key_(wrapped_key),
      update_size_(0) {
  // Set the backoff multiplier fuzz to a random value between 0 and 1.
  back_off_fuzz_ = static_cast<float>(base::RandDouble());

  // The first update must happen between 0-5 minutes of start up.
  next_update_sec_ = base::RandInt(60, kSbTimerStartIntervalSec);

  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (!version_info.get())
    version_ = "0.1";
  else
    version_ = WideToASCII(version_info->product_version());
}

SafeBrowsingProtocolManager::~SafeBrowsingProtocolManager() {
  // Delete in-progress SafeBrowsing requests.
  STLDeleteContainerPairFirstPointers(hash_requests_.begin(),
                                      hash_requests_.end());
  hash_requests_.clear();

  // Delete in-progress malware reports.
  STLDeleteContainerPointers(malware_reports_.begin(), malware_reports_.end());
  malware_reports_.clear();
}

// Public API used by the SafeBrowsingService ----------------------------------

// We can only have one update or chunk request outstanding, but there may be
// multiple GetHash requests pending since we don't want to serialize them and
// slow down the user.
void SafeBrowsingProtocolManager::GetFullHash(
    SafeBrowsingService::SafeBrowsingCheck* check,
    const std::vector<SBPrefix>& prefixes) {
  // If we are in GetHash backoff, we need to check if we're past the next
  // allowed time. If we are, we can proceed with the request. If not, we are
  // required to return empty results (i.e. treat the page as safe).
  if (gethash_error_count_ && Time::Now() <= next_gethash_time_) {
    std::vector<SBFullHashResult> full_hashes;
    sb_service_->HandleGetHashResults(check, full_hashes, false);
    return;
  }

  std::string url = StringPrintf(kSbGetHashUrl,
                                 kSbClientName,
                                 version_.c_str());
  if (!client_key_.empty()) {
    url.append("&wrkey=");
    url.append(wrapped_key_);
  }

  GURL gethash_url(url);
  URLFetcher* fetcher = new URLFetcher(gethash_url, URLFetcher::POST, this);
  hash_requests_[fetcher] = check;

  std::string get_hash;
  SafeBrowsingProtocolParser parser;
  parser.FormatGetHash(prefixes, &get_hash);

  fetcher->set_load_flags(net::LOAD_DISABLE_CACHE);
  fetcher->set_request_context(Profile::GetDefaultRequestContext());
  fetcher->set_upload_data("text/plain", get_hash);
  fetcher->Start();
}

void SafeBrowsingProtocolManager::GetNextUpdate() {
  if (initial_request_) {
    if (client_key_.empty() || wrapped_key_.empty()) {
      IssueKeyRequest();
      return;
    } else {
      initial_request_ = false;
    }
  }

  if (!request_.get())
    IssueUpdateRequest();
}

// URLFetcher::Delegate implementation -----------------------------------------

// All SafeBrowsing request responses are handled here.
// TODO(paulg): Clarify with the SafeBrowsing team whether a failed parse of a
//              chunk should retry the download and parse of that chunk (and
//              what back off / how many times to try), and if that effects the
//              update back off. For now, a failed parse of the chunk means we
//              drop it. This isn't so bad because the next UPDATE_REQUEST we
//              do will report all the chunks we have. If that chunk is still
//              required, the SafeBrowsing servers will tell us to get it again.
void SafeBrowsingProtocolManager::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  scoped_ptr<const URLFetcher> fetcher;
  bool parsed_ok = true;
  bool must_back_off = false;  // Reduce SafeBrowsing service query frequency.

  // See if this is a malware report fetcher. We don't take any action for
  // the response to those.
  std::set<const URLFetcher*>::iterator mit = malware_reports_.find(source);
  if (mit != malware_reports_.end()) {
    const URLFetcher* report = *mit;
    malware_reports_.erase(mit);
    delete report;
    return;
  }

  HashRequests::iterator it = hash_requests_.find(source);
  if (it != hash_requests_.end()) {
    // GetHash response.
    fetcher.reset(it->first);
    SafeBrowsingService::SafeBrowsingCheck* check = it->second;
    std::vector<SBFullHashResult> full_hashes;
    bool can_cache = false;
    if (response_code == 200 || response_code == 204) {
      can_cache = true;
      gethash_error_count_ = 0;
      gethash_back_off_mult_ = 1;
      bool re_key = false;
      SafeBrowsingProtocolParser parser;
      parsed_ok = parser.ParseGetHash(data.data(),
                                      static_cast<int>(data.length()),
                                      client_key_,
                                      &re_key,
                                      &full_hashes);
      if (!parsed_ok) {
        // If we fail to parse it, we must still inform the SafeBrowsingService
        // so that it doesn't hold up the user's request indefinitely. Not sure
        // what to do at that point though!
        full_hashes.clear();
      } else {
        if (re_key)
          HandleReKey();
      }
    } else if (response_code >= 300) {
      HandleGetHashError();
      SB_DLOG(INFO) << "SafeBrowsing GetHash request for: " << source->url()
                    << ", failed with error: " << response_code;
    }

    // Call back the SafeBrowsingService with full_hashes, even if there was a
    // parse error or an error response code (in which case full_hashes will be
    // empty). We can't block the user regardless of the error status.
    sb_service_->HandleGetHashResults(check, full_hashes, can_cache);

    hash_requests_.erase(it);
  } else {
    // Update, chunk or key response.
    DCHECK(source == request_.get());
    fetcher.reset(request_.release());

    if (response_code == 200) {
      // We have data from the SafeBrowsing service.
      parsed_ok = HandleServiceResponse(source->url(),
                                        data.data(),
                                        static_cast<int>(data.length()));
      if (!parsed_ok) {
        SB_DLOG(INFO) << "SafeBrowsing request for: " << source->url()
                      << "failed parse.";
        must_back_off = true;
        chunk_request_urls_.clear();
        UpdateFinished(false);
      }

      switch (request_type_) {
        case CHUNK_REQUEST:
          if (parsed_ok)
            chunk_request_urls_.pop_front();
          break;
        case GETKEY_REQUEST:
          if (initial_request_) {
            // This is the first request we've made this session. Now that we
            // have the keys, do the regular update request.
            initial_request_ = false;
            GetNextUpdate();
            return;
          }
          break;
        case UPDATE_REQUEST:
          if (chunk_request_urls_.empty() && parsed_ok) {
            // We are up to date since the servers gave us nothing new, so we
            // are done with this update cycle.
            UpdateFinished(true);
          }
          break;
        default:
          NOTREACHED();
          break;
      }

    } else if (response_code >= 300) {
      // The SafeBrowsing service error: back off.
      must_back_off = true;
      if (request_type_ == CHUNK_REQUEST)
        chunk_request_urls_.clear();
      UpdateFinished(false);
      SB_DLOG(INFO) << "SafeBrowsing request for: " << source->url()
                    << ", failed with error: " << response_code;
    }
  }

  // Schedule a new update request if we've finished retrieving all the chunks
  // from the previous update. We treat the update request and the chunk URLs it
  // contains as an atomic unit as far as back off is concerned.
  if (chunk_request_urls_.empty() &&
      (request_type_ == CHUNK_REQUEST || request_type_ == UPDATE_REQUEST))
    ScheduleNextUpdate(must_back_off);

  // Get the next chunk if available.
  IssueChunkRequest();
}

bool SafeBrowsingProtocolManager::HandleServiceResponse(const GURL& url,
                                                        const char* data,
                                                        int length) {
  SafeBrowsingProtocolParser parser;

  switch (request_type_) {
    case UPDATE_REQUEST: {
      int next_update_sec = -1;
      bool re_key = false;
      bool reset = false;
      std::vector<SBChunkDelete>* chunk_deletes =
          new std::vector<SBChunkDelete>;
      std::vector<ChunkUrl> chunk_urls;
      if (!parser.ParseUpdate(data, length, client_key_,
                              &next_update_sec, &re_key,
                              &reset, chunk_deletes, &chunk_urls)) {
        delete chunk_deletes;
        return false;
      }

      last_update_ = Time::Now();

      if (update_state_ == FIRST_REQUEST)
        update_state_ = SECOND_REQUEST;
      else if (update_state_ == SECOND_REQUEST)
        update_state_ = NORMAL_REQUEST;

      // New time for the next update.
      if (next_update_sec > 0) {
        next_update_sec_ = next_update_sec;
      } else if (update_state_ == SECOND_REQUEST) {
        next_update_sec_ = base::RandInt(15 * 60, 45 * 60);
      }

      // We need to request a new set of keys for MAC.
      if (re_key)
        HandleReKey();

      // New chunks to download.
      if (!chunk_urls.empty()) {
        UMA_HISTOGRAM_COUNTS(L"SB2.UpdateUrls", chunk_urls.size());
        for (size_t i = 0; i < chunk_urls.size(); ++i)
          chunk_request_urls_.push_back(chunk_urls[i]);
      }

      // Handle the case were the SafeBrowsing service tells us to dump our
      // database.
      if (reset) {
        sb_service_->ResetDatabase();
        delete chunk_deletes;
        return true;
      }

      // Chunks to delete from our storage.
      if (!chunk_deletes->empty())
        sb_service_->HandleChunkDelete(chunk_deletes);
      else
        delete chunk_deletes;

      break;
    }
    case CHUNK_REQUEST: {
      if (sb_service_->new_safe_browsing())
        UMA_HISTOGRAM_TIMES(L"SB2.ChunkRequest",
                            base::Time::Now() - chunk_request_start_);

      const ChunkUrl chunk_url = chunk_request_urls_.front();
      bool re_key = false;
      std::deque<SBChunk>* chunks = new std::deque<SBChunk>;
      UMA_HISTOGRAM_COUNTS(L"SB2.ChunkSize", length);
      update_size_ += length;
      if (!parser.ParseChunk(data, length,
                             client_key_, chunk_url.mac,
                             &re_key, chunks)) {
#ifndef NDEBUG
        std::string data_str;
        data_str.assign(data, length);
        std::string encoded_chunk;
        net::Base64Encode(data, &encoded_chunk);
        SB_DLOG(INFO) << "ParseChunk error for chunk: " << chunk_url.url
                      << ", client_key: " << client_key_
                      << ", wrapped_key: " << wrapped_key_
                      << ", mac: " << chunk_url.mac
                      << ", Base64Encode(data): " << encoded_chunk
                      << ", length: " << length;
#endif
        safe_browsing_util::FreeChunks(chunks);
        delete chunks;
        return false;
      }

      if (re_key)
        HandleReKey();

      if (chunks->empty()) {
        delete chunks;
      } else {
        chunk_pending_to_write_ = true;
        sb_service_->HandleChunk(chunk_url.list_name, chunks);
      }

      break;
    }
    case GETKEY_REQUEST: {
      std::string client_key, wrapped_key;
      if (!parser.ParseNewKey(data, length, &client_key, &wrapped_key))
        return false;

      client_key_ = client_key;
      wrapped_key_ = wrapped_key;
      notify_loop_->PostTask(FROM_HERE, NewRunnableMethod(
          sb_service_, &SafeBrowsingService::OnNewMacKeys, client_key_,
          wrapped_key_));
      break;
    }

    default:
      return false;
  }

  return true;
}

void SafeBrowsingProtocolManager::Initialize() {
  // Don't want to hit the safe browsing servers on build/chrome bots.
  if (base::SysInfo::HasEnvVar(env_vars::kHeadless))
    return;

  ScheduleNextUpdate(false /* no back off */);
}

void SafeBrowsingProtocolManager::ScheduleNextUpdate(bool back_off) {
  DCHECK(next_update_sec_ > 0);

  // Unschedule any current timer.
  update_timer_.Stop();

  // Reschedule with the new update.
  const int next_update = GetNextUpdateTime(back_off);
  update_timer_.Start(TimeDelta::FromMilliseconds(next_update), this,
                      &SafeBrowsingProtocolManager::GetNextUpdate);
}

// According to section 5 of the SafeBrowsing protocol specification, we must
// back off after a certain number of errors. We only change 'next_update_sec_'
// when we receive a response from the SafeBrowsing service.
int SafeBrowsingProtocolManager::GetNextUpdateTime(bool back_off) {
  int next = next_update_sec_;
  if (back_off) {
    next = GetNextBackOffTime(&update_error_count_, &update_back_off_mult_);
  } else {
    // Successful response means error reset.
    update_error_count_ = 0;
    update_back_off_mult_ = 1;
  }
  return next * 1000;  // milliseconds
}

int SafeBrowsingProtocolManager::GetNextBackOffTime(int* error_count,
                                                    int* multiplier) {
  DCHECK(multiplier && error_count);
  (*error_count)++;
  if (*error_count > 1 && *error_count < 6) {
    int next = static_cast<int>(*multiplier * (1 + back_off_fuzz_) * 30 * 60);
    *multiplier *= 2;
    if (*multiplier > kSbMaxBackOff)
      *multiplier = kSbMaxBackOff;
    return next;
  }

  if (*error_count >= 6)
    return 60 * 60 * 8;  // 8 hours

  return 60;  // 1 minute
}

// This request requires getting a list of all the chunks for each list from the
// database asynchronously. The request will be issued when we're called back in
// OnGetChunksComplete.
// TODO(paulg): We should get this at start up and maintain a ChunkRange cache
//              to avoid hitting the database with each update request. On the
//              otherhand, this request will only occur ~20-30 minutes so there
//              isn't that much overhead. Measure!
void SafeBrowsingProtocolManager::IssueUpdateRequest() {
  request_type_ = UPDATE_REQUEST;
  sb_service_->UpdateStarted();
}

void SafeBrowsingProtocolManager::IssueChunkRequest() {
  // We are only allowed to have one request outstanding at any time.  Also,
  // don't get the next url until the previous one has been written to disk so
  // that we don't use too much memory.
  if (request_.get() || chunk_request_urls_.empty() || chunk_pending_to_write_)
    return;

  ChunkUrl next_chunk = chunk_request_urls_.front();
  DCHECK(!next_chunk.url.empty());
  if (!StartsWithASCII(next_chunk.url, "http://", false) &&
      !StartsWithASCII(next_chunk.url, "https://", false))
    next_chunk.url = "http://" + next_chunk.url;
  GURL chunk_url(next_chunk.url);
  request_type_ = CHUNK_REQUEST;
  request_.reset(new URLFetcher(chunk_url, URLFetcher::GET, this));
  request_->set_load_flags(net::LOAD_DISABLE_CACHE);
  request_->set_request_context(Profile::GetDefaultRequestContext());
  chunk_request_start_ = base::Time::Now();
  request_->Start();
}

void SafeBrowsingProtocolManager::IssueKeyRequest() {
  GURL key_url(StringPrintf(kSbNewKeyUrl,
                            kSbClientName,
                            version_.c_str()));
  request_type_ = GETKEY_REQUEST;
  request_.reset(new URLFetcher(key_url, URLFetcher::GET, this));
  request_->set_load_flags(net::LOAD_DISABLE_CACHE);
  request_->set_request_context(Profile::GetDefaultRequestContext());
  request_->Start();
}

void SafeBrowsingProtocolManager::OnGetChunksComplete(
    const std::vector<SBListChunkRanges>& lists, bool database_error) {
  DCHECK(request_type_ == UPDATE_REQUEST);

  if (database_error) {
    ScheduleNextUpdate(false);
    return;
  }

  const bool use_mac = !client_key_.empty();

  // Format our stored chunks:
  std::string list_data;
  bool found_malware = false;
  bool found_phishing = false;
  for (size_t i = 0; i < lists.size(); ++i) {
    list_data.append(FormatList(lists[i], use_mac));
    if (lists[i].name == safe_browsing_util::kPhishingList)
      found_phishing = true;

    if (lists[i].name == safe_browsing_util::kMalwareList)
      found_malware = true;
  }

  // If we have an empty database, let the server know we want data for these
  // lists.
  if (!found_phishing)
    list_data.append(FormatList(
        SBListChunkRanges(safe_browsing_util::kPhishingList), use_mac));

  if (!found_malware)
    list_data.append(FormatList(
        SBListChunkRanges(safe_browsing_util::kMalwareList), use_mac));

  std::string url = StringPrintf(kSbUpdateUrl,
                                 kSbClientName,
                                 version_.c_str());
  if (use_mac) {
    url.append("&wrkey=");
    url.append(wrapped_key_);
  }

  GURL update_url(url);
  request_.reset(new URLFetcher(update_url, URLFetcher::POST, this));
  request_->set_load_flags(net::LOAD_DISABLE_CACHE);
  request_->set_request_context(Profile::GetDefaultRequestContext());
  request_->set_upload_data("text/plain", list_data);
  request_->Start();
}

void SafeBrowsingProtocolManager::OnChunkInserted() {
  chunk_pending_to_write_ = false;

  if (chunk_request_urls_.empty()) {
    // Don't pollute old implementation histograms with new implemetation data.
    if (sb_service_->new_safe_browsing())
      UMA_HISTOGRAM_LONG_TIMES(L"SB2.Update", Time::Now() - last_update_);
    else
      UMA_HISTOGRAM_LONG_TIMES(L"SB.Update", Time::Now() - last_update_);
    UpdateFinished(true);
  } else {
    IssueChunkRequest();
  }
}

void SafeBrowsingProtocolManager::ReportMalware(const GURL& malware_url,
                                                const GURL& page_url,
                                                const GURL& referrer_url) {
  std::string report_str = StringPrintf(
      kSbMalwareReportUrl,
      EscapeQueryParamValue(malware_url.spec()).c_str(),
      EscapeQueryParamValue(page_url.spec()).c_str(),
      EscapeQueryParamValue(referrer_url.spec()).c_str(),
      kSbClientName,
      version_.c_str());
  GURL report_url(report_str);
  URLFetcher* report = new URLFetcher(report_url, URLFetcher::GET, this);
  report->set_load_flags(net::LOAD_DISABLE_CACHE);
  report->set_request_context(Profile::GetDefaultRequestContext());
  report->Start();
  malware_reports_.insert(report);
}

// static
std::string SafeBrowsingProtocolManager::FormatList(
    const SBListChunkRanges& list, bool use_mac) {
  std::string formatted_results;
  formatted_results.append(list.name);
  formatted_results.append(";");
  if (!list.adds.empty()) {
    formatted_results.append("a:" + list.adds);
    if (!list.subs.empty() || use_mac)
      formatted_results.append(":");
  }
  if (!list.subs.empty()) {
    formatted_results.append("s:" + list.subs);
    if (use_mac)
      formatted_results.append(":");
  }
  if (use_mac)
    formatted_results.append("mac");
  formatted_results.append("\n");

  return formatted_results;
}

void SafeBrowsingProtocolManager::HandleReKey() {
  client_key_.clear();
  wrapped_key_.clear();
  IssueKeyRequest();
}

void SafeBrowsingProtocolManager::HandleGetHashError() {
  int next = GetNextBackOffTime(&gethash_error_count_, &gethash_back_off_mult_);
  next_gethash_time_ = Time::Now() + TimeDelta::FromSeconds(next);
}

void SafeBrowsingProtocolManager::UpdateFinished(bool success) {
  UMA_HISTOGRAM_COUNTS(L"SB2.UpdateSize", update_size_);
  update_size_ = 0;
  sb_service_->UpdateFinished(success);
}
