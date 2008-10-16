// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/greasemonkey_master.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"

bool GreasemonkeyMaster::UpdateScripts() {
  std::vector<std::string> scripts;
  std::wstring path;

  PathService::Get(chrome::DIR_USER_SCRIPTS, &path);
  file_util::FileEnumerator enumerator(path, false,
                                       file_util::FileEnumerator::FILES,
                                       L"*.user.js");
  for (std::wstring file = enumerator.Next(); !file.empty();
       file = enumerator.Next()) {
    // TODO(aa): Support unicode script files.
    std::string contents;
    file_util::ReadFileToString(file, &contents);
    scripts.push_back(contents);
  }

  // Pickle scripts data.
  Pickle pickle;
  pickle.WriteSize(scripts.size());
  for (std::vector<std::string>::iterator script = scripts.begin();
       script != scripts.end(); ++script) {
    // Write script body as 'data' so that we can read it out in the slave
    // without allocating a new string.
    pickle.WriteData(script->c_str(), script->size());
  }

  // Create the shared memory object.
  scoped_ptr<SharedMemory> temp_shared_memory(new SharedMemory());
  if (!temp_shared_memory.get()) {
    return false;
  }

  shared_memory_serial_++;
  if (!temp_shared_memory->Create(std::wstring(), // anonymous
                                  false, // read-only
                                  false, // open existing
                                  pickle.size())) {
    return false;
  }

  // Map into our process.
  if (!temp_shared_memory->Map(pickle.size())) {
    return false;
  }

  // Copy the pickle to shared memory.
  memcpy(temp_shared_memory->memory(), pickle.data(), pickle.size());

  shared_memory_.reset(temp_shared_memory.release());
  return true;
}

bool GreasemonkeyMaster::ShareToProcess(ProcessHandle process,
                                        SharedMemoryHandle* new_handle) {
  if (shared_memory_.get())
    return shared_memory_->ShareToProcess(process, new_handle);

  NOTREACHED();
  return false;
}
