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


#include "formatter.h"

namespace stats_report {

Formatter::Formatter(const char *name, uint32 measurement_secs) {
  output_ << name << "&" << measurement_secs;
}

Formatter::~Formatter() {
}

void Formatter::AddCount(const char *name, uint64 value) {
  output_ << "&" << name << ":c=" << value;
}

void Formatter::AddTiming(const char *name, uint64 num, uint64 avg,
                          uint64 min, uint64 max) {
  output_ << "&" << name << ":t=" << num << ";"
          << avg << ";" << min << ";" << max;
}

void Formatter::AddInteger(const char *name, uint64 value) {
  output_ << "&" << name << ":i=" << value;
}

void Formatter::AddBoolean(const char *name, bool value) {
  output_ << "&" << name << ":b=" << (value ? "t" : "f");
}

void Formatter::AddMetric(MetricBase *metric) {
  switch (metric->type()) {
    case kCountType: {
      CountMetric *count = metric->AsCount();
      AddCount(count->name(), count->value());
      break;
    }
    case kTimingType: {
      TimingMetric *timing = metric->AsTiming();
      AddTiming(timing->name(), timing->count(), timing->average(),
                timing->minimum(), timing->maximum());
      break;
    }
    case kIntegerType: {
      IntegerMetric *integer = metric->AsInteger();
      AddInteger(integer->name(), integer->value());
      break;
    }
    case kBoolType: {
      BoolMetric *boolean = metric->AsBool();
      // TODO: boolean->value() returns a TristateBoolValue. The
      // formatter is going to serialize kBoolUnset to true.
      DCHECK_NE(boolean->value(), BoolMetric::kBoolUnset);
      AddBoolean(boolean->name(), boolean->value() != BoolMetric::kBoolFalse);
      break;
    }
    default:
      DCHECK(false && "Impossible metric type");
  }
}

}  // namespace stats_report
