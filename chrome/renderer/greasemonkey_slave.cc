// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/greasemonkey_slave.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "base/shared_memory.h"

GreasemonkeySlave::GreasemonkeySlave() : shared_memory_(NULL) {
}

bool GreasemonkeySlave::UpdateScripts(SharedMemoryHandle shared_memory) {
  scripts_.clear();

  // Create the shared memory object.
  shared_memory_.reset(new SharedMemory(shared_memory, true)); // read-only
  if (!shared_memory_.get())
    return false;

  // First get the size of the memory block.
  if (!shared_memory_->Map(sizeof(Pickle::Header)))
    return false;
  Pickle::Header* pickle_header =
      reinterpret_cast<Pickle::Header*>(shared_memory_->memory());

  // Now map in the rest of the block.
  int pickle_size = sizeof(Pickle::Header) + pickle_header->payload_size;
  shared_memory_->Unmap();
  if (!shared_memory_->Map(pickle_size))
    return false;

  // Unpickle scripts.
  void* iter = NULL;
  int num_scripts = 0;
  Pickle pickle(reinterpret_cast<char*>(shared_memory_->memory()),
                pickle_size);
  pickle.ReadInt(&iter, &num_scripts);

  for (int i = 0; i < num_scripts; ++i) {
    const char* data = NULL;
    int data_length = 0;
    pickle.ReadData(&iter, &data, &data_length);

    GreasemonkeyScript script;
    if (script.Parse(StringPiece(data, data_length))) {
      scripts_.push_back(script);
    }
  }

  return true;
}

bool GreasemonkeySlave::InjectScripts(WebFrame* frame) {
  // TODO(aa): Check script patterns here

  for (std::vector<GreasemonkeyScript>::iterator script = scripts_.begin();
       script != scripts_.end(); ++script) {
    // TODO(aa): Pass in URL to script when we have it.
    frame->ExecuteJavaScript(script->GetBody().as_string(),
                             "Greasemonkey Script");
  }

  return true;
}
