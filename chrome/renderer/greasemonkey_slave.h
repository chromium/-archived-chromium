// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GREASEMONKEY_SLAVE_H__
#define CHROME_RENDERER_GREASEMONKEY_SLAVE_H__

#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/string_piece.h"
#include "webkit/glue/webframe.h"

// Parsed representation of a Greasemonkey script.
class GreasemonkeyScript {
 public:
  GreasemonkeyScript(const StringPiece& script_url)
    : url_(script_url) {}

  const StringPiece& GetBody() const {
    return body_;
  }

  const StringPiece& GetURL() const {
    return url_;
  }

  bool Parse(const StringPiece& script_text) {
    // TODO(aa): Parse out includes, convert to regexes.
    body_ = script_text;
    return true;
  }

 private:
  // The body of the script, which will be injected into content pages. This
  // references shared_memory_, and is valid until that memory is either
  // deleted or Unmap()'d.
  StringPiece body_;

  // The url of the file the script came from. This references shared_memory_,
  // and is valid until that memory is either deleted or Unmap()'d.
  StringPiece url_;
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
