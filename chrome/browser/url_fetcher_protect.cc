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

#include "chrome/browser/url_fetcher_protect.h"

// ProtectEntry --------------------------------------------------------------


// Default parameters.  Time is in milliseconds.
// static
const int ProtectEntry::kDefaultSlidingWindowPeriod = 2000;

const int ProtectEntry::kDefaultMaxSendThreshold = 20;
const int ProtectEntry::kDefaultMaxRetries = 0;

const int ProtectEntry::kDefaultInitialTimeout = 100;
const double ProtectEntry::kDefaultMultiplier = 2.0;
const int ProtectEntry::kDefaultConstantFactor = 100;
const int ProtectEntry::kDefaultMaximumTimeout = 60000;


ProtectEntry::ProtectEntry()
    : sliding_window_period_(kDefaultSlidingWindowPeriod),
      max_send_threshold_(kDefaultMaxSendThreshold),
      max_retries_(kDefaultMaxRetries),
      initial_timeout_(kDefaultInitialTimeout),
      multiplier_(kDefaultMultiplier),
      constant_factor_(kDefaultConstantFactor),
      maximum_timeout_(kDefaultMaximumTimeout) {
  ResetBackoff();
}

ProtectEntry::ProtectEntry(int sliding_window_period, int max_send_threshold,
                           int max_retries, int initial_timeout,
                           double multiplier, int constant_factor,
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

int ProtectEntry::UpdateBackoff(EventType event_type) {
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

TimeDelta ProtectEntry::AntiOverload() {
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

TimeDelta ProtectEntry::ResetBackoff() {
  timeout_period_ = initial_timeout_;
  release_time_ = TimeTicks::Now();
  return TimeDelta::FromMilliseconds(0);
}

TimeDelta ProtectEntry::IncreaseBackoff() {
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

// ProtectManager ------------------------------------------------------------

// static
scoped_ptr<ProtectManager> ProtectManager::protect_manager_;
Lock ProtectManager::lock_;

ProtectManager::~ProtectManager() {
  // Deletes all entries
  ProtectService::iterator i;
  for (i = services_.begin(); i != services_.end(); ++i) {
    if (i->second)
      delete i->second;
  }
}

// static
ProtectManager* ProtectManager::GetInstance() {
  AutoLock lock(lock_);

  if (protect_manager_.get() == NULL) {
    protect_manager_.reset(new ProtectManager());
  }
  return protect_manager_.get();
}

ProtectEntry* ProtectManager::Register(std::string id) {
  AutoLock lock(lock_);

  ProtectService::iterator i = services_.find(id);

  if (i != services_.end()) {
    // The entry exists.
    return i->second;
  }

  // Creates a new entry.
  ProtectEntry* entry = new ProtectEntry();
  services_[id] = entry;
  return entry;
}

ProtectEntry* ProtectManager::Register(std::string id, ProtectEntry* entry) {
  AutoLock lock(lock_);

  ProtectService::iterator i = services_.find(id);
  if (i != services_.end()) {
    // The entry exists.
    delete i->second;
  }

  services_[id] = entry;
  return entry;
}
