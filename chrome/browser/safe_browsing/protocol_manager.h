// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_PROTOCOL_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_PROTOCOL_MANAGER_H_

// A class that implements Chrome's interface with the SafeBrowsing protocol.
// The SafeBrowsingProtocolManager handles formatting and making requests of,
// and handling responses from, Google's SafeBrowsing servers. This class uses
// The SafeBrowsingProtocolParser class to do the actual parsing.

#include <deque>
#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/url_fetcher.h"
#include "chrome/browser/safe_browsing/chunk_range.h"
#include "chrome/browser/safe_browsing/protocol_parser.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "net/url_request/url_request.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class MessageLoop;
class Task;
class Timer;


class SafeBrowsingProtocolManager : public URLFetcher::Delegate {
  // Testing friends:
  FRIEND_TEST(SafeBrowsingProtocolManagerTest, TestBackOffTimes);
  FRIEND_TEST(SafeBrowsingProtocolManagerTest, TestChunkStrings);
  FRIEND_TEST(SafeBrowsingProtocolManagerTest, DISABLED_TestGetHashBackOffTimes);

 public:
  SafeBrowsingProtocolManager(SafeBrowsingService* sb_service,
                              MessageLoop* notify_loop,
                              const std::string& client_key,
                              const std::string& wrapped_key);
  ~SafeBrowsingProtocolManager();

  // Set up the update schedule and internal state for making periodic requests
  // of the SafeBrowsing service.
  void Initialize();

  // URLFetcher::Delegate interface.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // API used by the SafeBrowsingService for issuing queries. When the results
  // are available, SafeBrowsingService::HandleGetHashResults is called.
  void GetFullHash(SafeBrowsingService::SafeBrowsingCheck* check,
                   const std::vector<SBPrefix>& prefixes);

  // Scheduled update callback.
  void GetNextUpdate();

  // Called by the SafeBrowsingService when our request for a list of all chunks
  // for each list is done.  If database_error is true, that means the protocol
  // manager shouldn't fetch updates since they can't be written to disk.  It
  // should try again later to open the database.
  void OnGetChunksComplete(const std::vector<SBListChunkRanges>& list,
                           bool database_error);

  // Called after the chunks that were parsed were inserted in the database.
  void OnChunkInserted();

  // The last time we received an update.
  base::Time last_update() const { return last_update_; }

 private:
  // Internal API for fetching information from the SafeBrowsing servers. The
  // GetHash requests are higher priority since they can block user requests
  // so are handled separately.
  enum SafeBrowsingRequestType {
    NO_REQUEST = 0,     // No requests in progress
    UPDATE_REQUEST,   // Request for redirect URLs
    CHUNK_REQUEST,      // Request for a specific chunk
    GETKEY_REQUEST      // Update the client's MAC key
  };

  // Returns the time (in milliseconds) for the next update request. If
  // 'back_off' is true, the time returned will increment an error count and
  // return the appriate next time (see ScheduleNextUpdate below).
  int GetNextUpdateTime(bool back_off);

  // Worker function for calculating GetHash and Update backoff times (in
  // seconds). 'Multiplier' is doubled for each consecutive error between the
  // 2nd and 5th, and 'error_count' is incremented with each call.
  int GetNextBackOffTime(int* error_count, int* multiplier);

  // Manage our update with the next allowable update time. If 'back_off_' is
  // true, we must decrease the frequency of requests of the SafeBrowsing
  // service according to section 5 of the protocol specification.
  void ScheduleNextUpdate(bool back_off);

  // Send a request for a list of chunks we should download to the SafeBrowsing
  // servers. In order to format this request, we need to send all the chunk
  // numbers for each list that we have to the server. Getting the chunk numbers
  // requires a database query (run on the database thread), and the request
  // is sent upon completion of that query in OnGetChunksComplete.
  void IssueUpdateRequest();

  // Send a request for a chunk to the SafeBrowsing servers.
  void IssueChunkRequest();

  // Get a key from the SafeBrowsing servers for use with MAC. This should only
  // be called once per client unless the server directly tells us to update.
  void IssueKeyRequest();

  // Format a string returned from the database into:
  //   "list_name;a:<add_chunk_ranges>:s:<sub_chunk_ranges>:mac\n"
  static std::string FormatList(const SBListChunkRanges& list, bool use_mac);

  // Run the protocol parser on received data and update the SafeBrowsingService
  // with the new content. Returns 'true' on successful parse, 'false' on error.
  bool HandleServiceResponse(const GURL& url, const char* data, int length);

  // If the SafeBrowsing service wants us to re-key, we clear our key state and
  // issue the request.
  void HandleReKey();

  // Update internal state for each GetHash response error.
  void HandleGetHashError();

  // Helper function for update completion.
  void UpdateFinished(bool success);

 private:
  // Main SafeBrowsing interface object.
  SafeBrowsingService* sb_service_;

  // Current active request (in case we need to cancel) for updates or chunks
  // from the SafeBrowsing service. We can only have one of these outstanding
  // at any given time unlike GetHash requests, which are tracked separately.
  scoped_ptr<URLFetcher> request_;

  // The kind of request that is currently in progress.
  SafeBrowsingRequestType request_type_;

  // The number of HTTP response errors, used for request backoff timing.
  int update_error_count_;
  int gethash_error_count_;

  // Multipliers which double (max == 8) for each error after the second.
  int update_back_off_mult_;
  int gethash_back_off_mult_;

  // Multiplier between 0 and 1 to spread clients over an interval.
  float back_off_fuzz_;

  // The list for which we are make a request.
  std::string list_name_;

  // For managing the next earliest time to query the SafeBrowsing servers for
  // updates.
  int next_update_sec_;
  base::OneShotTimer<SafeBrowsingProtocolManager> update_timer_;

  // All chunk requests that need to be made, along with their MAC.
  std::deque<ChunkUrl> chunk_request_urls_;

  // Map of GetHash requests.
  typedef base::hash_map<const URLFetcher*,
                         SafeBrowsingService::SafeBrowsingCheck*> HashRequests;
  HashRequests hash_requests_;

  // The next scheduled update has special behavior for the first 2 requests.
  enum UpdateRequestState {
    FIRST_REQUEST = 0,
    SECOND_REQUEST,
    NORMAL_REQUEST
  };
  UpdateRequestState update_state_;

  // We'll attempt to get keys once per browser session if we don't already have
  // them. They are not essential to operation, but provide a layer of
  // verification.
  bool initial_request_;

  // True if the service has been given an add/sub chunk but it hasn't been
  // added to the database yet.
  bool chunk_pending_to_write_;

  // Message loop for forwarding MAC keys to the SafeBrowsingService for
  // storage.
  MessageLoop* notify_loop_;

  // The keys used for MAC. Empty keys mean we aren't using MAC.
  std::string client_key_;
  std::string wrapped_key_;

  // The last time we successfully received an update.
  base::Time last_update_;

  // While in GetHash backoff, we can't make another GetHash until this time.
  base::Time next_gethash_time_;

  // Current product version sent in each request.
  std::string version_;

  // Used for measuring chunk request latency.
  base::Time chunk_request_start_;

  // Track the size of each update (in bytes).
  int update_size_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingProtocolManager);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_PROTOCOL_MANAGER_H_
