// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_UTILITY_THREAD_H_
#define CHROME_UTILITY_UTILITY_THREAD_H_

#include <string>

#include "base/thread.h"
#include "chrome/common/child_thread.h"

class GURL;

// This class represents the background thread where the utility task runs.
class UtilityThread : public ChildThread {
 public:
  UtilityThread();
  ~UtilityThread();

  // Returns the one utility thread.
  static UtilityThread* current() {
    return static_cast<UtilityThread*>(ChildThread::current());
  }

 private:
  // IPC messages
  virtual void OnControlMessageReceived(const IPC::Message& msg);
  void OnUnpackExtension(const FilePath& extension_path);

  // IPC messages for web resource service.
  void OnUnpackWebResource(const std::string& resource_data);

  // Called by the thread base class
  virtual void Init();
  virtual void CleanUp();

  DISALLOW_COPY_AND_ASSIGN(UtilityThread);
};

#endif  // CHROME_UTILITY_UTILITY_THREAD_H_
