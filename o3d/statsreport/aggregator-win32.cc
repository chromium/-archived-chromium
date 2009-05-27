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


// Implementation of Win32 metrics aggregator.
#include "aggregator-win32.h"
#include "const-win32.h"
#include "util-win32.h"

namespace stats_report {

MetricsAggregatorWin32::MetricsAggregatorWin32(const MetricCollection &coll,
                                               const wchar_t *key_name)
    : MetricsAggregator(coll),
      is_machine_(false) {
  DCHECK(NULL != key_name);

  key_name_.Format(kStatsKeyFormatString, key_name);
}

MetricsAggregatorWin32::MetricsAggregatorWin32(const MetricCollection &coll,
                                               const wchar_t *key_name,
                                               bool is_machine)
    : MetricsAggregator(coll),
      is_machine_(is_machine) {
  DCHECK(NULL != key_name);

  key_name_.Format(kStatsKeyFormatString, key_name);
}

MetricsAggregatorWin32::~MetricsAggregatorWin32() {
}

bool MetricsAggregatorWin32::StartAggregation() {
  DCHECK(NULL == key_.m_hKey);

  HKEY parent_key = is_machine_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  LONG err = key_.Create(parent_key, key_name_);
  if (err != ERROR_SUCCESS)
    return false;

  return true;
}

void MetricsAggregatorWin32::EndAggregation() {
  count_key_.Close();
  timing_key_.Close();
  integer_key_.Close();
  bool_key_.Close();

  key_.Close();
}

bool MetricsAggregatorWin32::EnsureKey(const wchar_t *name, CRegKey *key) {
  if (NULL != key->m_hKey)
    return true;

  LONG err = key->Create(key_, name);
  if (ERROR_SUCCESS != err) {
    DCHECK(NULL == key->m_hKey);
    // TODO: log?
    return false;
  }

  return true;
}

void MetricsAggregatorWin32::Aggregate(CountMetric *metric) {
  DCHECK(NULL != metric);

  // do as little as possible if no value
  uint64 value = metric->Reset();
  if (0 == value)
    return;

  if (!EnsureKey(kCountsKeyName, &count_key_))
    return;

  CString name(metric->name());
  uint64 reg_value = 0;
  if (!GetData(&count_key_, name, &reg_value)) {
    // TODO: clean up??
  }
  reg_value += value;

  DWORD err = count_key_.SetBinaryValue(name, &reg_value, sizeof(reg_value));
}

void MetricsAggregatorWin32::Aggregate(TimingMetric *metric) {
  DCHECK(NULL != metric);

  // do as little as possible if no value
  TimingMetric::TimingData value = metric->Reset();
  if (0 == value.count)
    return;

  if (!EnsureKey(kTimingsKeyName, &timing_key_))
    return;

  CString name(metric->name());
  TimingMetric::TimingData reg_value;
  if (!GetData(&timing_key_, name, &reg_value)) {
    memcpy(&reg_value, &value, sizeof(value));
  } else {
    reg_value.count += value.count;
    reg_value.sum += value.sum;
    reg_value.minimum = std::min(reg_value.minimum, value.minimum);
    reg_value.maximum = std::max(reg_value.maximum, value.maximum);
  }

  DWORD err = timing_key_.SetBinaryValue(name, &reg_value, sizeof(reg_value));
}

void MetricsAggregatorWin32::Aggregate(IntegerMetric *metric) {
  DCHECK(NULL != metric);

  // do as little as possible if no value
  uint64 value = metric->value();
  if (0 == value)
    return;

  if (!EnsureKey(kIntegersKeyName, &integer_key_))
    return;

  DWORD err = integer_key_.SetBinaryValue(CString(metric->name()),
                                          &value, sizeof(value));
}

void MetricsAggregatorWin32::Aggregate(BoolMetric *metric) {
  DCHECK(NULL != metric);

  // do as little as possible if no value
  int32 value = metric->Reset();
  if (BoolMetric::kBoolUnset == value)
    return;

  if (!EnsureKey(kBooleansKeyName, &bool_key_))
    return;

  DWORD err = bool_key_.SetBinaryValue(CString(metric->name()),
                                       &value, sizeof(value));
}

}  // namespace stats_report
