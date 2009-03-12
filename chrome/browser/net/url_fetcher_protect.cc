// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_fetcher_protect.h"

#include "base/logging.h"

// URLFetcherProtectEntry ----------------------------------------------------

using base::TimeDelta;
using base::TimeTicks;

// Default parameters.  Time is in milliseconds.
// static
const int URLFetcherProtectEntry::kDefaultSlidingWindowPeriod = 2000;

const int URLFetcherProtectEntry::kDefaultMaxSendThreshold = 20;
const int URLFetcherProtectEntry::kDefaultMaxRetries = 0;

const int URLFetcherProtectEntry::kDefaultInitialTimeout = 100;
const double URLFetcherProtectEntry::kDefaultMultiplier = 2.0;
const int URLFetcherProtectEntry::kDefaultConstantFactor = 100;
const int URLFetcherProtectEntry::kDefaultMaximumTimeout = 60000;


URLFetcherProtectEntry::URLFetcherProtectEntry()
    : sliding_window_period_(kDefaultSlidingWindowPeriod),
      max_send_threshold_(kDefaultMaxSendThreshold),
      max_retries_(kDefaultMaxRetries),
      initial_timeout_(kDefaultInitialTimeout),
      multiplier_(kDefaultMultiplier),
      constant_factor_(kDefaultConstantFactor),
      maximum_timeout_(kDefaultMaximumTimeout) {
  ResetBackoff();
}

URLFetcherProtectEntry::URLFetcherProtectEntry(int sliding_window_period,
                                               int max_send_threshold,
                                               int max_retries,
                                               int initial_timeout,
                                               double multiplier,
                                               int constant_factor,
                                               int maximum_timeout)
    : sliding_window_period_(sliding_window_period),
      max_send_threshold_(max_send_threshold),
      max_retries_(max_retries),
      initial_timeout_(initial_timeout),
      multiplier_(multiplier),
      constant_factor_(constant_factor),
      maximum_timeout_(maximum_timeout) {
  ResetBackoff();
}

int URLFetcherProtectEntry::UpdateBackoff(EventType event_type) {
  // request may be sent in different threads
  AutoLock lock(lock_);

  TimeDelta t;
  switch (event_type) {
    case SEND:
      t = AntiOverload();
      break;
    case SUCCESS:
      t = ResetBackoff();
      break;
    case FAILURE:
      t = IncreaseBackoff();
      break;
    default:
      NOTREACHED();
  }

  int wait = static_cast<int>(t.InMilliseconds());
  DCHECK(wait >= 0);
  return wait;
}

TimeDelta URLFetcherProtectEntry::AntiOverload() {
  TimeDelta sw = TimeDelta::FromMilliseconds(sliding_window_period_);
  TimeTicks now = TimeTicks::Now();
  // Estimate when the next request will be sent.
  release_time_ = now;
  if (send_log_.size() > 0) {
    release_time_ = std::max(release_time_, send_log_.back());
  }
  // Checks if there are too many send events in recent time.
  if (send_log_.size() >= static_cast<unsigned>(max_send_threshold_)) {
    release_time_ = std::max(release_time_, send_log_.front() + sw);
  }
  // Logs the new send event.
  send_log_.push(release_time_);
  // Drops the out-of-date events in the event list.
  while (!send_log_.empty() &&
         (send_log_.front() + sw <= send_log_.back())) {
    send_log_.pop();
  }
  return release_time_ - now;
}

TimeDelta URLFetcherProtectEntry::ResetBackoff() {
  timeout_period_ = initial_timeout_;
  release_time_ = TimeTicks::Now();
  return TimeDelta::FromMilliseconds(0);
}

TimeDelta URLFetcherProtectEntry::IncreaseBackoff() {
  TimeTicks now = TimeTicks::Now();

  release_time_ = std::max(release_time_, now) +
                  TimeDelta::FromMilliseconds(timeout_period_);

  // Calculates the new backoff time.
  timeout_period_ = static_cast<int>
      (multiplier_ * timeout_period_ + constant_factor_);
  if (maximum_timeout_ && timeout_period_ > maximum_timeout_)
    timeout_period_ = maximum_timeout_;

  return release_time_ - now;
}

// URLFetcherProtectManager --------------------------------------------------

// static
scoped_ptr<URLFetcherProtectManager> URLFetcherProtectManager::protect_manager_;
Lock URLFetcherProtectManager::lock_;

URLFetcherProtectManager::~URLFetcherProtectManager() {
  // Deletes all entries
  ProtectService::iterator i;
  for (i = services_.begin(); i != services_.end(); ++i) {
    if (i->second)
      delete i->second;
  }
}

// static
URLFetcherProtectManager* URLFetcherProtectManager::GetInstance() {
  AutoLock lock(lock_);

  if (protect_manager_.get() == NULL) {
    protect_manager_.reset(new URLFetcherProtectManager());
  }
  return protect_manager_.get();
}

URLFetcherProtectEntry* URLFetcherProtectManager::Register(std::string id) {
  AutoLock lock(lock_);

  ProtectService::iterator i = services_.find(id);

  if (i != services_.end()) {
    // The entry exists.
    return i->second;
  }

  // Creates a new entry.
  URLFetcherProtectEntry* entry = new URLFetcherProtectEntry();
  services_[id] = entry;
  return entry;
}

URLFetcherProtectEntry* URLFetcherProtectManager::Register(
    std::string id, URLFetcherProtectEntry* entry) {
  AutoLock lock(lock_);

  ProtectService::iterator i = services_.find(id);
  if (i != services_.end()) {
    // The entry exists.
    delete i->second;
  }

  services_[id] = entry;
  return entry;
}
