// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_SLAVE_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_SLAVE_H_

#include <map>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/string_piece.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/stl_util-inl.h"

class WebFrame;

// Manages installed UserScripts for a render process.
class UserScriptSlave {
 public:
  UserScriptSlave();

  // Update the parsed scripts from shared memory.
  bool UpdateScripts(base::SharedMemoryHandle shared_memory);

  // Inject the appropriate scripts into a frame based on its URL.
  // TODO(aa): Extract a UserScriptFrame interface out of this to improve
  // testability.
  bool InjectScripts(WebFrame* frame);

 private:
  // Shared memory containing raw script data.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // Parsed script data.
  std::vector<UserScript*> scripts_;
  STLElementDeleter<std::vector<UserScript*> > script_deleter_;

  // Script contents.
  std::map<UserScript*, StringPiece> script_contents_;

  // Greasemonkey API source that is injected with the scripts.
  StringPiece api_js_;

  // The line number of the first line of the user script among all of the
  // injected javascript.  This is used to make reported errors correspond with
  // the proper line in the user script.
  int user_script_start_line_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptSlave);
};

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_SLAVE_H_
