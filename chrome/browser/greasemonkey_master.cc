// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/greasemonkey_master.h"

#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

bool GreasemonkeyMaster::UpdateScripts() {
  std::vector<std::wstring> scripts;
  std::wstring path;

  PathService::Get(chrome::DIR_USER_SCRIPTS, &path);
  file_util::FileEnumerator enumerator(path, false,
                                       file_util::FileEnumerator::FILES,
                                       L"*.user.js");
  for (std::wstring file = enumerator.Next(); !file.empty();
       file = enumerator.Next()) {
    scripts.push_back(file);
  }

  // Pickle scripts data.
  Pickle pickle;
  pickle.WriteSize(scripts.size());
  for (std::vector<std::wstring>::iterator path = scripts.begin();
       path != scripts.end(); ++path) {
    std::string file_url = net::FilePathToFileURL(*path).spec();
    std::string contents;
    // TODO(aa): Support unicode script files.
    file_util::ReadFileToString(*path, &contents);

    // Write scripts as 'data' so that we can read it out in the slave without
    // allocating a new string.
    pickle.WriteData(file_url.c_str(), file_url.length());
    pickle.WriteData(contents.c_str(), contents.length());
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
