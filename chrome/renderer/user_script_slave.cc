// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/user_script_slave.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "base/shared_memory.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/renderer/renderer_resources.h" 
#include "googleurl/src/gurl.h"

// These two strings are injected before and after the Greasemonkey API and
// user script to wrap it in an anonymous scope.
static const char kUserScriptHead[] = "(function (unsafeWindow) {";
static const char kUserScriptTail[] = "\n})(window);";

// UserScript

bool UserScript::MatchesUrl(const GURL& url) {
  for (std::vector<std::string>::iterator pattern = include_patterns_.begin();
       pattern != include_patterns_.end(); ++pattern) {
    if (MatchPattern(url.spec(), *pattern)) {
      return true;
    }
  }

  return false;
}

void UserScript::AddInclude(const std::string &glob_pattern) {
  include_patterns_.push_back(EscapeGlob(glob_pattern));
}

std::string UserScript::EscapeGlob(const std::string& input_pattern) {
  std::string output_pattern;

  for (size_t i = 0; i < input_pattern.length(); ++i) {
    switch (input_pattern[i]) {
      // These characters have special meaning to the MatchPattern() function,
      // so we escape them.
      case '\\':
      case '?':
        output_pattern += '\\';
        // fall through

      default:
        output_pattern += input_pattern[i];
    }
  }

  return output_pattern;
}


// UserScriptSlave
UserScriptSlave::UserScriptSlave()
    : shared_memory_(NULL),
      user_script_start_line_(0) {
  // TODO: Only windows supports resources and only windows supports user
  // scrips, so only load the Greasemonkey API on windows.  Fix this when
  // better cross platofrm support is available.
#if defined(OS_WIN)
  api_js_ = ResourceBundle::GetSharedInstance().GetRawDataResource(
                IDR_GREASEMONKEY_API_JS);
#endif

  // Count the number of lines that will be injected before the user script.
  StringPiece::size_type pos = 0;
  while ((pos = api_js_.find('\n', pos)) != StringPiece::npos) {
    user_script_start_line_++;
    pos++;
  }

  // Add one more line to account for the function that wraps everything.
  user_script_start_line_++;
}

bool UserScriptSlave::UpdateScripts(base::SharedMemoryHandle shared_memory) {
  scripts_.clear();

  // Create the shared memory object (read only).
  shared_memory_.reset(new base::SharedMemory(shared_memory, true));
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
  size_t num_scripts = 0;
  Pickle pickle(reinterpret_cast<char*>(shared_memory_->memory()),
                pickle_size);
  pickle.ReadSize(&iter, &num_scripts);

  for (size_t i = 0; i < num_scripts; ++i) {
    const char* url = NULL;
    int url_length = 0;
    const char* body = NULL;
    int body_length = 0;

    pickle.ReadData(&iter, &url, &url_length);
    pickle.ReadData(&iter, &body, &body_length);

    scripts_.push_back(UserScript(StringPiece(url, url_length),
                                  StringPiece(body, body_length)));
    UserScript& script = scripts_.back();

    size_t num_includes;
    pickle.ReadSize(&iter, &num_includes);
    for (size_t j = 0; j < num_includes; ++j) {
      std::string include;
      pickle.ReadString(&iter, &include);
      script.AddInclude(include);
    }
  }

  return true;
}

bool UserScriptSlave::InjectScripts(WebFrame* frame) {
  for (std::vector<UserScript>::iterator script = scripts_.begin();
       script != scripts_.end(); ++script) {
    if (script->MatchesUrl(frame->GetURL())) {
      std::string inject(kUserScriptHead);
      inject.append(api_js_.as_string());
      inject.append(script->GetBody().as_string());
      inject.append(kUserScriptTail);
      frame->ExecuteJavaScript(inject,
                               GURL(script->GetURL().as_string()),
                               -user_script_start_line_);
    }
  }

  return true;
}
