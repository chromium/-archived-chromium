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

// BookmarkService handles reading/write the bookmark bar model. The
// BookmarkBarModel uses the BookmarkService to load bookmarks from
// disk, as well as notifying the BookmarkService every time the model
// changes.
//
// Internally BookmarkService uses BookmarkCoded to do the actual read/write.

// NOTE: This class is currently unsed. The plan is to move bookmarks
// out of the history db using this class and BookmarksCodec instead
// (bug 1256202).

#ifndef CHROME_BROWSER_BOOKMARK_STORAGE_H_
#define CHROME_BROWSER_BOOKMARK_STORAGE_H_

#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/browser_process.h"

class BookmarkBarModel;
class Profile;
class Value;

namespace {
class BookmarkStorageBackend;
}

class BookmarkStorage : public base::RefCountedThreadSafe<BookmarkStorage> {
  friend class BookmarkStorageBackend;

 public:
  // Creates a BookmarkStorage for the specified model
  BookmarkStorage(Profile* profile, BookmarkBarModel* model);

  // Loads the bookmarks into the model, notifying the model when done.
  void LoadBookmarks();

  // Schedules saving the bookmark bar model to disk.
  void ScheduleSave();

  // Notification the bookmark bar model is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void BookmarkModelDeleted();

 private:
  // Callback from backend with the results of the bookmark file.
  void LoadedBookmarks(Value* root_value);

  // Schedules a save on the backend thread.
  void SaveNow();

  // Returns the thread the backend is run on.
  Thread* backend_thread() { return g_browser_process->file_thread(); }

  // The model. The model is NULL once BookmarkModelDeleted has been invoked.
  BookmarkBarModel* model_;

  // Used to delay saves.
  ScopedRunnableMethodFactory<BookmarkStorage> save_factory_;

  // The backend handles actual reading/writing to disk.
  scoped_refptr<BookmarkStorageBackend> backend_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkStorage);
};

#endif  // CHROME_BROWSER_BOOKMARK_STORAGE_H_
