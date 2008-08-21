// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_BOOKMARK_STORAGE_H_
#define CHROME_BROWSER_BOOKMARK_STORAGE_H_

#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/browser_process.h"

class BookmarkBarModel;
class Profile;
class Value;

// BookmarkStorage handles reading/write the bookmark bar model. The
// BookmarkBarModel uses the BookmarkStorage to load bookmarks from
// disk, as well as notifying the BookmarkStorage every time the model
// changes.
//
// Internally BookmarkStorage uses BookmarkCodec to do the actual read/write.

class BookmarkStorage : public base::RefCountedThreadSafe<BookmarkStorage> {
  friend class BookmarkStorageBackend;

 public:
  // Creates a BookmarkStorage for the specified model
  BookmarkStorage(Profile* profile, BookmarkBarModel* model);

  // Loads the bookmarks into the model, notifying the model when done. If
  // load_from_history is true, the bookmarks are loaded from the file written
  // by history (StarredURLDatabase).
  void LoadBookmarks(bool load_from_history);

  // Schedules saving the bookmark bar model to disk.
  void ScheduleSave();

  // Notification the bookmark bar model is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void BookmarkModelDeleted();

 private:
  // Callback from backend with the results of the bookmark file.
  void LoadedBookmarks(Value* root_value,
                       bool bookmark_file_exists,
                       bool loaded_from_history);

  // Schedules a save on the backend thread.
  void SaveNow();

  // Returns the thread the backend is run on.
  Thread* backend_thread() const { return backend_thread_; }

  // The model. The model is NULL once BookmarkModelDeleted has been invoked.
  BookmarkBarModel* model_;

  // Used to delay saves.
  ScopedRunnableMethodFactory<BookmarkStorage> save_factory_;

  // The backend handles actual reading/writing to disk.
  scoped_refptr<BookmarkStorageBackend> backend_;

  // Thread read/writing is run on. This comes from the profile, and is null
  // during testing.
  Thread* backend_thread_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkStorage);
};

// Used to save the bookmarks on the file thread.
class BookmarkStorageBackend :
    public base::RefCountedThreadSafe<BookmarkStorageBackend> {
 public:
  explicit BookmarkStorageBackend(const std::wstring& path,
                                  const std::wstring& tmp_histor_path);

  // Writes the specified value to disk. This takes ownership of |value| and
  // deletes it when done.
  void Write(Value* value);

  // Reads the bookmarks from kBookmarksFileName. Notifies |service| with
  // the results on the specified MessageLoop.
  void Read(scoped_refptr<BookmarkStorage> service,
            MessageLoop* message_loop,
            bool load_from_history);

 private:
  // Path we read/write to.
  const std::wstring path_;

  // Path bookmarks are read from if asked to load from history file.
  const std::wstring tmp_history_path_;

  DISALLOW_EVIL_CONSTRUCTORS(BookmarkStorageBackend);
};

#endif  // CHROME_BROWSER_BOOKMARK_STORAGE_H_
