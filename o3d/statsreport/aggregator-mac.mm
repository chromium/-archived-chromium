
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


// Implementation of a Mac metrics aggregator.

#include "aggregator-mac.h"
#include "const-mac.h"
#include "formatter.h"


namespace stats_report {


MetricsAggregatorMac::MetricsAggregatorMac(const MetricCollection &coll)
  : MetricsAggregator(coll),
    dict_(nil),
    pool_(nil){
  storePath_ = O3DStatsPath();
}

MetricsAggregatorMac::~MetricsAggregatorMac() {
}


bool MetricsAggregatorMac::StartAggregation() {
  pool_ = [[NSAutoreleasePool alloc] init];
  dict_ = [NSMutableDictionary dictionaryWithContentsOfFile:storePath_];
  if (dict_ == nil)
    dict_ = [[[NSMutableDictionary alloc] init] autorelease];

  return true;
}

void MetricsAggregatorMac::EndAggregation() {
  [dict_ writeToFile:storePath_ atomically:YES];
  dict_ = nil; // it will get autoreleased when the pool dies

  [pool_ release];
  pool_ = nil;
}

void MetricsAggregatorMac::Aggregate(CountMetric *metric) {
  // do as little as possible if no value
  uint64 value = metric->Reset();
  if (0 == value)
    return;

  NSString *keyName = [NSString stringWithFormat:@"C_%s", metric->name()];
  NSNumber *previousValue = [dict_ objectForKey:keyName];
  if (previousValue == nil)
    previousValue = [NSNumber numberWithLongLong:0];

  NSNumber *newTotal =
      [NSNumber numberWithLongLong:value + [previousValue longLongValue]];
  [dict_ setObject:newTotal forKey:keyName];
}


static long long LLMin(long long a, long long b) {
  return a < b ? a : b;
}

static long long LLMax(long long a, long long b) {
  return a > b ? a : b;
}

void MetricsAggregatorMac::Aggregate(TimingMetric *metric) {
  // do as little as possible if no value
  TimingMetric::TimingData newValue = metric->Reset();
  if (0 == newValue.count)
    return;

  NSString *keyName = [NSString stringWithFormat:@"T_%s", metric->name()];
  NSArray *previousValue = [dict_ objectForKey:keyName];
  NSArray *newTotal = nil;

  if (previousValue == nil) {
    newTotal = [NSArray arrayWithObjects:
                [NSNumber numberWithInt:newValue.count],
                [NSNumber numberWithLongLong:newValue.sum],
                [NSNumber numberWithLongLong:newValue.minimum],
                [NSNumber numberWithLongLong:newValue.maximum],
                nil];
  } else {
    int previousCount = [[previousValue objectAtIndex:0] intValue];
    long long previousSum = [[previousValue objectAtIndex:1] longLongValue];
    long long previousMin = [[previousValue objectAtIndex:2] longLongValue];
    long long previousMax = [[previousValue objectAtIndex:3] longLongValue];

    newTotal =
        [NSArray arrayWithObjects:
         [NSNumber numberWithInt:newValue.count + previousCount],
         [NSNumber numberWithLongLong:newValue.sum + previousSum],
         [NSNumber numberWithLongLong:LLMin(newValue.minimum, previousMin)],
         [NSNumber numberWithLongLong:LLMax(newValue.maximum, previousMax)],
         nil];
  }

  [dict_ setObject:newTotal forKey:keyName];
}

void MetricsAggregatorMac::Aggregate(IntegerMetric *metric) {
  // do as little as possible if no value
  int64 value = metric->value();  // yes, we only have 63 positive bits here :(
  if (0 == value)
    return;

  NSString *keyName = [NSString stringWithFormat:@"I_%s", metric->name()];
  [dict_ setObject:[NSNumber numberWithLongLong:value] forKey:keyName];
}

void MetricsAggregatorMac::Aggregate(BoolMetric *metric) {
  // do as little as possible if no value
  int32 value = metric->Reset();
  if (BoolMetric::kBoolUnset == value)
    return;

  NSString *keyName = [NSString stringWithFormat:@"B_%s", metric->name()];
  [dict_ setObject:[NSNumber numberWithBool:(value != 0)] forKey:keyName];
}

}  // namespace stats_report
