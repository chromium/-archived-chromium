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


// Iterator over persisted metrics
#ifndef O3D_STATSREPORT_PERSISTENT_ITERATOR_WIN32_H__
#define O3D_STATSREPORT_PERSISTENT_ITERATOR_WIN32_H__

#include <atlbase.h>
#include <atlstr.h>
#include <iterator>
#include "base/scoped_ptr.h"
#include "metrics.h"
#include "const-win32.h"

namespace stats_report {

// Forward iterator for persisted metrics
class PersistentMetricsIteratorWin32
    : public std::iterator<std::forward_iterator_tag, const MetricBase *> {
 public:
  // @param app_name see MetricsAggregatorWin32
  explicit PersistentMetricsIteratorWin32(const wchar_t *app_name)
      : state_(kUninitialized),
        is_machine_(false) {
    key_name_.Format(kStatsKeyFormatString, app_name);
    Next();
  }

  // @param app_name see MetricsAggregatorWin32
  // @param is_machine specifies the registry hive
  PersistentMetricsIteratorWin32(const wchar_t *app_name, bool is_machine)
      : state_(kUninitialized),
        is_machine_(is_machine) {
    key_name_.Format(kStatsKeyFormatString, app_name);
    Next();
  }

  // Constructs the at-end iterator
  PersistentMetricsIteratorWin32() : state_(kUninitialized) {
  }

  MetricBase *operator* () {
    return Current();
  }
  MetricBase *operator-> () {
    return Current();
  }

  // Preincrement, we don't implement postincrement because we don't
  // want to deal with making iterators copyable, comparable etc.
  PersistentMetricsIteratorWin32 &operator++() {
    Next();

    return (*this);
  }

  // Compare for equality with o.
  bool equals(const PersistentMetricsIteratorWin32 &o) const {
    // compare equal to self, and end iterators compare equal
    if ((this == &o) || (NULL == current_value_.get() &&
                         NULL == o.current_value_.get()))
      return true;

    return false;
  }

 private:
  MetricBase *Current() {
    DCHECK(current_value_.get());
    return current_value_.get();
  }

  enum IterationState {
    kUninitialized,
    kCounts,
    kTimings,
    kIntegers,
    kBooleans,
    kFinished,
  };

  // Walk to the next key/value under iteration
  void Next();

  // Keeps track of which subkey we're iterating over
  IterationState state_;

  // The full path from HKCU to the key we iterate over
  CString key_name_;

  // The top-level key we're iterating over, valid only
  // after first call to Next().
  CRegKey key_;

  // The subkey we're currently enumerating over
  CRegKey sub_key_;

  // Current value we're indexing over
  DWORD value_index_;

  // Name of the value under the iterator
  CStringA current_value_name_;

  // The metric under the iterator
  scoped_ptr<MetricBase> current_value_;

  // Specifies HKLM or HKCU, respectively.
  bool is_machine_;
};

inline bool operator == (const PersistentMetricsIteratorWin32 &a,
                         const PersistentMetricsIteratorWin32 &b) {
  return a.equals(b);
}

inline bool operator != (const PersistentMetricsIteratorWin32 &a,
                         const PersistentMetricsIteratorWin32 &b) {
  return !a.equals(b);
}

}  // namespace stats_report

#endif  // O3D_STATSREPORT_PERSISTENT_ITERATOR_WIN32_H__
