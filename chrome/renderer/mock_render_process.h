// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MOCK_RENDER_PROCESS_H_
#define CHROME_RENDERER_MOCK_RENDER_PROCESS_H_

#include "chrome/common/child_process.h"

class ChildThread;

// This class is a trivial mock of the child process singleton. It is necessary
// so we don't trip DCHECKs in ChildProcess::ReleaseProcess() when destroying
// a render widget instance.
class MockProcess : public ChildProcess {
 public:
  explicit MockProcess() : ChildProcess(NULL) {}
  explicit MockProcess(ChildThread* thread) : ChildProcess(thread) {}
};

#endif  // CHROME_RENDERER_MOCK_RENDER_PROCESS_H_
