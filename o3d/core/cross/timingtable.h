/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the class definitions for TimingTable and TimingRecord,
// which together make up a quick-and-dirty profiling tool for hand-instrumented
// code.

#ifndef O3D_CORE_CROSS_TIMINGTABLE_H_
#define O3D_CORE_CROSS_TIMINGTABLE_H_

#ifdef PROFILE_CLIENT

#include <map>
#include <string>

#include "core/cross/timer.h"
#include "utils/cross/structured_writer.h"

// A record for keeping track of timing stats for a single section of code,
// identified by a string.  TimingRecord stores enough info to tell you the max,
// min, and mean time that the code segment took, and the number of times it was
// called.  It also reports unfinished and unbegun calls, which are cases in
// which you told it to start or finish recording data, but that call failed to
// have a corresponding finish or start.  Unfinished calls usually mean that you
// failed to instrument a branch exiting the block early.  Unstarted calls are
// probably more serious bugs.
class TimingRecord {
 public:
  TimingRecord() :
      started_(false),
      unfinished_(0),
      unbegun_(0),
      calls_(0),
      time_(0),
      min_time_(LONG_MAX),
      max_time_(0) {
  }

  void Start() {
    if (started_) {
      ++unfinished_;
    }
    started_ = true;
    timer_.GetElapsedTimeAndReset();  // This is how you reset the timer.
  }

  void Stop() {
    if (started_) {
      started_= false;
      ++calls_;
      float time = timer_.GetElapsedTimeAndReset();
      if (time > max_time_) {
        max_time_ = time;
      }
      if (time < min_time_) {
        min_time_ = time;
      }
      time_ += time;
    } else {
      ++unbegun_;
    }
  }

  int UnfinishedCount() const {
    return unfinished_;
  }
  int UnbegunCount() const {
    return unbegun_;
  }
  int CallCount() const {
    return calls_;
  }
  float TimeSpent() const {
    return time_;
  }
  float MinTime() const {
    return min_time_;
  }
  float MaxTime() const {
    return max_time_;
  }
  void Write(o3d::StructuredWriter* writer) const {
    writer->OpenObject();
    writer->WritePropertyName("max");
    writer->WriteFloat(max_time_);
    writer->WritePropertyName("min");
    writer->WriteFloat(min_time_);
    writer->WritePropertyName("mean");
    writer->WriteFloat(calls_ ? time_ / calls_ : 0);
    writer->WritePropertyName("total");
    writer->WriteFloat(time_);
    writer->WritePropertyName("calls");
    writer->WriteInt(calls_);
    if (unfinished_) {
      writer->WritePropertyName("unfinished");
      writer->WriteInt(unfinished_);
    }
    if (unbegun_) {
      writer->WritePropertyName("unbegun");
      writer->WriteInt(unbegun_);
    }
    writer->CloseObject();
  }

 private:
  bool started_;
  int unfinished_;
  int unbegun_;
  int calls_;
  float time_;
  float min_time_;
  float max_time_;
  o3d::ElapsedTimeTimer timer_;
};

// The TimingTable is a quick-and-dirty profiler for hand-instrumented code.
// Don't call its functions directly; wrap them in macros so that they can be
// compiled in optionally.  Currently we use GLUE_PROFILE_START/STOP/etc. in the
// glue code [defined in common.h] and PROFILE_START/STOP/etc. elsewhere
// in the plugin [defined below].
class TimingTable {
 public:
  TimingTable() {
  }

  virtual public ~TimingTable() {}

  virtual void Reset() {
    table_.clear();
  }

  virtual void Start(const o3d::String& key) {
    table_[key].Start();
  }

  virtual void Stop(const o3d::String& key) {
    table_[key].Stop();
  }

  virtual void Write(o3d::StructuredWriter* writer) {
    std::map<o3d::String, TimingRecord>::iterator iter;
    writer->OpenArray();
    for (iter = table_.begin(); iter != table_.end(); ++iter) {
      const TimingRecord& record = iter->second;
      if (record.CallCount() || record.UnfinishedCount() ||
          record.UnbegunCount()) {
        writer->OpenObject();
        writer->WritePropertyName(iter->first);
        iter->second.Write(writer);
        writer->CloseObject();
      }
    }
    writer->CloseArray();
    writer->Close();
  }

 private:
  std::map<o3d::String, TimingRecord> table_;
};

#endif  // PROFILE_CLIENT

#endif  // O3D_CORE_CROSS_TIMINGTABLE_H_
