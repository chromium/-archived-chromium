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


// Implementation of a Posix metrics aggregator.

#include "aggregator-posix.h"
#include "const-posix.h"
#include "formatter.h"
#include "util/endian/endian.h"
#include "statsreport/common/pathhelpers.h"

namespace stats_report {

MetricsAggregatorPosix::MetricsAggregatorPosix(const MetricCollection &coll) {
  key_value_table_.reset(new KeyValueTable(get_cache_dir() + "stats.sqlite3",
                                           "stats"));
}

MetricsAggregatorPosix::~MetricsAggregatorPosix() {
}

void MetricsAggregatorPosix::ResetMetrics() {
  StartAggregation();
  (void)transaction_->Clear();
  EndAggregation();
}

namespace {

void AddMetric(const string& key,
               const ScopedStatement &statement,
               void *ref_con) {
  Formatter *formatter =
      static_cast<Formatter *>(ref_con);

  string original_key;
  scoped_ptr<MetricBase> metric;

  if (key.compare(0, kCountsKeyName.length(), kCountsKeyName) == 0) {
    int64 value;
    GetColumn(statement.get(), 0, &value);
    original_key = key.substr(kCountsKeyName.length());
    metric.reset(new CountMetric(original_key.c_str(), value));
  } else if (key.compare(0, kTimingsKeyName.length(), kTimingsKeyName) == 0) {
    std::vector<BYTE> value_bytes;
    TimingMetric::TimingData value;
    GetColumn(statement.get(), 0, &value_bytes);
    memcpy(&value, &value_bytes[0], value_bytes.size());
    value.count   = gntohl(value.count);
    value.sum     = gntohll(value.sum);
    value.minimum = gntohll(value.minimum);
    value.maximum = gntohll(value.maximum);
    metric.reset(new TimingMetric(original_key.c_str(), value));
  } else if (key.compare(0, kIntegersKeyName.length(), kIntegersKeyName) == 0) {
    int64 value;
    GetColumn(statement.get(), 0, &value);
    original_key = key.substr(kIntegersKeyName.length());
    metric.reset(new IntegerMetric(original_key.c_str(), value));
  } else if (key.compare(0, kBooleansKeyName.length(), kBooleansKeyName) == 0) {
    int32 value;
    GetColumn(statement.get(), 0, &value);
    original_key = key.substr(kBooleansKeyName.length());
    metric.reset(new BoolMetric(original_key.c_str(), value));
  }

  if (!original_key.empty())
    formatter->AddMetric(&*metric);
}

}  // namespace

void MetricsAggregatorPosix::FormatMetrics(Formatter *formatter) {
  StartAggregation();
  transaction_->Iterate(AddMetric, formatter);
  EndAggregation();
}

bool MetricsAggregatorPosix::StartAggregation() {
  transaction_.reset(new KeyValueTransaction(&*key_value_table_));

  return true;
}

void MetricsAggregatorPosix::EndAggregation() {
  transaction_.reset();
}

void MetricsAggregatorPosix::Aggregate(CountMetric *metric) {
  // do as little as possible if no value
  uint64 value = metric->Reset();
  if (0 == value)
    return;

  string name(metric->name());
  name.insert(0, kCountsKeyName);
  int64 reg_value = 0;  // yes, we only have 63 positive bits here :(
  if (!transaction_->Get(name, &reg_value)) {
    // TODO: clean up??
  }
  reg_value += value;

  (void)transaction_->Put(name, reg_value);
}

void MetricsAggregatorPosix::Aggregate(TimingMetric *metric) {
  // do as little as possible if no value
  TimingMetric::TimingData value = metric->Reset();
  if (0 == value.count)
    return;

  string name(metric->name());
  name.insert(0, kTimingsKeyName);
  std::vector<BYTE> reg_value_bytes;
  TimingMetric::TimingData reg_value;
  if (!transaction_->Get(name, &reg_value_bytes)) {
    reg_value_bytes.resize(sizeof(reg_value));
    memcpy(&reg_value, &value, sizeof(value));
  } else {
    memcpy(&reg_value, &reg_value_bytes[0], reg_value_bytes.size());
    reg_value.count   = gntohl(reg_value.count);
    reg_value.sum     = gntohll(reg_value.sum);
    reg_value.minimum = gntohll(reg_value.minimum);
    reg_value.maximum = gntohll(reg_value.maximum);

    reg_value.count += value.count;
    reg_value.sum += value.sum;
    reg_value.minimum = std::min(reg_value.minimum, value.minimum);
    reg_value.maximum = std::max(reg_value.maximum, value.maximum);
  }

  reg_value.count   = ghtonl(reg_value.count);
  reg_value.sum     = ghtonll(reg_value.sum);
  reg_value.minimum = ghtonll(reg_value.minimum);
  reg_value.maximum = ghtonll(reg_value.maximum);

  memcpy(&reg_value_bytes[0], &reg_value, sizeof(reg_value));
  (void)transaction_->Put(name, reg_value_bytes);
}

void MetricsAggregatorPosix::Aggregate(IntegerMetric *metric) {
  // do as little as possible if no value
  int64 value = metric->value();  // yes, we only have 63 positive bits here :(
  if (0 == value)
    return;

  string name(metric->name());
  name.insert(0, kIntegersKeyName);

  (void)transaction_->Put(name, value);
}

void MetricsAggregatorPosix::Aggregate(BoolMetric *metric) {
  // do as little as possible if no value
  int32 value = metric->Reset();
  if (BoolMetric::kBoolUnset == value)
    return;

  string name(metric->name());
  name.insert(0, kBooleansKeyName);

  (void)transaction_->Put(name, value);
}

}  // namespace stats_report
