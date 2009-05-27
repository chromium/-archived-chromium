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


// Implements metrics and metrics collections
#include "metrics.h"

#ifndef OS_WIN
#include <pthread.h>
#else  // OS_WIN
#include "lock.h"
#endif  // OS_WIN

namespace stats_report {
// Make sure global stats collection is placed in zeroed storage so as to avoid
// initialization order snafus.
MetricCollectionBase g_global_metric_storage = { 0, 0 };
MetricCollection &g_global_metrics =
    *static_cast<MetricCollection*>(&g_global_metric_storage);

#pragma warning(push)
// C4640: construction of local static object is not thread-safe.
// C4073: initializers put in library initialization area.
#pragma warning(disable : 4640 4073)

// Serialize all metric manipulation and access under this lock.
#ifdef OS_WIN
// Initializes g_lock before other global objects of user defined types.
// It assumes the program is single threaded while executing CRT startup and
// exit code.
#pragma init_seg(lib)
LLock g_lock;
#pragma warning(pop)
#else  // OS_WIN
// On non-Windowsen we use a link time initialized pthread mutex.
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
#endif  // OS_WIN

class MetricBase::ObjectLock {
 public:
  explicit ObjectLock(const MetricBase *metric) : metric_(metric) {
    metric_->Lock();
  }

  ~ObjectLock() {
    metric_->Unlock();
  }

 private:
  MetricBase const *const metric_;
  DISALLOW_COPY_AND_ASSIGN(ObjectLock);
};

void MetricBase::Lock() const {
#ifdef OS_WIN
  g_lock.Lock();
#else  // OS_WIN
  pthread_mutex_lock(&g_lock);
#endif  // OS_WIN
}

void MetricBase::Unlock() const {
#ifdef OS_WIN
  g_lock.Unlock();
#else  // OS_WIN
  pthread_mutex_unlock(&g_lock);
#endif  // OS_WIN
}

MetricBase::MetricBase(const char *name,
                       MetricType type,
                       MetricCollectionBase *coll)
    : name_(name), type_(type), next_(coll->first_), coll_(coll) {
  DCHECK_NE(static_cast<MetricCollectionBase*>(NULL), coll_);
  DCHECK_EQ(false, coll_->initialized_);
  coll->first_ = this;
}

MetricBase::MetricBase(const char *name, MetricType type)
    : name_(name), type_(type), next_(NULL), coll_(NULL) {
}

MetricBase::~MetricBase() {
  if (coll_) {
    DCHECK_EQ(this, coll_->first_);
    LOG_IF(WARNING, coll_->initialized_)
        << "Warning: Metric destructor called without call to Uninitialize().";

    coll_->first_ = next_;
  } else {
    DCHECK(NULL == next_);
  }
}

void IntegerMetricBase::Set(uint64 value) {
  ObjectLock lock(this);
  value_ = value;
}

uint64 IntegerMetricBase::value() const {
  ObjectLock lock(this);
  uint64 ret = value_;
  return ret;
}

void IntegerMetricBase::Increment() {
  ObjectLock lock(this);
  ++value_;
}

void IntegerMetricBase::Decrement() {
  ObjectLock lock(this);
  --value_;
}

void IntegerMetricBase::Add(uint64 value) {
  ObjectLock lock(this);
  value_ += value;
}

void IntegerMetricBase::Subtract(uint64 value) {
  ObjectLock lock(this);
  if (value_ < value)
    value_ = 0;
  else
    value_ -= value;
}

uint64 CountMetric::Reset() {
  ObjectLock lock(this);
  uint64 ret = value_;
  value_ = 0;
  return ret;
}

TimingMetric::TimingData TimingMetric::Reset() {
  ObjectLock lock(this);
  TimingData ret = data_;
  Clear();
  return ret;
}

uint32 TimingMetric::count() const {
  ObjectLock lock(this);
  uint32 ret = data_.count;
  return ret;
}

uint64 TimingMetric::sum() const {
  ObjectLock lock(this);
  uint64 ret = data_.sum;
  return ret;
}

uint64 TimingMetric::minimum() const {
  ObjectLock lock(this);
  uint64 ret = data_.minimum;
  return ret;
}

uint64 TimingMetric::maximum() const {
  ObjectLock lock(this);
  uint64 ret = data_.maximum;
  return ret;
}

uint64 TimingMetric::average() const {
  ObjectLock lock(this);

  uint64 ret = 0;
  if (0 == data_.count) {
    DCHECK_EQ(0, data_.sum);
  } else {
    ret = data_.sum / data_.count;
  }
  return ret;
}

void TimingMetric::AddSample(uint64 time_ms) {
  ObjectLock lock(this);
  if (0 == data_.count) {
    data_.minimum = time_ms;
    data_.maximum = time_ms;
  } else {
    if (data_.minimum > time_ms)
      data_.minimum = time_ms;
    if (data_.maximum < time_ms)
      data_.maximum = time_ms;
  }
  data_.count++;
  data_.sum += time_ms;
}

void TimingMetric::AddSamples(uint64 count, uint64 total_time_ms) {
  if (0 == count)
    return;

  uint64 time_ms = total_time_ms / count;

  ObjectLock lock(this);
  if (0 == data_.count) {
    data_.minimum = time_ms;
    data_.maximum = time_ms;
  } else {
    if (data_.minimum > time_ms)
      data_.minimum = time_ms;
    if (data_.maximum < time_ms)
      data_.maximum = time_ms;
  }

  // TODO: truncation from 64 to 32 may occur here.
  DCHECK_LE(count, kuint32max);
  data_.count += static_cast<uint32>(count);
  data_.sum += total_time_ms;
}

void TimingMetric::Clear() {
  memset(&data_, 0, sizeof(data_));
}

void BoolMetric::Set(bool value) {
  ObjectLock lock(this);
  value_ = value ? kBoolTrue : kBoolFalse;
}

BoolMetric::TristateBoolValue BoolMetric::Reset() {
  ObjectLock lock(this);
  TristateBoolValue ret = value_;
  value_ = kBoolUnset;
  return ret;
}

void MetricCollection::Initialize() {
  DCHECK(!initialized());
  initialized_ = true;
}

void MetricCollection::Uninitialize() {
  DCHECK(initialized());
  initialized_ = false;
}


}  // namespace stats_report
