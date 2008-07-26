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
//
// This file implements backoff in the suggest system so that we don't
// DOS the Suggest servers when using URLFetcher.

#ifndef CHROME_BROWSER_URL_FETCHER_PROTECT_H__
#define CHROME_BROWSER_URL_FETCHER_PROTECT_H__

#include <map>
#include <queue>
#include <string>

#include "base/lock.h"
#include "base/logging.h"
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
class ProtectEntry {
 public:
  enum EventType {
    SEND,      // request will be sent out
    SUCCESS,   // successful response
    FAILURE    // no response or error
  };

  ProtectEntry();
  ProtectEntry(int sliding_window_period, int max_send_threshold,
               int max_retries, int initial_timeout,
               double multiplier, int constant_factor,
               int maximum_timeout);


  virtual ~ProtectEntry() { }

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
  TimeDelta AntiOverload();
  // Resets backoff when service is ok.
  // Returns the backoff time before sending.
  TimeDelta ResetBackoff();
  // Calculates new backoff when encountering a failure.
  // Returns the backoff time before sending.
  TimeDelta IncreaseBackoff();

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
  TimeTicks release_time_;

  // Sets up a lock to ensure thread safe.
  Lock lock_;

  // A list of the recent send events. We ues them to decide whether
  // there are too many requests sent in sliding window.
  std::queue<TimeTicks> send_log_;

  DISALLOW_EVIL_CONSTRUCTORS(ProtectEntry);
};


// This singleton class is used to manage all protect entries.
// Now we use the host name as service id.
class ProtectManager {
 public:
  ~ProtectManager();

  // Returns the global instance of this class.
  static ProtectManager* GetInstance();

  // Registers a new entry in this service. If the entry already exists,
  // just returns it.
  ProtectEntry* Register(std::string id);
  // Always registers the entry even when it exists.
  ProtectEntry* Register(std::string id, ProtectEntry* entry);

 private:
  ProtectManager() { }

  typedef std::map<const std::string, ProtectEntry*> ProtectService;

  static Lock lock_;
  static scoped_ptr<ProtectManager> protect_manager_;
  ProtectService services_;

  DISALLOW_EVIL_CONSTRUCTORS(ProtectManager);
};

#endif  // CHROME_BROWSER_URL_FETCHER_PROTECT_H__
