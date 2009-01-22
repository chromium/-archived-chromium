// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_

#include "base/directory_watcher.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/string_piece.h"

// Manages a segment of shared memory that contains the user scripts the user
// has installed.  Lives on the UI thread.
class UserScriptMaster : public base::RefCounted<UserScriptMaster>,
                         public DirectoryWatcher::Delegate {
 public:
  // For testability, the constructor takes the MessageLoop to run the
  // script-reloading worker on as well as the path the scripts live in.
  // These are normally the file thread and a directory inside the profile.
  UserScriptMaster(MessageLoop* worker, const FilePath& script_dir);
  ~UserScriptMaster();

  // Gets the segment of shared memory for the scripts.
  base::SharedMemory* GetSharedMemory() const {
    return shared_memory_.get();
  }

  // Called by the script reloader when new scripts have been loaded.
  void NewScriptsAvailable(base::SharedMemory* handle);

  // Return true if we have any scripts ready.
  bool ScriptsReady() const { return shared_memory_.get() != NULL; }

  // Returns the path to the directory user scripts are stored in.
  FilePath user_script_dir() const { return *user_script_dir_; }

 private:
  FRIEND_TEST(UserScriptMasterTest, Parse1);
  FRIEND_TEST(UserScriptMasterTest, Parse2);
  FRIEND_TEST(UserScriptMasterTest, Parse3);

  // We reload user scripts on the file thread to prevent blocking the UI.
  // ScriptReloader lives on the file thread and does the reload
  // work, and then sends a message back to its master with a new SharedMemory*.
  // ScriptReloader is the worker that manages running the script scan
  // on the file thread. It must be created on, and its public API must only be
  // called from, the master's thread.
  class ScriptReloader
      : public base::RefCounted<UserScriptMaster::ScriptReloader> {
   public:
    // Parses the includes out of |script| and returns them in |includes|.
    static void ParseMetadataHeader(const StringPiece& script,
                                    std::vector<std::string>* includes);

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
    //                     ParseMetadataHeader()
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
    // Scan the script directory for scripts, returning either a
    // new SharedMemory or NULL on error.
    base::SharedMemory* GetNewScripts(const FilePath& script_dir);

    // A pointer back to our master.
    // May be NULL if DisownMaster() is called.
    UserScriptMaster* master_;

    // The message loop to call our master back on.
    // Expected to always outlive us.
    MessageLoop* master_message_loop_;

    DISALLOW_COPY_AND_ASSIGN(ScriptReloader);
  };

  // DirectoryWatcher::Delegate implementation.
  virtual void OnDirectoryChanged(const FilePath& path);

  // Kicks off a process on the file thread to reload scripts from disk
  // into a new chunk of shared memory and notify renderers.
  void StartScan();

  // The directory containing user scripts.
  scoped_ptr<FilePath> user_script_dir_;

  // The watcher watches the profile's user scripts directory for new scripts.
  scoped_ptr<DirectoryWatcher> dir_watcher_;

  // The MessageLoop that the scanner worker runs on.
  // Typically the file thread; configurable for testing.
  MessageLoop* worker_loop_;

  // ScriptReloader (in another thread) reloads script off disk.
  // We hang on to our pointer to know if we've already got one running.
  scoped_refptr<ScriptReloader> script_reloader_;

  // Contains the scripts that were found the last time scripts were updated.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // If the script directory is modified while we're rescanning it, we note
  // that we're currently mid-scan and then start over again once the scan
  // finishes.  This boolean tracks whether another scan is pending.
  bool pending_scan_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptMaster);
};

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_
