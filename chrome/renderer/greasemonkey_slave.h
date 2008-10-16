// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GREASEMONKEY_SLAVE_H__
#define CHROME_RENDERER_GREASEMONKEY_SLAVE_H__

#include <vector>

#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/string_piece.h"
#include "webkit/glue/webframe.h"

// Parsed representation of a Greasemonkey script.
class GreasemonkeyScript {
 public:
  // TODO(aa): Pass in filename script came from, for errors. Needs to be in
  // shared memory.
  GreasemonkeyScript() {}

  const StringPiece& GetBody() const {
    return body_;
  }

  bool Parse(const StringPiece& script_text) {
    // TODO(aa): Parse out includes, convert to regexes.
    body_ = script_text;
    return true;
  }

 private:
  // References the body of the script in shared memory. The underlying memory
  // is valid until shared_memory_ is either deleted or Unmap()'d.
  StringPiece body_;
};


// Manages installed GreasemonkeyScripts for a render process.
class GreasemonkeySlave {
 public:
  GreasemonkeySlave();

  // Update the parsed scripts from shared memory.
  bool UpdateScripts(SharedMemoryHandle shared_memory);

  // Inject the appropriate scripts into a frame based on its URL.
  // TODO(aa): Extract a GreasemonkeyFrame interface out of this to improve
  // testability.
  bool InjectScripts(WebFrame* frame);

 private:
  // Shared memory containing raw script data.
  scoped_ptr<SharedMemory> shared_memory_;

  // Parsed script data.
  std::vector<GreasemonkeyScript> scripts_;

  DISALLOW_COPY_AND_ASSIGN(GreasemonkeySlave);
};

#endif // CHROME_RENDERER_GREASEMONKEY_SLAVE_H__
