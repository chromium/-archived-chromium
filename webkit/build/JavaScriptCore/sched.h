// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BUILD_JAVASCRIPTCORE_SCHED_H__
#define WEBKIT_BUILD_JAVASCRIPTCORE_SCHED_H__

// Dummy implementation of sched.h so WTF will compile.

inline int sched_yield() {
  return 0;
}

#endif  // WEBKIT_BUILD_JAVASCRIPTCORE_SCHED_H__
