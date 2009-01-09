// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_master.h"

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

// We reload user scripts on the file thread to prevent blocking the UI.
// ScriptReloader lives on the file thread and does the reload
// work, and then sends a message back to its master with a new SharedMemory*.

// ScriptReloader is the worker that manages running the script scan
// on the file thread.
// It must be created on, and its public API must only be called from,
// the master's thread.
class UserScriptMaster::ScriptReloader
    : public base::RefCounted<UserScriptMaster::ScriptReloader> {
 public:
  ScriptReloader(UserScriptMaster* master)
       : master_(master), master_message_loop_(MessageLoop::current()) {}

  // Start a scan for scripts.
  // Will always send a message to the master upon completion.
  void StartScan(MessageLoop* work_loop, const FilePath& script_dir);

  // The master is going away; don't call it back.
  void DisownMaster() {
    master_ = NULL;
  }

 private:
  // Where functions are run:
  //    master          file
  //   StartScan   ->  RunScan
  //                     GetNewScripts()
  // NotifyMaster  <-  RunScan

  // Runs on the master thread.
  // Notify the master that new scripts are available.
  void NotifyMaster(base::SharedMemory* memory);

  // Runs on the File thread.
  // Scan the script directory for scripts, calling NotifyMaster when done.
  // The path is intentionally passed by value so its lifetime isn't tied
  // to the caller.
  void RunScan(const FilePath script_dir);

  // Runs on the File thread.
  // Scan the script directory for scripts, returning either a new SharedMemory
  // or NULL on error.
  base::SharedMemory* GetNewScripts(const FilePath& script_dir);

  // A pointer back to our master.
  // May be NULL if DisownMaster() is called.
  UserScriptMaster* master_;

  // The message loop to call our master back on.
  // Expected to always outlive us.
  MessageLoop* master_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ScriptReloader);
};

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
