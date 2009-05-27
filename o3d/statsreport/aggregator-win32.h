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


// Win32 aggregator, which aggregates counters to registry under a named
// Mutex lock.
#ifndef O3D_STATSREPORT_AGGREGATOR_WIN32_H__
#define O3D_STATSREPORT_AGGREGATOR_WIN32_H__

#include "aggregator.h"
#include <atlbase.h>
#include <atlstr.h>

namespace stats_report {

class MetricsAggregatorWin32: public MetricsAggregator {
 public:
  // @param coll the metrics collection to aggregate, most usually this
  //           is g_global_metrics.
  // @param app_name name of the subkey under HKCU\Software\Google we
  //           aggregate to. Should be or encode the application name for
  //           transparency, e.g. "Scour", or "Gears".
  MetricsAggregatorWin32(const MetricCollection &coll,
                         const wchar_t *app_name);

  // @param is_machine specifies the registry hive where the stats are
  //           aggregated to.
  MetricsAggregatorWin32(const MetricCollection &coll,
                         const wchar_t *app_name,
                         bool is_machine);
  virtual ~MetricsAggregatorWin32();

 protected:
  virtual bool StartAggregation();
  virtual void EndAggregation();

  virtual void Aggregate(CountMetric *metric);
  virtual void Aggregate(TimingMetric *metric);
  virtual void Aggregate(IntegerMetric *metric);
  virtual void Aggregate(BoolMetric *metric);
 private:
  enum {
    // Max length of time we wait for the mutex on StartAggregation.
    kMaxMutexWaitMs = 1000,  // 1 second for now
  };

  // Ensures that *key is open, opening it if it's NULL
  // @return true on success, false on failure to open key
  bool EnsureKey(const wchar_t *name, CRegKey *key);

  // Mutex name for locking access to key
  CString mutex_name_;

  // Subkey name, as per constructor docs
  CString key_name_;

  // Handle to our subkey under HKCU\Software\Google
  CRegKey key_;

  // Subkeys under the above
  // @{
  CRegKey count_key_;
  CRegKey timing_key_;
  CRegKey integer_key_;
  CRegKey bool_key_;
  // @}

  // Specifies HKLM or HKCU, respectively.
  bool is_machine_;

  DISALLOW_COPY_AND_ASSIGN(MetricsAggregatorWin32);
};

}  // namespace stats_report

#endif  // O3D_STATSREPORT_AGGREGATOR_WIN32_H__
