// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GREASEMONKEY_MASTER_H__
#define CHROME_BROWSER_GREASEMONKEY_MASTER_H__

#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"

// Manages a segment of shared memory that contains the Greasemonkey scripts the
// user has installed.
class GreasemonkeyMaster {
 public:
  GreasemonkeyMaster()
      : shared_memory_serial_(0) {}

  // Reloads scripts from disk into a new chunk of shared memory and notifies
  // renderers.
  bool UpdateScripts();

  // Creates a handle to the shared memory that can be used in the specified
  // process.
  bool ShareToProcess(ProcessHandle process, SharedMemoryHandle* new_handle);

  // Gets the segment of shared memory for the scripts.
  SharedMemory* GetSharedMemory() const {
    return shared_memory_.get();
  }

 private:
  // Contains the scripts that were found the last time UpdateScripts() was
  // called.
  scoped_ptr<SharedMemory> shared_memory_;

  // A counter that is incremented each time a new shared memory segment is
  // created. This is used to uniquely identify segments created at different
  // times by this class.
  int shared_memory_serial_;

  DISALLOW_COPY_AND_ASSIGN(GreasemonkeyMaster);
};

#endif // CHROME_BROWSER_GREASEMONKEY_MASTER_H__
