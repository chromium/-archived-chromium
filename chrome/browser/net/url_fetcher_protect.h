// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements backoff in the suggest system so that we don't
// DOS the Suggest servers when using URLFetcher.

#ifndef CHROME_BROWSER_URL_FETCHER_PROTECT_H__
#define CHROME_BROWSER_URL_FETCHER_PROTECT_H__

#include <map>
#include <queue>
#include <string>

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "base/time.h"


// This class is used to manage one service's rate protection. It maintains
// a queue of connection successes and failures and analyzes the requests
// over some period of time, in order to deduce the backoff time of every
// request.
// The backoff algorithm consists of two parts. Firstly, avoid too many
// send events in a sliding window. That will prevent traffic overload.
// Secondly, exponential backoff is used when receiving an error message
// from server. Exponential backoff period is calculated using the following
// formula:
//
//             initial backoff time      (the first time to receive error)
//  backoff =  k * current_backoff + c   (the second, third, ... error)
//             maximum backoff time      (when backoff > maximum backoff time)
//
// where |k| is the multiplier, and |c| is the constant factor.
class URLFetcherProtectEntry {
 public:
  enum EventType {
    SEND,      // request will be sent out
    SUCCESS,   // successful response
    FAILURE    // no response or error
  };

  URLFetcherProtectEntry();
  URLFetcherProtectEntry(int sliding_window_period, int max_send_threshold,
                         int max_retries, int initial_timeout,
                         double multiplier, int constant_factor,
                         int maximum_timeout);


  virtual ~URLFetcherProtectEntry() { }

  // When a connection event happens, log it to the queue, and recalculate
  // the timeout period. It returns the backoff time, in milliseconds, that
  // indicates to the sender how long should it wait before sending the request.
  // If the request is allowed to be sent immediately, the backoff time is 0.
  int UpdateBackoff(EventType event_type);

  // Returns the max retries allowed.
  int max_retries() const {
    return max_retries_;
  }

 private:
  // When a request comes, calculate the release time for it.
  // Returns the backoff time before sending.
  base::TimeDelta AntiOverload();
  // Resets backoff when service is ok.
  // Returns the backoff time before sending.
  base::TimeDelta ResetBackoff();
  // Calculates new backoff when encountering a failure.
  // Returns the backoff time before sending.
  base::TimeDelta IncreaseBackoff();

  // Default parameters.  Time is in milliseconds.
  static const int kDefaultSlidingWindowPeriod;
  static const int kDefaultMaxSendThreshold;
  static const int kDefaultMaxRetries;
  static const int kDefaultInitialTimeout;
  static const double kDefaultMultiplier;
  static const int kDefaultConstantFactor;
  static const int kDefaultMaximumTimeout;

  // time to consider events when checking backoff
  int sliding_window_period_;

  // maximum number of requests allowed in sliding window period
  int max_send_threshold_;
  // maximum retris allowed
  int max_retries_;

  // initial timeout on first failure
  int initial_timeout_;
  // factor by which to multiply on exponential backoff (e.g., 2.0)
  double multiplier_;
  // constant time term to add to each attempt
  int constant_factor_;
  // maximum amount of time between requests
  int maximum_timeout_;

  // current exponential backoff period
  int timeout_period_;
  // time that protection is scheduled to end
  base::TimeTicks release_time_;

  // Sets up a lock to ensure thread safe.
  Lock lock_;

  // A list of the recent send events. We ues them to decide whether
  // there are too many requests sent in sliding window.
  std::queue<base::TimeTicks> send_log_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherProtectEntry);
};


// This singleton class is used to manage all protect entries.
// Now we use the host name as service id.
class URLFetcherProtectManager {
 public:
  ~URLFetcherProtectManager();

  // Returns the global instance of this class.
  static URLFetcherProtectManager* GetInstance();

  // Registers a new entry in this service. If the entry already exists,
  // just returns it.
  URLFetcherProtectEntry* Register(std::string id);
  // Always registers the entry even when it exists.
  URLFetcherProtectEntry* Register(std::string id,
                                   URLFetcherProtectEntry* entry);

 private:
  URLFetcherProtectManager() { }

  typedef std::map<const std::string, URLFetcherProtectEntry*> ProtectService;

  static Lock lock_;
  static scoped_ptr<URLFetcherProtectManager> protect_manager_;
  ProtectService services_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherProtectManager);
};

#endif  // CHROME_BROWSER_URL_FETCHER_PROTECT_H__
