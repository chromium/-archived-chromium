// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/user_script_slave.h"

#include "app/resource_bundle.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/perftimer.h"
#include "base/pickle.h"
#include "base/shared_memory.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "webkit/api/public/WebScriptSource.h"
#include "webkit/glue/webframe.h"

#include "grit/renderer_resources.h"

using WebKit::WebScriptSource;
using WebKit::WebString;

// These two strings are injected before and after the Greasemonkey API and
// user script to wrap it in an anonymous scope.
static const char kUserScriptHead[] = "(function (unsafeWindow) {\n";
static const char kUserScriptTail[] = "\n})(window);";

// Creates a convenient reference to a content script's parent extension.
// TODO(mpcomplete): self.onConnect is deprecated.  Remove it at 1.0.
// http://code.google.com/p/chromium/issues/detail?id=16356
static const char kInitExtension[] =
    "chrome.extension = new chrome.Extension('%s');"
    "chrome.self.onConnect = chrome.extension.onConnect;";

UserScriptSlave::UserScriptSlave()
    : shared_memory_(NULL),
      script_deleter_(&scripts_),
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

  // NOTE: There is actually one extra line in the injected script because the
  // function header includes a newline as well. But WebKit expects the
  // numbering to be one-based, not zero-based, so actually *not* accounting for
  // this extra line ends us up with the right offset.
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

  scripts_.reserve(num_scripts);
  for (size_t i = 0; i < num_scripts; ++i) {
    scripts_.push_back(new UserScript());
    UserScript* script = scripts_.back();
    script->Unpickle(pickle, &iter);

    // Note that this is a pointer into shared memory. We don't own it. It gets
    // cleared up when the last renderer or browser process drops their
    // reference to the shared memory.
    for (size_t j = 0; j < script->js_scripts().size(); ++j) {
      const char* body = NULL;
      int body_length = 0;
      CHECK(pickle.ReadData(&iter, &body, &body_length));
      script->js_scripts()[j].set_external_content(
          StringPiece(body, body_length));
    }
    for (size_t j = 0; j < script->css_scripts().size(); ++j) {
      const char* body = NULL;
      int body_length = 0;
      CHECK(pickle.ReadData(&iter, &body, &body_length));
      script->css_scripts()[j].set_external_content(
          StringPiece(body, body_length));
    }
  }

  return true;
}

bool UserScriptSlave::InjectScripts(WebFrame* frame,
                                    UserScript::RunLocation location) {
  PerfTimer timer;
  int num_matched = 0;

  for (size_t i = 0; i < scripts_.size(); ++i) {
    std::vector<WebScriptSource> sources;
    UserScript* script = scripts_[i];
    if (!script->MatchesUrl(frame->GetURL()))
      continue;  // This frame doesn't match the script url pattern, skip it.

    ++num_matched;
    // CSS files are always injected on document start before js scripts.
    if (location == UserScript::DOCUMENT_START) {
      for (size_t j = 0; j < script->css_scripts().size(); ++j) {
        UserScript::File& file = script->css_scripts()[j];
        frame->InsertCSSStyles(file.GetContent().as_string());
      }
    }
    if (script->run_location() == location) {
      for (size_t j = 0; j < script->js_scripts().size(); ++j) {
        UserScript::File &file = script->js_scripts()[j];
        std::string content = file.GetContent().as_string();

        // We add this dumb function wrapper for standalone user script to
        // emulate what Greasemonkey does.
        if (script->is_standalone()) {
          content.insert(0, kUserScriptHead);
          content += kUserScriptTail;
        }
        sources.push_back(WebScriptSource(
            WebString::fromUTF8(content.c_str(), content.length()),
            file.url()));
      }
    }

    if (!sources.empty()) {
      if (script->is_standalone()) {
        // For standalone scripts, we try to emulate the Greasemonkey API.
        sources.insert(sources.begin(),
            WebScriptSource(WebString::fromUTF8(api_js_.as_string())));
      } else {
        // Setup chrome.self to contain an Extension object with the correct
        // ID.
        sources.insert(sources.begin(),
            WebScriptSource(WebString::fromUTF8(
                StringPrintf(kInitExtension, script->extension_id().c_str()))));
      }

      frame->ExecuteScriptInNewContext(&sources.front(), sources.size());
    }
  }

  // Log debug info.
  if (location == UserScript::DOCUMENT_START) {
    HISTOGRAM_COUNTS_100("UserScripts:DocStart:Count", num_matched);
    HISTOGRAM_TIMES("UserScripts:DocStart:Time", timer.Elapsed());
  } else {
    HISTOGRAM_COUNTS_100("UserScripts:DocEnd:Count", num_matched);
    HISTOGRAM_TIMES("UserScripts:DocEnd:Time", timer.Elapsed());
  }

  LOG(INFO) << "Injected " << num_matched << " user scripts into " <<
      frame->GetURL().spec();
  return true;
}
