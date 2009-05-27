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


#include "statsreport/common/highres_timer.h"
#include "gtest/gtest.h"
#include "base/basictypes.h"

// These unittests have proven to be flaky on the build server. While
// we don't want them breaking the build, we still build them to guard
// against bitrot. On dev's machines during local builds we leave them
// enabled.
#ifndef BUILD_SERVER_BUILD
TEST(HighresTimer, MillisecondClock) {
#else
TEST(HighresTimer, DISABLED_MillisecondClock) {
#endif
  HighresTimer timer;

  // note: this could fail if we context switch between initializing the timer
  // and here. Very unlikely however.
  EXPECT_EQ(0, timer.GetElapsedMs());
  timer.Start();
  uint64 half_ms = HighresTimer::GetTimerFrequency() / 2000;
  // busy wait for half a millisecond
  while (timer.start_ticks() + half_ms > HighresTimer::GetCurrentTicks()) {
    // Nothing
  }
  EXPECT_EQ(1, timer.GetElapsedMs());
}

#ifndef BUILD_SERVER_BUILD
TEST(HighresTimer, SecondClock) {
#else
TEST(HighresTimer, DISABLED_SecondClock) {
#endif
  HighresTimer timer;

  EXPECT_EQ(0, timer.GetElapsedSec());
#ifdef OS_WIN
  ::Sleep(250);
#else
  struct timespec ts1 = {0, 250000000};
  nanosleep(&ts1, 0);
#endif
  EXPECT_EQ(0, timer.GetElapsedSec());
  EXPECT_LE(230, timer.GetElapsedMs());
  EXPECT_GE(270, timer.GetElapsedMs());
#ifdef OS_WIN
  ::Sleep(251);
#else
  struct timespec ts2 = {0, 251000000};
  nanosleep(&ts2, 0);
#endif
  EXPECT_EQ(1, timer.GetElapsedSec());
}
