// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Counters used within the browser.

#ifndef IPC_COMMON_IPC_COUNTERS_H_
#define IPC_COMMON_IPC_COUNTERS_H_

class StatsCounter;

namespace IPC {

class Counters {
 public:
  // The number of messages sent on IPC channels.
  static StatsCounter& ipc_send_counter();
};

}  // namespace IPC

#endif  // IPC_COMMON_IPC_COUNTERS_H_
