// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_master.h"

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_protocols.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

// static
void UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      const StringPiece& script_text, std::vector<std::string> *includes) {
  // http://wiki.greasespot.net/Metadata_block
  StringPiece line;
  size_t line_start = 0;
  size_t line_end = 0;
  bool in_metadata = false;

  static const StringPiece kUserScriptBegin("// ==UserScript==");
  static const StringPiece kUserScriptEng("// ==/UserScript==");
  static const StringPiece kIncludeDeclaration("// @include ");

  while (line_start < script_text.length()) {
    line_end = script_text.find('\n', line_start);

    // Handle the case where there is no trailing newline in the file.
    if (line_end == std::string::npos) {
      line_end = script_text.length() - 1;
    }

    line.set(script_text.data() + line_start, line_end - line_start);

    if (!in_metadata) {
      if (line.starts_with(kUserScriptBegin)) {
        in_metadata = true;
      }
    } else {
      if (line.starts_with(kUserScriptEng)) {
        break;
      }

      if (line.starts_with(kIncludeDeclaration)) {
        std::string pattern(line.data() + kIncludeDeclaration.length(),
                            line.length() - kIncludeDeclaration.length());
        std::string pattern_trimmed;
        TrimWhitespace(pattern, TRIM_ALL, &pattern_trimmed);
        includes->push_back(pattern_trimmed);
      }

      // TODO(aa): Handle more types of metadata.
    }

    line_start = line_end + 1;
  }

  // If no @include patterns were specified, default to @include *.
  // This is what Greasemonkey does.
  if (includes->size() == 0) {
    includes->push_back("*");
  }
}

void UserScriptMaster::ScriptReloader::StartScan(
    MessageLoop* work_loop,
    const FilePath& script_dir) {
  // Add a reference to ourselves to keep ourselves alive while we're running.
  // Balanced by NotifyMaster().
  AddRef();
  work_loop->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &UserScriptMaster::ScriptReloader::RunScan,
                        script_dir));
}

void UserScriptMaster::ScriptReloader::NotifyMaster(
    base::SharedMemory* memory) {
  if (!master_) {
    // The master went away, so these new scripts aren't useful anymore.
    delete memory;
  } else {
    master_->NewScriptsAvailable(memory);
  }

  // Drop our self-reference.
  // Balances StartScan().
  Release();
}

void UserScriptMaster::ScriptReloader::RunScan(const FilePath script_dir) {
  base::SharedMemory* shared_memory = GetNewScripts(script_dir);

  // Post the new scripts back to the master's message loop.
  master_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &UserScriptMaster::ScriptReloader::NotifyMaster,
                        shared_memory));
}

base::SharedMemory* UserScriptMaster::ScriptReloader::GetNewScripts(
    const FilePath& script_dir) {
  std::vector<std::wstring> scripts;

  file_util::FileEnumerator enumerator(script_dir, false,
                                       file_util::FileEnumerator::FILES,
                                       FILE_PATH_LITERAL("*.user.js"));
  for (FilePath file = enumerator.Next(); !file.value().empty();
       file = enumerator.Next()) {
    scripts.push_back(file.ToWStringHack());
  }

  if (scripts.empty())
    return NULL;

  // Pickle scripts data.
  Pickle pickle;
  pickle.WriteSize(scripts.size());
  for (std::vector<std::wstring>::iterator path = scripts.begin();
       path != scripts.end(); ++path) {
    std::string url(kUserScriptURLScheme);
    url += ":/";
    url += net::FilePathToFileURL(*path).ExtractFileName();

    std::string contents;
    // TODO(aa): Support unicode script files.
    file_util::ReadFileToString(*path, &contents);

    std::vector<std::string> includes;
    ParseMetadataHeader(contents, &includes);

    // Write scripts as 'data' so that we can read it out in the slave without
    // allocating a new string.
    pickle.WriteData(url.c_str(), url.length());
    pickle.WriteData(contents.c_str(), contents.length());
    pickle.WriteSize(includes.size());
    for (std::vector<std::string>::iterator iter = includes.begin();
         iter != includes.end(); ++iter) {
      pickle.WriteString(*iter);
    }
  }

  // Create the shared memory object.
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());

  if (!shared_memory->Create(std::wstring(),  // anonymous
                             false,  // read-only
                             false,  // open existing
                             pickle.size())) {
    return NULL;
  }

  // Map into our process.
  if (!shared_memory->Map(pickle.size()))
    return NULL;

  // Copy the pickle to shared memory.
  memcpy(shared_memory->memory(), pickle.data(), pickle.size());

  return shared_memory.release();
}

UserScriptMaster::UserScriptMaster(MessageLoop* worker_loop,
                                       const FilePath& script_dir)
    : user_script_dir_(new FilePath(script_dir)),
      dir_watcher_(new DirectoryWatcher),
      worker_loop_(worker_loop),
      pending_scan_(false) {
  // Watch our scripts directory for modifications.
  if (dir_watcher_->Watch(script_dir, this)) {
    // (Asynchronously) scan for our initial set of scripts.
    StartScan();
  }
}

UserScriptMaster::~UserScriptMaster() {
  if (script_reloader_)
    script_reloader_->DisownMaster();
}

void UserScriptMaster::NewScriptsAvailable(base::SharedMemory* handle) {
  // Ensure handle is deleted or released.
  scoped_ptr<base::SharedMemory> handle_deleter(handle);

  if (pending_scan_) {
    // While we were scanning, there were further changes.  Don't bother
    // notifying about these scripts and instead just immediately rescan.
    pending_scan_ = false;
    StartScan();
  } else {
    // We're no longer scanning.
    script_reloader_ = NULL;
    // We've got scripts ready to go.
    shared_memory_.swap(handle_deleter);

    NotificationService::current()->Notify(NOTIFY_USER_SCRIPTS_LOADED,
        NotificationService::AllSources(),
        Details<base::SharedMemory>(handle));
  }
}

void UserScriptMaster::OnDirectoryChanged(const FilePath& path) {
  if (script_reloader_.get()) {
    // We're already scanning for scripts.  We note that we should rescan when
    // we get the chance.
    pending_scan_ = true;
    return;
  }

  StartScan();
}

void UserScriptMaster::StartScan() {
  if (!script_reloader_)
    script_reloader_ = new ScriptReloader(this);

  script_reloader_->StartScan(worker_loop_, *user_script_dir_);
}
